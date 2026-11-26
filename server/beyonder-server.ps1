# Beyonder local test server (Windows, PowerShell 5.1+).
#
# Every start:
#   1. finds Java 21+ (or offers to install it with winget)
#   2. asks once for the Minecraft EULA
#   3. syncs the Beyonder pack's server-side files from the launcher manifest (same pack players install)
#   4. installs the NeoForge version the pack needs, if it isn't installed yet
#   5. starts the server
#
# Options:  start.bat -Edition lite      use another edition's pack
#           start.bat -SkipSync          start without checking for pack updates (offline)
#           start.bat -Memory 6G         server memory (default 4G)

param(
    [string]$Manifest = "https://n252g2.github.io/BeyonderLauncher/pack/manifest.json",
    [string]$Edition = "standard",
    [string]$Memory = "4G",
    [switch]$SkipSync
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"   # Invoke-WebRequest is very slow with the progress bar on
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root
$StateFile = Join-Path $Root ".beyonder-state.json"

function Say($text, $color = "Gray") { Write-Host $text -ForegroundColor $color }

function Read-State {
    if (Test-Path $StateFile) { return Get-Content $StateFile -Raw | ConvertFrom-Json }
    return [pscustomobject]@{ packVersion = ""; edition = ""; neoforge = ""; minecraft = ""; files = @() }
}

function Save-State($state) { $state | ConvertTo-Json -Depth 5 | Set-Content $StateFile -Encoding UTF8 }

# ---------------------------------------------------------------------------------------------------------------- Java

function Get-JavaMajor([string]$java) {
    # java -version prints to stderr; in Windows PowerShell 5.1 that throws when ErrorActionPreference is Stop.
    $ErrorActionPreference = "Continue"
    try {
        $out = & $java -version 2>&1 | Out-String
        if ($out -match 'version "(\d+)(\.(\d+))?') {
            $major = [int]$Matches[1]
            if ($major -eq 1 -and $Matches[3]) { $major = [int]$Matches[3] }
            return $major
        }
    } catch { }
    return 0
}

function Find-Java {
    $candidates = @()
    if ($env:JAVA_HOME) { $candidates += Join-Path $env:JAVA_HOME "bin\java.exe" }
    $cmd = Get-Command java -ErrorAction SilentlyContinue
    if ($cmd) { $candidates += $cmd.Source }
    # Java that Beyonder Launcher downloaded for the game
    $launcherJava = Join-Path $env:APPDATA "BeyonderLauncher\java"
    if (Test-Path $launcherJava) {
        $candidates += Get-ChildItem $launcherJava -Recurse -Depth 4 -Filter java.exe -ErrorAction SilentlyContinue | ForEach-Object FullName
    }
    # Common installs
    $candidates += Get-ChildItem "$env:ProgramFiles\Eclipse Adoptium\*\bin\java.exe", "$env:ProgramFiles\Java\*\bin\java.exe", "$env:ProgramFiles\Microsoft\jdk-*\bin\java.exe" -ErrorAction SilentlyContinue | ForEach-Object FullName
    foreach ($c in $candidates | Select-Object -Unique) {
        if ((Test-Path $c) -and (Get-JavaMajor $c) -ge 21) { return $c }
    }
    return $null
}

$Java = Find-Java
if (-not $Java) {
    Say "Java 21 or newer wasn't found. The server needs it." Yellow
    if (Get-Command winget -ErrorAction SilentlyContinue) {
        $answer = Read-Host "Install Eclipse Temurin 21 with winget now? (Y/N)"
        if ($answer -match '^[Yy]') {
            winget install --id EclipseAdoptium.Temurin.21.JRE --exact --accept-source-agreements --accept-package-agreements
            $Java = Find-Java
        }
    }
    if (-not $Java) {
        Say "Install Java 21 from https://adoptium.net/temurin/releases/?version=21 and run start.bat again." Red
        exit 1
    }
}
Say "Java: $Java" DarkGray

# ---------------------------------------------------------------------------------------------------------------- EULA

$eulaFile = Join-Path $Root "eula.txt"
if (-not ((Test-Path $eulaFile) -and ((Get-Content $eulaFile -Raw) -match 'eula\s*=\s*true'))) {
    Say ""
    Say "Running a Minecraft server requires accepting the Minecraft EULA:" Cyan
    Say "  https://aka.ms/MinecraftEULA" Cyan
    $answer = Read-Host "Do you accept it? (Y/N)"
    if ($answer -notmatch '^[Yy]') { Say "The server can't start without accepting the EULA." Red; exit 1 }
    "# Accepted $(Get-Date -Format s) via start.bat`neula=true" | Set-Content $eulaFile -Encoding ASCII
}

# ------------------------------------------------------------------------------------------------- Pack sync (manifest)

$state = Read-State

function Test-SafeRelativePath([string]$p) {
    # .mrpack paths must stay inside the server folder
    if ([string]::IsNullOrWhiteSpace($p)) { return $false }
    if ($p -match '(^|[\\/])\.\.([\\/]|$)' -or $p -match '^[\\/]' -or $p -match '^[A-Za-z]:') { return $false }
    return $true
}

function Sync-Pack {
    Say "Checking the Beyonder pack..." Cyan
    $m = Invoke-RestMethod -Uri $Manifest -UseBasicParsing
    $packUrl = $m.mrpackUrl
    if ($m.editions) {
        $e = $m.editions | Where-Object { $_.id -eq $Edition } | Select-Object -First 1
        if (-not $e) { $e = $m.editions | Select-Object -First 1 }
        $packUrl = $e.mrpackUrl
    }

    $tmp = Join-Path ([IO.Path]::GetTempPath()) ("beyonder-pack-" + [Guid]::NewGuid())
    New-Item -ItemType Directory -Path $tmp | Out-Null
    try {
        $zip = Join-Path $tmp "pack.zip"
        Invoke-WebRequest -Uri $packUrl -OutFile $zip -UseBasicParsing
        Expand-Archive -Path $zip -DestinationPath (Join-Path $tmp "x") -Force
        $index = Get-Content (Join-Path $tmp "x\modrinth.index.json") -Raw | ConvertFrom-Json

        $wanted = @()
        foreach ($f in $index.files) {
            if ($f.env -and $f.env.server -eq "unsupported") { continue }   # client-only (shaders, minimaps...)
            if (-not (Test-SafeRelativePath $f.path)) { Say "Skipping unsafe path in pack: $($f.path)" Yellow; continue }
            $target = Join-Path $Root $f.path
            $wanted += $f.path
            $have = $false
            if (Test-Path $target) {
                $have = ((Get-FileHash $target -Algorithm SHA1).Hash.ToLower() -eq $f.hashes.sha1.ToLower())
            }
            if (-not $have) {
                Say "  downloading $($f.path)"
                New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
                $ok = $false
                foreach ($url in $f.downloads) {
                    try {
                        Invoke-WebRequest -Uri $url -OutFile $target -UseBasicParsing
                        if ((Get-FileHash $target -Algorithm SHA1).Hash.ToLower() -eq $f.hashes.sha1.ToLower()) { $ok = $true; break }
                        Say "  checksum mismatch from $url" Yellow
                    } catch { Say "  failed from $url : $($_.Exception.Message)" Yellow }
                }
                if (-not $ok) { Remove-Item $target -ErrorAction SilentlyContinue; throw "Couldn't download $($f.path)" }
            }
        }

        # Remove files an earlier pack version installed that this one doesn't list
        foreach ($old in @($state.files)) {
            if ($old -and ($wanted -notcontains $old) -and (Test-SafeRelativePath $old)) {
                $p = Join-Path $Root $old
                if (Test-Path $p) { Say "  removing $old (no longer in the pack)"; Remove-Item $p -Force }
            }
        }

        # Configs and other files shipped in the pack (server-overrides win over overrides)
        foreach ($dir in "overrides", "server-overrides") {
            $src = Join-Path $tmp "x\$dir"
            if (-not (Test-Path $src)) { continue }
            $srcFull = (Resolve-Path $src).Path.TrimEnd('\') + '\'
            foreach ($file in Get-ChildItem $src -Recurse -File) {
                $rel = $file.FullName.Substring($srcFull.Length)
                if (-not (Test-SafeRelativePath $rel)) { continue }
                $dest = Join-Path $Root $rel
                New-Item -ItemType Directory -Path (Split-Path $dest) -Force | Out-Null
                Copy-Item -LiteralPath $file.FullName -Destination $dest -Force
            }
        }

        $state.packVersion = $index.versionId
        $state.edition = $Edition
        $state.files = $wanted
        $state.minecraft = $index.dependencies.minecraft
        $state | Add-Member -NotePropertyName requiredNeoforge -NotePropertyValue $index.dependencies.neoforge -Force
        $state | Add-Member -NotePropertyName server -NotePropertyValue $m.server -Force
        Save-State $state
        Say "Beyonder pack $($index.versionId) ($Edition): $($wanted.Count) server file(s) ready." Green
    } finally {
        Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue
    }
}

if ($SkipSync) {
    if (-not $state.packVersion) { Say "No pack synced yet; run without -SkipSync once." Red; exit 1 }
    Say "Skipping pack sync (using Beyonder pack $($state.packVersion))." Yellow
} else {
    try { Sync-Pack; $state = Read-State }
    catch {
        if ($state.packVersion) { Say "Pack sync failed ($($_.Exception.Message)); starting with the last synced pack $($state.packVersion)." Yellow }
        else { Say "Pack sync failed: $($_.Exception.Message)" Red; exit 1 }
    }
}

# ---------------------------------------------------------------------------------------------------------------- NeoForge

$neoforge = $state.requiredNeoforge
if (-not $neoforge) { Say "The pack doesn't say which NeoForge version it needs." Red; exit 1 }
$argsFile = Join-Path $Root "libraries\net\neoforged\neoforge\$neoforge\win_args.txt"
if (-not (Test-Path $argsFile)) {
    Say "Installing NeoForge $neoforge (first start or pack update)..." Cyan
    $installer = Join-Path $Root "neoforge-$neoforge-installer.jar"
    Invoke-WebRequest -Uri "https://maven.neoforged.net/releases/net/neoforged/neoforge/$neoforge/neoforge-$neoforge-installer.jar" -OutFile $installer -UseBasicParsing
    & $Java -jar $installer --installServer $Root
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $argsFile)) { Say "NeoForge installation failed." Red; exit 1 }
    Remove-Item $installer, "$installer.log" -ErrorAction SilentlyContinue
    $state.neoforge = $neoforge
    Save-State $state
}

# ---------------------------------------------------------------------------------------------------------------- Config

$props = Join-Path $Root "server.properties"
if (-not (Test-Path $props)) {
    @"
# Beyonder local test server
motd=\u00a7cBeyonder\u00a7r \u00a77- Tingen (local test)
server-port=25565
online-mode=true
enforce-secure-profile=true
max-players=20
view-distance=10
simulation-distance=8
spawn-protection=0
white-list=false
pvp=true
difficulty=normal
"@ | Set-Content $props -Encoding ASCII
}
@"
# Edited by start.bat: use -Memory to change. Add other JVM arguments below.
-Xms1G
-Xmx$Memory
"@ | Set-Content (Join-Path $Root "user_jvm_args.txt") -Encoding ASCII

# ---------------------------------------------------------------------------------------------------------------- Run

Say ""
Say "Starting Beyonder server (pack $($state.packVersion), NeoForge $neoforge, $Memory RAM)." Green
Say "Join from Beyonder Launcher with Play, or in Minecraft at: localhost" Green
Say "Type 'stop' here to shut it down." DarkGray
Say ""
& $Java "@user_jvm_args.txt" "@libraries/net/neoforged/neoforge/$neoforge/win_args.txt" nogui
exit $LASTEXITCODE
