# Cube World Living Harvestables Mod

Bushes, shrubs, cobwebs, crafting plants and ore and gem deposits get a chance,
to come to life and turn hostile when you hit them, so you have to fight them
for the resources instead of just mining them.

This mod is for the **alpha** build of Cube World.

## Installation

The client version is only for *singleplayer* and the server version is only for
*multiplayer*. The client mod is not required in multiplayer, since all
creatures are simulated by the server. In fact, the client runs an internal
server for singleplayer as well. Both versions change the same behaviour, only
the locations where they patch the game are different.

**Client**

* requires [coremaze Cube-World-Mod-Launcher](https://github.com/coremaze/Cube-World-Mod-Launcher/releases/tag/v1.5)
* unpack Mod Launcher in the game directory
* put `LivingHarvestables.dll` into `Mods`
* start the game via `CubeModLauncher.exe`

**Server**

* requires [coremaze Cube-World-Server-Mod-Launcher](https://github.com/coremaze/Cube-World-Server-Mod-Launcher/releases/tag/prerelease2)
* unpack Mod Launcher in the game directory
* put `LivingHarvestablesServer.dll` into `Server_Mods`
* start server via `ServerModLauncher.exe`

## Compilation

Requires a compiler that targets 32-bit Windows. Due to the `asm` style, `MSVC`
will not work, use `GCC` or `Clang`. Run `make` in this directory and specify
the compiler as `CXX` variable if necessary (e.g.
`make CXX=i686-w64-mingw32-g++`). `make` builds both DLLs; name one of them to
build just that one.
