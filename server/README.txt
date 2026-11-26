BEYONDER LOCAL TEST SERVER
==========================

Put this folder anywhere with a few GB free (for example C:\BeyonderServer) and
double-click start.bat.

First start
-----------
1. If Java 21 isn't installed, it offers to install it (Eclipse Temurin, via winget).
2. It asks you to accept the Minecraft EULA (https://aka.ms/MinecraftEULA).
3. It downloads the Beyonder pack's server files and the NeoForge version the pack needs.
4. The server starts. Windows may ask whether Java can use the network:
   "Private networks" is enough for playing on this PC.

Every start
-----------
It re-reads the launcher's pack manifest, so the server always runs the same pack
players install. New mods are downloaded, removed ones are deleted, and pack config
files are copied in.

Playing
-------
In Beyonder Launcher press Play: it joins "localhost" (this PC) automatically.
In plain Minecraft, add the server address: localhost

Stopping
--------
Type  stop  in the server window and press Enter. Don't just close the window: the
world may not save.

Options (run from a Command Prompt in this folder)
--------------------------------------------------
  start.bat -Memory 6G        give the server 6 GB instead of 4 GB
  start.bat -Edition lite     use the Lite edition's pack
  start.bat -SkipSync         start without checking for pack updates (offline)

Testing your own mod build
--------------------------
Drop the mod's .jar into the mods folder and start the server. Files that aren't
part of the pack are left alone. Put the same jar in the game's mods folder
(Launcher > Library > folder button > mods).

Files
-----
  world\                      the world (back this up before big changes)
  server.properties           server settings (created on first start, then yours to edit)
  .beyonder-state.json        which pack version and files start.bat installed
