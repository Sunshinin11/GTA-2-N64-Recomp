# GTA 2 for Nintendo 64

An early playable native Nintendo 64 port of **Grand Theft Auto 2**, built from the reverse-engineered PC engine.

The game is already playable, including in-car radio playback. This repository records the current work in progress and provides the source for further development. Performance optimization and additional gameplay systems are the next priorities.

> **Current status:** one district is available. Missions and gang systems are not enabled yet. This is an experimental build, with incomplete features and remaining bugs.

## What works

- Playable exploration of one district.
- On-foot movement and vehicle driving.
- In-car radio playback and station switching.
- Nintendo 64 controller input.
- A native renderer using libdragon's RDPQ API.
- A build pipeline that converts PC game data for the N64.

## How the port works

The port compiles the recovered GTA 2 PC engine for the N64 and supplies a platform layer for graphics, input, timing, files, and audio.

The engine's draw commands are rendered through the N64's RDP. Textures are converted and cached for the console's memory and texture limits. Radio tracks are converted to compressed audio and streamed through libdragon's mixer.

The implementation builds on [CriminalRETeam's gta2_re](https://github.com/CriminalRETeam/gta2_re) and work from [M1sterMo's gta2-browser](https://github.com/M1sterMo/gta2-browser). The project name uses “Recomp”; the current approach is a source port of the recovered PC engine, with an N64-specific platform layer.

## Current limitations and roadmap

- **Performance:** playable, but frame rate and frame pacing still need improvement. There is no fixed frame-rate guarantee.
- **World coverage:** only one district is currently included.
- **Missions:** not enabled yet.
- **Gangs:** not enabled yet.
- **Compatibility and stability:** broader testing and bug fixes are ongoing.
- **Rendering and memory:** further optimization of texture handling, draw submission, and memory use is planned.

Please report problems with the build or commit you tested, your emulator or hardware configuration, and steps to reproduce the issue.

## Controls

The current input mapping is defined in [`port/n64/main_n64.c`](port/n64/main_n64.c).

| N64 control | Action |
| --- | --- |
| D-Pad / Control Stick | Move or steer; navigate menus |
| A | Enter / exit a vehicle; confirm in menus |
| B | Attack |
| Z | Jump / handbrake |
| Start | Escape / menu |
| C-Left / C-Right | Previous / next weapon |
| C-Down | Change radio station |
| L | Toggle texture filtering (render diagnostic) |
| R | Toggle perspective correction (render diagnostic) |
| C-Up | Cycle render debug modes |

The steering and acceleration behavior follows the engine's directional input mapping. The L, R, and C-Up toggles are development tools and can change the appearance of the scene.

## Running

To run the game, you must compile `gta2_recomp.z64` locally using your own copy of the GTA 2 PC game data (see [Building from source](#building-from-source)). **Precompiled ROMs are not distributed in this repository.**

Once built, use an N64 emulator with accurate homebrew support, such as [ares](https://ares-emu.net/), with **8 MB of RAM / Expansion Pak enabled**, or test on real hardware using a flash cartridge.

The development target is an N64 with Expansion Pak. Compatibility on a particular flash cartridge or real console should be confirmed by testing; this README does not claim hardware validation.

## Building from source

The N64 build uses the **libdragon toolchain**, GNU Make, Python 3, and Bash. On Windows, use WSL. The checked-in convenience scripts contain paths from the development machine; for another setup, invoke the Makefile directly from the repository root.

1. Install a compatible [libdragon toolchain and SDK](https://libdragon.dev/). This build also uses `audioconv64` with ULC audio compression support.
2. Supply the PC game data from your own GTA 2 installation (freeware release). `GAME_DATA` must point to its `data` directory.
3. From Bash or WSL, run:

```bash
export N64_INST=/opt/libdragon
export PATH="$N64_INST/bin:$PATH"

make -f Makefile.recomp \
  GAME_DATA="/absolute/path/to/GTA2/data" \
  -j4 all
```

Adjust `N64_INST` to your toolchain installation. The build stages the selected data files into DragonFS and converts supported data to the byte order and audio formats used by the port. The required entries are listed in [`Makefile.recomp`](Makefile.recomp).

The generated ROM will be output as `gta2_recomp.z64` in the repository root.

## Repository layout

| Path | Contents |
| --- | --- |
| `port/gta2_re/` | Recovered PC engine and port modifications |
| `port/n64/` | N64 entry point, graphics, audio, input, and platform support |
| `port/compat/` and `port/compat_ci/` | Compatibility headers |
| `tools/` | Data conversion and staging utilities |
| `Makefile.recomp` | Engine and N64 ROM build |
| `docs/` | Technical reports and project documentation |

## Credits

- **DMA Design / Rockstar Games** — the original Grand Theft Auto 2.
- **[CriminalRETeam and gta2_re contributors](https://github.com/CriminalRETeam/gta2_re)** — reverse engineering and recovery of the PC engine.
- **[M1sterMo and gta2-browser contributors](https://github.com/M1sterMo/gta2-browser)** — browser port work and portability adaptations used as a foundation.
- **[libdragon contributors](https://libdragon.dev/)** — the N64 SDK, toolchain, graphics, and audio libraries.
- **[Sunshinin11](https://github.com/Sunshinin11)** — N64 port development and integration.

This is an unofficial fan project. GTA 2 and its original assets belong to their respective owners. Retain upstream attribution and consult the notices accompanying each component; code licensing does not grant rights to the original game assets.
