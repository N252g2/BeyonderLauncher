"""Builds the Beyonder .mrpack files and updates docs/pack/manifest.json.

Run by .github/workflows/pack.yml whenever pack/pack.json changes. Every file is downloaded and its
hash checked, so no hash is ever typed by hand.

  - The Beyonder mod jar and shader pack come from the newest (or a named) BeyonderMod release,
    which publishes release.json with their hashes.
  - Modrinth projects (Iris, plus its required dependencies such as Sodium) are resolved for the
    pack's Minecraft version and NeoForge.
"""
import hashlib
import json
import os
import sys
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ElementTree
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPEC = json.load(open(os.path.join(ROOT, "pack", "pack.json")))
OUT = os.path.join(ROOT, "docs", "pack")
SITE = "https://n252g2.github.io/BeyonderLauncher/pack"
UA = {"User-Agent": "N252g2/BeyonderLauncher pack builder (github.com/N252g2/BeyonderLauncher)"}


def get(url, headers=None, raw=False):
    h = dict(UA)
    h.update(headers or {})
    with urllib.request.urlopen(urllib.request.Request(url, headers=h), timeout=120) as r:
        data = r.read()
    return data if raw else json.loads(data)


def github(path):
    headers = {"Accept": "application/vnd.github+json"}
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        headers["Authorization"] = "Bearer " + token
    return get("https://api.github.com/" + path, headers)


def verified(url, sha1=None, sha512=None, size=None):
    data = get(url, raw=True)
    got1, got512 = hashlib.sha1(data).hexdigest(), hashlib.sha512(data).hexdigest()
    if (sha1 and got1 != sha1) or (sha512 and got512 != sha512) or (size and len(data) != size):
        sys.exit(f"Hash mismatch for {url}")
    return {"sha1": got1, "sha512": got512}, len(data)


def mod_release():
    repo = SPEC["modRepo"]
    if SPEC.get("modRelease", "latest") == "latest":
        release = github(f"repos/{repo}/releases?per_page=1")[0]
    else:
        release = github(f"repos/{repo}/releases/tags/{SPEC['modRelease']}")
    assets = {a["name"]: a["browser_download_url"] for a in release["assets"]}
    info = get(assets["release.json"])
    files = {}
    for kind, f in info["files"].items():
        url = assets[f["name"]]
        hashes, size = verified(url, f["sha1"], f["sha512"], f["size"])
        files[kind] = {"url": url, "name": f["name"], "hashes": hashes, "size": size}
    print(f"Mod release {release['tag_name']}: " + ", ".join(f["name"] for f in files.values()))
    return release["tag_name"], files


def modrinth_version(project=None, version_id=None):
    if version_id:
        return get(f"https://api.modrinth.com/v2/version/{version_id}")
    query = urllib.parse.urlencode({
        "game_versions": json.dumps([SPEC["minecraft"]]),
        "loaders": json.dumps(["neoforge"]),
    })
    versions = get(f"https://api.modrinth.com/v2/project/{project}/version?{query}")
    if not versions:
        sys.exit(f"No {project} build for Minecraft {SPEC['minecraft']} on NeoForge")
    return versions[0]  # newest first


def modrinth_files(projects):
    """Resolves projects and their required dependencies to verified client-side files."""
    resolved, queue = {}, [(p, None) for p in projects]
    while queue:
        project, version_id = queue.pop(0)
        v = modrinth_version(project, version_id)
        if v["project_id"] in resolved:
            continue
        f = next((f for f in v["files"] if f.get("primary")), v["files"][0])
        hashes, size = verified(f["url"], f["hashes"]["sha1"], f["hashes"]["sha512"], f["size"])
        resolved[v["project_id"]] = {"url": f["url"], "name": f["filename"], "hashes": hashes, "size": size,
                                     "server": project in SPEC.get("serverMods", [])}
        print(f"Modrinth {v['project_id']}: {f['filename']}")
        for dep in v.get("dependencies", []):
            if dep.get("dependency_type") == "required" and dep.get("project_id") not in resolved:
                queue.append((dep.get("project_id"), dep.get("version_id")))
    return list(resolved.values())


