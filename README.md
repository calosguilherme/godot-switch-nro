# godot-switch-nro

**Take a Godot 4.7.2 game to Nintendo Switch homebrew.** Point it at your project folder and get an SD
card folder, ready to open from the hbmenu on Atmosphère:

```text
python gsn.py gerar ../my-game
  → saida/my-game/switch/my-game/my-game.nro      (OpenGL, listed in the hbmenu)
                                 my-game.vulkan   (Vulkan, the default; the .nro hands over to it)
                                 my-game.pck      (the game)
```

[Português](README.pt-BR.md) · [Decisions](docs/decisoes.md) · [Tests](docs/testes.md) ·
[Pending work](docs/pendencias.md) (the docs and code comments are in Brazilian Portuguese)

## What it is for

There is no Godot 4 for Switch homebrew, official or not: the last known port stopped at Godot 3.5. This
repository is a `switch` platform for Godot 4.7.2, written for the game
[Batida Capixaba](#where-it-came-from) and split from it so any project can use it.

- **Homebrew only** (`.nro` on Atmosphère, with the hbmenu). Not for stores or unmodified consoles.
- **GDScript projects**, 2D or 3D. It is the whole engine; what is left out is under
  [Limits](#limits).
- **Two video backends:** Vulkan (Mesa 26 NVK, Mobile renderer: the same look as on PC and phones; the
  default) and OpenGL ES 3.2 (Mesa 20.1, Compatibility renderer: the fallback).

## How it works

```text
          once                                  for every game version
 ┌────────────────────────────┐        ┌──────────────────────────────────────────┐
 │ gsn.py preparar            │        │ gsn.py gerar <project>                   │
 │  Godot 4.7.2 + patches     │        │  1. reads project.godot (+ config)       │
 │  devkitPro Docker image    │        │  2. exports the .pck (Godot editor)      │
 │  NVK from mesa-switch      │        │  3. 256×256 icon and 1280×720 splash     │
 └─────────────┬──────────────┘        │  4. builds the engine (only what changed)│
               ▼                       │  5. elf2nro: engine + NACP + romfs       │
 ┌────────────────────────────┐        │  6. lays out the SD card folder          │
 │ gsn.py motor               │──ELF──▶└──────────────────────────────────────────┘
 │  godot-opengl.elf          │
 │  godot-vulkan.elf          │
 └────────────────────────────┘
```

- **The engine** (`platform/switch/`) is built once per backend and knows nothing about the game. File
  names come from the executable itself: `my-game.nro` looks for `my-game.pck` and `my-game.vulkan` in
  the same folder.
- **Packaging** (`gsn.py` + `scripts/empacotar.sh`) puts the title, author, version and icon in the
  `.nro` (NACP), and in its romfs the splash image (`abertura.rgba`), the version (`versao.txt`) and,
  optionally, the `.pck` itself (`jogo.pck`).
- **At startup**, `my-game.nro` reads `dados/renderizador.txt`. With no file or with `vulkan`, it hands
  over to `my-game.vulkan` through the homebrew loader. If Vulkan does not reach 300 frames (crashed,
  hung or was closed), the next start falls back to OpenGL on its own.
- Build steps run in Docker (the official devkitPro image): through WSL Ubuntu on Windows, directly on
  Linux.

## Step by step

### 1. Install (once)

| What | Why |
| --- | --- |
| **Windows 10/11 with WSL and Ubuntu** (or Linux) | where the engine is built |
| **Docker** in Ubuntu: `sudo apt install docker.io` | the devkitPro image; on WSL the scripts run as root and start `dockerd` themselves |
| **Python 3** with **Pillow** (`python -m pip install pillow`) where you run `gsn.py` | icon and splash |
| **The Godot 4.7.2 editor** (same version!) | exporting the `.pck` |
| **Git** | fetching Godot and mesa-switch |
| ~25 GB free in Linux/WSL | Godot sources (two trees), mesa-switch and the Docker images |

On Windows, run the commands from PowerShell or Git Bash in this folder. The default WSL distro is
`Ubuntu` (override with `GSN_WSL`). The heavy work lives in `~/godot-switch-nro-trabalho` inside Linux
(override with `GSN_TRABALHO`).

### 2. Prepare (once, ~1 h)

```sh
python gsn.py preparar
```

Fetches Godot 4.7.2, applies `patches/godot/`, builds the Docker image and builds the NVK from
[mesa-switch](https://github.com/danfromtico/mesa-switch) at the tested commit (the slow part: 30–60
min and a ~7 GB image). Without Vulkan: `python gsn.py preparar --sem-vulkan` (OpenGL `.nro` only).

### 3. Generate the game

```sh
python gsn.py info ../my-game      # check what will come out (title, icon, splash, preset...)
python gsn.py gerar ../my-game --godot "C:/path/Godot_v4.7.2-stable_win64_console.exe"
```

The first time the engine builds from scratch (~9 min per backend); after that, only what changed.
Output: `saida/my-game/switch/my-game/`. Options: `--sem-vulkan`, `--sem-compilar` (reuse the built
engine), `--saida <folder>`, `--config <file>`.

### 4. Put it on the Switch

1. Copy `saida/my-game/switch/` to the SD card root (you get `sdmc:/switch/my-game/`). **Always replace
   all files together**: a `.nro`, `.vulkan` and `.pck` from different versions do not match.
2. Open the hbmenu **holding R while launching a game** (title mode; from the album, homebrew only gets
   ~400 MB).
3. The first Vulkan start shows a black screen while shaders compile (minutes for a large 3D game). Do
   not close it: from the second start on it is fast.

### 5. Test and collect the log

Next to the `.nro` you get `my-game.log` (or `my-game.vulkan.log`; the previous run's log is kept as
`my-game.anterior.log` / `my-game.vulkan.anterior.log`), `my-game.saida.txt` (the exit
check: "tudo solto" means clean) and, on Vulkan, `vulkan_etapas.txt`. If the console shows the error
screen, the report is in `sdmc:/atmosphere/crash_reports/`. With `nxlink -s my-game.nro` on the PC
(same network, console on the hbmenu) the log shows in the terminal.

## Game configuration

With no configuration at all, everything comes from `project.godot`:

| Key (`[jogo]`) | Default | What |
| --- | --- | --- |
| `titulo` | `application/config/name` | name in the hbmenu |
| `autor` | `Godot Engine (homebrew)` | author in the hbmenu |
| `arquivo` | the title, lowercase ASCII with hyphens | file and SD folder name |
| `versao` | `application/config/version` or `1.0.0` | version (NACP, `romfs:/versao.txt`) |
| `versao_arquivo` | — | a file holding the version (wins over `versao`) |
| `subir_versao` | `não` | `sim`: bump the last number on every `gerar` (written back to `versao_arquivo`) |
| `escrever_versao_em` | — | e.g. `res://versao.txt`: writes the version into the project before export, so the game can show it |
| `icone` | `application/config/icon` | PNG/JPG; becomes a 256×256 JPG |
| `abertura` | `boot_splash/image.switch` or `boot_splash/image` | drawn before Godot starts; fit inside 1280×720 |
| `cor_de_fundo` | `boot_splash/bg_color(.switch)` | `#rrggbb`; splash and icon background |
| `preset` | `Switch` | export preset; if missing, a temporary Linux arm64 one is used (`export_presets.cfg` is restored) |
| `vulkan` | `sim` | `não`: OpenGL `.nro` only |
| `embutir_pck` | `não` | `sim`: the `.pck` goes inside the `.nro` (a single file on the SD) |
| `godot` | `GODOT` env var or `godot` on PATH | the Godot 4.7.2 editor |
| `projeto_nome` | — | only in `jogos/*/jogo.cfg`: the project's `application/config/name` it applies to |

The config is looked up in: `--config`; `godot-switch-nro.cfg` at the project root;
`jogos/<any>/jogo.cfg` in this repository whose `projeto_nome` matches the project name (so the game
needs nothing Switch-specific inside it). Relative paths are relative to the config file; `res://` is
the project folder. Example:

```ini
[jogo]
projeto_nome = My Game
titulo = My Game
autor = My Studio
arquivo = mygame
versao_arquivo = VERSAO
subir_versao = sim
icone = icon.jpg
```

## For the game

The game runs unchanged. To make use of the console:

- **`OS.has_feature("switch")`** is true on the Switch (also `nintendo_switch`). Recommendation: keep
  anything that **lowers** graphics behind that check; PC and phones stay as they are.
- **`user://`** lives in `sdmc:/switch/<game>/dados/`.
- **The `Switch` singleton** (exists only on the Switch; method names are Portuguese):

  ```gdscript
  if Engine.has_singleton("Switch"):
      var switch := Engine.get_singleton("Switch")
      switch.definir_turbo(true)
  ```

  | Method | What |
  | --- | --- |
  | `versao()` | the game version in this `.nro` (`romfs:/versao.txt`) |
  | `versao_do_motor()` | the godot-switch-nro version |
  | `renderizador()` | `"opengl"` or `"vulkan"` |
  | `tem_vulkan()` | `<game>.vulkan` is in the folder |
  | `reabrir()` | asks the loader to open `<game>.nro` when the game exits (applies a video switch); quit right after (`get_tree().quit()`) |
  | `modelo()` | `"erista"` (2017) or `"mariko"` (V2, Lite, OLED) |
  | `na_base()`, `carregando()` | docked / charging |
  | `definir_turbo(bool)`, `estado_do_turbo()` | CPU at 1785 MHz ("desligado", "ligado" or "esperando": Erista only allows it docked or charging) |
  | `fonte_do_sistema(type)` | the shared system TTF bytes (0 standard/Japanese, 1 Simplified Chinese, 2 extended, 3 Traditional, 4 Korean, 5 Nintendo symbols) |

- **Switching video:** write `opengl` or `vulkan` to `user://renderizador.txt` and call `reabrir()`. If
  `user://vulkan_falhou` exists, the last Vulkan attempt never got running and the game fell back to
  OpenGL: tell the player and delete the file.
- **HOME** freezes the process without notice; on return the game gets
  `NOTIFICATION_APPLICATION_FOCUS_OUT` and `NOTIFICATION_APPLICATION_PAUSED`: pause there.
- **Performance:** the CPU is the bottleneck (~10–15× slower than a desktop for the same script) and
  each draw call is expensive in the driver. Batch (MultiMesh), avoid per-frame allocations, measure on
  the console.
- **Colors on OpenGL:** the Compatibility renderer treats `ALBEDO` as sRGB; custom shaders doing math
  in linear look darker on the OpenGL `.nro` (not on Vulkan).
- **Files next to the `.nro`** for diagnostics: `<game>.verbose` (Godot verbose mode), `mesa_env.txt`
  (`NAME=value` lines become driver environment variables), `render_separado` (render thread,
  experimental).

## Versions

| Component | Tested version |
| --- | --- |
| Godot | 4.7.2-stable (with `patches/godot/`) |
| devkitPro (`devkitpro/devkita64` image) | digest `sha256:1fc388c3a0d3…` |
| devkitA64 (GCC) | r29.2 (GCC 15.2.0) |
| libnx | 4.12.0 |
| switch-mesa (OpenGL) | 20.1.0 (Mesa 20.1.0-rc3, nouveau, GLES 3.2) |
| switch-tools (`elf2nro`, `nacptool`) | 1.13.1 |
| mesa-switch (Vulkan, NVK) | commit `0ff52079` (Mesa 26.2.4), Vulkan 1.4.354 |
| Rust (for the NVK's NAK) | 1.101.0-nightly (2026-10-18) |
| Meson | 1.12.1 |
| godot-switch-nro | see `VERSION` |

## Tests done

Summary (details in [docs/testes.md](docs/testes.md)): the engine ran a full 3D game (Batida Capixaba)
on a real console with both backends: 3D, synchronized audio stems, Joy-Con, touch, rumble, system
language and fonts, HOME, close from the system, OpenGL ↔ Vulkan switching with automatic fallback.
Vulkan gave ~25% more frames than OpenGL. This repository's generic packaging still awaits its first
console test.

## Limits

Networking is IPv4 UDP/TCP and ENet only (`PacketPeerUDP`, `ENetMultiplayerPeer`, DNS, `Crypto`), still
awaiting its first console test; no TLS/HTTPS certificates, WebSocket, WebRTC or UPnP
([D-18](docs/decisoes.md)). No C#, no GDExtension, no software keyboard, up to two controllers, some
modules off (CSG, Theora video, Jolt, complex text shaping), black screen on the very first Vulkan
start. List and reasons in [docs/pendencias.md](docs/pendencias.md).

## What is here

| Path | What |
| --- | --- |
| `gsn.py` | the tool: `preparar`, `motor`, `gerar`, `info` |
| `platform/switch/` | the Godot platform (C++): video (EGL and Vulkan), audio (audren), controllers, touch, the `Switch` singleton |
| `patches/godot/` | patches to the Godot sources |
| `patches/mesa-switch/` | the NVK build with Rust targeting the Switch's ARM |
| `scripts/` | bash steps (Docker): prepare, build the engine, package |
| `docker/Dockerfile` | devkitPro image + scons |
| `exemplo/` | a minimal project that proves the engine (`python gsn.py gerar exemplo`) |
| `jogos/` | per-game configs (one folder per game, on their own branches) |
| `docs/` | decisions, tests, pending work and the feasibility study |

## Where it came from

The port was written in October 2026 for **Batida Capixaba**, a rhythm game about the *congo* music of
Espírito Santo, Brazil. The game's Switch config and history live on the
[`batida-capixaba`](../../tree/batida-capixaba) branch (`jogos/batida-capixaba/`), which builds the
same version the game ships.

## Credits and references

- [Godot Engine](https://godotengine.org) (MIT): the engine.
- The **Godot 3.5 Switch port** by [Stary2001](https://github.com/Stary2001) and
  [cpasjuste](https://github.com/cpasjuste) (MIT): the reference for the console-facing parts.
- [devkitPro](https://devkitpro.org) and [libnx](https://github.com/switchbrew/libnx): the Switch
  homebrew toolchain and library.
- [danfromtico/mesa-switch](https://github.com/danfromtico/mesa-switch) (Mesa, MIT): Vulkan (NVK) and a
  modern Mesa for Horizon; also used by [NetherSX2_nx](https://github.com/NaGaa95/NetherSX2_nx).
- [Atmosphère](https://github.com/Atmosphere-NX/Atmosphere): the custom firmware the `.nro` runs on.

Nintendo Switch is a trademark of Nintendo. This project is not affiliated with Nintendo.

## License

MIT ([LICENSE](LICENSE)), like Godot. Generated games keep their own licenses.
