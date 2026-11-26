<p align="center">
  <img src="program_info/io.github.n252g2.BeyonderLauncher.svg" alt="Beyonder Launcher logo" width="128"/>
</p>

<h1 align="center">Beyonder Launcher</h1>

<p align="center">
  The launcher for <b>Beyonder</b>, a Minecraft MMORPG inspired by <i>Lord of the Mysteries</i>.
  <br />
  Sign in, press Play: it installs and updates the modpack and connects you to the server.
</p>

> **Beyonder Launcher is a fork of [Prism Launcher](https://github.com/PrismLauncher/PrismLauncher).
> It is not Prism Launcher and is not endorsed by or affiliated with the Prism Launcher project (<https://prismlauncher.org>).**
> Please report problems with this launcher on this repository's issue tracker, not to the Prism Launcher team.
>
> Beyonder is an unofficial fan project and is not affiliated with the author or publishers of *Lord of the Mysteries*.

## What's different from Prism Launcher

- Rebranded: name, app ID, icons, installer, URL scheme (`beyonderlauncher://`).
- Prism Launcher's API keys are removed. The Microsoft client ID is supplied at build time (see below);
  CurseForge and Imgur integrations are disabled.
- The installer no longer claims `modrinth://` / `curseforge://` links, so it can sit next to Prism or the Modrinth App.
- CI builds Linux x64, Windows x64 (MSVC) and macOS only.
- Single-game mode (in progress): one managed Beyonder instance, auto-updated from a hosted modpack, launched straight into the server.

## Building

Builds run on GitHub Actions on every push (Actions tab → latest run → Artifacts). Tagging a commit (`git tag v0.1.0 && git push --tags`) creates a draft release with installers.

Local builds follow Prism's instructions: <https://prismlauncher.org/wiki/development/build-instructions/>.

### Microsoft sign-in

Microsoft login needs your **own** Azure application, and Mojang has to allow-list it for the Minecraft API:

1. Register an app in the Azure portal (Microsoft Entra ID → App registrations). Supported account types: *Personal Microsoft accounts only*. Under Authentication, enable *Allow public client flows*.
2. Apply for Minecraft API access for that app ID using the form linked from Minecraft's help article on Java Edition game service API access. Until it's approved, sign-in fails with a 403.
3. The app ID is set as `Launcher_MSA_CLIENT_ID` in `CMakeLists.txt`. It's a public identifier, not a secret. To override it, add a repository secret named `MSA_CLIENT_ID`, or pass `-DLauncher_MSA_CLIENT_ID=<id>` to a local build.

### Pack manifest (single-game mode)

On start-up the launcher downloads `Beyonder_MANIFEST_URL` (default: `docs/pack/manifest.json` served by GitHub Pages) and compares `packVersion` with the installed Beyonder instance:

```json
{
  "formatVersion": 1,
  "packVersion": "0.1.0",
  "mrpackUrl": "https://github.com/<you>/BeyonderPack/releases/download/v0.1.0/Beyonder-0.1.0.mrpack",
  "server": "play.example.net",
  "recommendedMemoryMB": 6144,
  "minLauncherVersion": "12.0.0",
  "motd": "<h2>Season 0: Tingen</h2><p>Shown on the home screen.</p>"
}
```

- `mrpackUrl`: a Modrinth-format modpack (`.mrpack`). Updates are applied in place; unchanged files aren't downloaded again.
- `server`: joined automatically when the player presses Play. Leave it empty to just open the title screen.
- `recommendedMemoryMB`: applied once, the first time the pack is installed. After that, the player's own setting wins.
- To release an update: upload the new `.mrpack`, then change `packVersion` and `mrpackUrl` and push.

Setting `BeyonderClassicView=true` in the launcher config, or building with `-DBeyonder_MANIFEST_URL=""`, gives you the normal multi-instance launcher. The **Advanced** button on the home screen also opens it.

### Fork settings

Repository and website URLs are set in one place near the top of `CMakeLists.txt` (`Beyonder_GIT_URL`, `Beyonder_SITE_URL`).

## License

All launcher code is available under the GPL-3.0-only license (see [LICENSE](LICENSE) and [COPYING.md](COPYING.md)), as inherited from Prism Launcher, PolyMC and MultiMC. If you distribute builds, you must also make the corresponding source available.

The Beyonder Launcher icon is original artwork for this project.