def neoforge_version():
    """The pinned NeoForge version, or the newest stable one for this Minecraft version.

    Mods such as Sodium bundle Fabric API pieces that need a recent NeoForge, so "latest" is the default.
    """
    wanted = SPEC.get("neoforge", "latest")
    if wanted != "latest":
        return wanted
    mc = SPEC["minecraft"].split(".")
    prefix = f"{mc[1]}.{mc[2] if len(mc) > 2 else 0}."  # 1.21.1 -> 21.1.
    xml = get("https://maven.neoforged.net/releases/net/neoforged/neoforge/maven-metadata.xml", raw=True)
    released = {v.text for v in ElementTree.fromstring(xml).iter("version")
                if v.text.startswith(prefix) and "-" not in v.text}
    # Launchers install NeoForge through Prism's metadata, which lags the Maven a little.
    # Only pick a version both know about, or the install fails with "Could not download metadata".
    meta = get("https://meta.prismlauncher.org/v1/net.neoforged/index.json")
    in_launcher = {v["version"] for v in meta.get("versions", [])}
    versions = released & in_launcher
    if not versions:
        sys.exit(f"No NeoForge release for Minecraft {SPEC['minecraft']} in both Maven and Prism metadata")
    newest = max(versions, key=lambda v: [int(x) for x in v.split(".")])
    newest_maven = max(released, key=lambda v: [int(x) for x in v.split(".")])
    if newest != newest_maven:
        print(f"NeoForge {newest_maven} is out, but launchers do not list it yet")
    minimum = SPEC.get("neoforgeMin")
    if minimum and [int(x) for x in newest.split(".")] < [int(x) for x in minimum.split(".")]:
        sys.exit(f"Launchers only list NeoForge up to {newest}, but the pack needs {minimum}+. Try again later.")
    print(f"NeoForge {newest}")
    return newest


def entry(path, f, server):
    return {
        "path": path,
        "hashes": f["hashes"],
        "env": {"client": "required", "server": "required" if server else "unsupported"},
        "downloads": [f["url"]],
        "fileSize": f["size"],
    }


def build_edition(edition, mod_files, neoforge):
    files = [entry(f"mods/{mod_files['mod']['name']}", mod_files["mod"], True)]
    if edition.get("shaderpack"):
        files.append(entry("shaderpacks/BeyonderShaders.zip", mod_files["shaderpack"], False))
    for f in modrinth_files(edition.get("modrinth", [])):
        files.append(entry(f"mods/{f['name']}", f, f.get("server", False)))

    index = {
        "formatVersion": 1,
        "game": "minecraft",
        "versionId": SPEC["version"],
        "name": "Beyonder" + ("" if edition["id"] == "standard" else f" ({edition['name']})"),
        "summary": SPEC["summary"],
        "files": files,
        "dependencies": {"minecraft": SPEC["minecraft"], "neoforge": neoforge},
    }
    name = f"Beyonder-{SPEC['version']}-{edition['id']}.mrpack"
    with zipfile.ZipFile(os.path.join(OUT, name), "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("modrinth.index.json", json.dumps(index, indent=2))
        z.writestr("overrides/config/beyonder-pack.txt",
                   f"# Beyonder pack {SPEC['version']} ({edition['name']} edition)\n")
        if edition.get("shaderpack"):
            z.writestr("overrides/config/iris.properties",
                       "# Set by the Beyonder pack\nenableShaders=true\nshaderPack=BeyonderShaders.zip\n")
    print(f"Wrote {name} ({len(files)} files)")
    return name


def main():
    tag, mod_files = mod_release()
    neoforge = neoforge_version()
    editions = []
    for edition in SPEC["editions"]:
        name = build_edition(edition, mod_files, neoforge)
        editions.append({
            "id": edition["id"],
            "name": edition["name"],
            "description": edition["description"],
            "mrpackUrl": f"{SITE}/{name}",
            "recommendedMemoryMB": edition["recommendedMemoryMB"],
        })

    path = os.path.join(OUT, "manifest.json")
    manifest = json.load(open(path))
    manifest["packVersion"] = SPEC["version"]
    manifest["mrpackUrl"] = editions[0]["mrpackUrl"]
    manifest["recommendedMemoryMB"] = editions[0]["recommendedMemoryMB"]
    manifest["editions"] = editions
    manifest["modBuild"] = tag
    with open(path, "w") as f:
        json.dump(manifest, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print(f"Manifest: pack {SPEC['version']} with mod {tag} on NeoForge {neoforge}")


if __name__ == "__main__":
    main()
