# GTA 2 for Nintendo 64 (tech demo port)

A work-in-progress port of the PC game **Grand Theft Auto 2** to the **Nintendo 64**, built on the
[`gta2_re`](https://github.com/CriminalRETeam/gta2_re) decompilation and [libdragon](https://github.com/DragonMinded/libdragon).
The original game logic runs on the console (**Expansion Pak / 8 MB RAM required**) on top of a custom
RDP renderer, audio, input and file layer. Tested in the [ares](https://ares-emu.net/) emulator.

> **This repository contains only the port's source code.**
> It contains **no** part of GTA 2: no game data, no ROM, no binaries. It does not redistribute the
> `gta2_re` sources either: `setup_engine.sh` downloads them from upstream at a pinned commit and applies
> our changes as a patch. **You must supply your own copy of the game data.**

Personal / educational project. Not affiliated with or endorsed by Rockstar Games or Take-Two Interactive.

## Project status: finished

This project is **finished**. The port runs as a playable tech demo on real hardware and in ares, and **no further
development, new features or support are planned**. The repository is kept as-is for reference and learning.
You are welcome to fork it and take it further.

## Important: the game is NOT included

This project is only the **port code**. **We do not provide, host, link or distribute GTA 2** in any form: no game data,
no ROM, no ISO, no binaries. To build the ROM you need **your own legally obtained copy of the PC version of GTA 2**
(Rockstar released the PC game as freeware in the past; getting and keeping a copy is your responsibility).
The ROM you build contains the game's data, so **do not share it**: it is for your personal use only.
Issues or pull requests asking for game files or ROMs will be closed.

## Step-by-step tutorial

1. **Install the tools** (see [Requirements](#requirements)): libdragon with its MIPS toolchain, plus `git`, `patch`, `make` and `python3`.
   On Windows, do everything inside **WSL** (Ubuntu is fine).
2. **Get the game data.** Install or extract your own copy of GTA 2 for PC and find its `data` folder
   (it contains files such as `e.gxt`, `fstyle.sty`, `bil.gmp`, `audio/` and `gtaudio/`).
   Copy that folder to `gta2_data/data`, **next to** (not inside) this repository:
   ```
   some-folder/
   ├── GTA-2-N64-Recomp/     <- this repository
   └── gta2_data/
       └── data/             <- the GTA 2 "data" folder goes here
   ```
   Any other location works too: pass `GAME_DATA=/path/to/data` to the build.
3. **Clone the repository** on a case-insensitive file system (on WSL: a Windows drive path such as `/mnt/c/...`):
   ```bash
   git clone https://github.com/Sunshinin11/GTA-2-N64-Recomp.git
   cd GTA-2-N64-Recomp
   ```
4. **Download and patch the engine** (once). This fetches `gta2_re` from upstream at a pinned commit and applies our patch:
   ```bash
   bash setup_engine.sh
   ```
5. **Build the ROM.** This converts the game data (endianness, audio) and produces `gta2_recomp.z64`:
   ```bash
   bash build.sh
   ```
   (Using a different libdragon location? `N64_INST=/path/to/libdragon bash build.sh`)
6. **Run it** in [ares](https://ares-emu.net/) (or another accurate emulator) with the **Expansion Pak enabled (8 MB)**.
   Hold **Z** while the ROM boots to skip the menu and go straight into a map.

If the build says a data file is missing, check that `GAME_DATA` points at the real GTA 2 `data` folder.
The first build takes several minutes.

## Requirements

* **Linux, macOS, or Windows through WSL.** The checkout must be on a **case-insensitive** file system, because the
  original sources use mixed-case `#include`s. On WSL that means building from a Windows drive path (`/mnt/c/...`);
  default macOS volumes are fine.
* [libdragon](https://github.com/DragonMinded/libdragon) (preview branch) with its MIPS toolchain, installed at
  `/opt/libdragon` (or set `N64_INST=/path`). The build uses `audioconv64` and `mkdfs` from it.
* `git`, `patch`, `make`, `python3` (standard library only).
* **The GTA 2 game data** (GTA 2 was released as freeware by Rockstar): the game's `data` folder, placed at
  `../gta2_data/data` (next to this repository) or passed as `GAME_DATA=/path/to/data`.

### Game data used by the build

`fstyle.sty`, `e.gxt`, `s.gxt`, the city maps (`bil`, `ste`, `wil`: `.gmp`, `.sty`, `.scr` and the mission folders), the bonus
levels, `nyc.gci`, `test1.seq`, `attract/`, `keyboard/`, `frontend/` (menu screens), `audio/` (sound banks) and `gtaudio/` (radio WAVs).
A missing file only prints a warning during the data step. `p.gxt` (an optional Portuguese text file) is not part of the original game;
without it the language option simply keeps the current language.

## Building on Windows (WSL) in one command

From PowerShell, with the repository at `C:\path\to\GTA-2-N64-Recomp`:

```powershell
wsl --cd "C:\path\to\GTA-2-N64-Recomp" bash -lc "bash setup_engine.sh && bash build.sh"
```

or open a WSL terminal and run the Quick start commands from `/mnt/c/path/to/GTA-2-N64-Recomp`.

## Build options

Pass them to `build.sh` or `make -f Makefile.recomp`:

| Option | Meaning |
|---|---|
| `GAME_DATA=/path/to/data` | Where the GTA 2 `data` folder is (default `../gta2_data/data`) |
| `MAPS="bil ste wil"` | Cities to include (fewer = smaller ROM) |
| `INCLUDE_FRONTEND=0` | Leave out the menu screens (smaller ROM; hold Z at boot to jump straight into the map) |
| `RADIO_RATE=12000` | Radio sample rate in Hz (`8000` = smaller) |
| `N64_INST=/path` | libdragon install location |
| `OVERSCAN_X=36` `OVERSCAN_Y=28` | Safe-area borders in VI dots (640x480 base) so TVs/upscalers do not crop the picture edges. `0` = full image (emulator) |

`bash build.sh clean` removes the build output; `bash setup_engine.sh --force` redoes the engine checkout.

## Repository layout

| Path | What it is |
|---|---|
| `port/n64/` | Console platform layer: renderer (`render_n64.c`), audio, input (DirectInput stand-in), Win32/CRT stubs, RAM file system, entry points |
| `port/compat/` | Small Win32 / DirectX / MSVC compatibility headers used to compile the decompiled engine with GCC |
| `port/game_srcs.txt` | Engine source files compiled into the ROM |
| `patches/gta2_re_n64.patch` | Our changes on top of `gta2_re` (pinned commit `02de8be`) |
| `tools/` | Build-time data converters (little- to big-endian for maps/styles/text, audio, shared SFX bank) |
| `Makefile.recomp`, `build.sh`, `setup_engine.sh` | Build system |

`port/gta2_re/` and `port/compat_ci/` are generated by `setup_engine.sh` and are git-ignored.

## Credits

* **DMA Design / Rockstar Games**: the original GTA 2.
* **CriminalRETeam / `gta2_re`**: the decompilation this port runs on.
* **M1sterMo / `gta2-browser`**: the browser port whose platform-abstraction approach and compatibility headers this work started from (MIT, see `THIRD_PARTY_NOTICES.md`).
* **libdragon**: N64 SDK, RDP/RSP/audio libraries.
