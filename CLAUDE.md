# CLAUDE.md — guide for AI agents working in godot-switch-nro

This repository ports **Godot 4.7.2** to **Nintendo Switch homebrew** (`.nro` on Atmosphère) and
packages any Godot project for it. Read `README.md` first; the decisions behind every non-obvious
choice are in `docs/decisoes.md` (D-01…D-20). Docs and code comments are in Brazilian Portuguese;
keep writing them in Portuguese. Talk to the user in the language they write in.

## Layout

- `gsn.py` — the CLI (`preparar`, `motor`, `gerar`, `info`). Runs on the host (Windows or Linux);
  calls the bash scripts through `wsl.exe -d Ubuntu -u root` on Windows, `bash` on Linux.
- `scripts/*.sh` — run inside Linux/WSL with Docker: `preparar_godot.sh`, `preparar_nvk.sh`,
  `construir_motor.sh opengl|vulkan`, `empacotar.sh`. Shared settings in `scripts/comum.sh` (pinned
  Godot tag, mesa-switch commit, work dir `$GSN_TRABALHO`, default `~/godot-switch-nro-trabalho`).
- `platform/switch/` — the Godot platform (C++). Copied into `$GSN_TRABALHO/godot` (OpenGL) and
  `$GSN_TRABALHO/godot-vk` (Vulkan) on every build; never edit the copies.
- `patches/godot/` — patches applied to the Godot tree; `patches/mesa-switch/` — our NVK build files.
- `exemplo/` — minimal test project; `jogos/<game>/` — per-game configs (game branches only).

## Hard rules

1. **The engine never knows a game.** No game names, paths or assets in `platform/switch/`. Names come
   from `argv[0]` (`<jogo>.nro`, `<jogo>.vulkan`, `<jogo>.pck`); per-game data goes through the romfs
   (`abertura.rgba`, `versao.txt`, `jogo.pck`) and the NACP, assembled by `gsn.py`.
2. **Both backends build.** OpenGL (`GLES3_ENABLED`, Mesa 20.1) and Vulkan (`RD_ENABLED`, NVK) are
   separate binaries; guard backend code with those macros. Run `python gsn.py motor todos` after any
   C++ change.
3. **Nothing survives exit.** Every thread the port creates must be joined and every service closed
   before `main` returns: a leftover thread pins heap pages and the homebrew loader crashes loading the
   next, larger `.nro` (2001-0106, D-14). The exit check writes `<jogo>.saida.txt`; keep it reporting
   "tudo solto".
4. **Versions are pinned** (Docker digest, Godot tag, mesa-switch commit). Changing one means redoing the
   console tests in `docs/testes.md`.
5. **You cannot test on a Switch.** Builds and packaging can be checked locally; behavior on the console
   needs the user. Ask for the log (`<jogo>.log` / `<jogo>.vulkan.log`), `<jogo>.saida.txt`,
   `vulkan_etapas.txt` and, after an error screen, the newest file in `sdmc:/atmosphere/crash_reports/`.
   Record results in `docs/testes.md`; new decisions in `docs/decisoes.md`; open items in
   `docs/pendencias.md`.
6. **Low-priority threads starve.** libnx pthreads start at priority 59 (below the main thread, 44);
   diagnostic threads that must keep running use `threadCreate` with priority 0x2B (D-15). Writes to
   stdout/stderr go through one lock (`__wrap__write_r`); keep it that way.

## Working on Windows

- Shell heredocs mangle backslashes (`\n`, `\r`) in this environment: write files with the editor tools
  or script files, not inline heredocs with escapes.
- The console is cp1252: run Python with `PYTHONUTF8=1` if output has accents.
- Docker runs inside WSL as root (`docker.io`, no Docker Desktop); `scripts/comum.sh` starts `dockerd`.
- A full engine build is ~9 min per backend (LTO); run it in the background.

## Typical tasks

- **New game:** `python gsn.py info <project>` then `python gsn.py gerar <project>`; if it needs
  overrides, add `jogos/<game>/jogo.cfg` (with `projeto_nome`) on a branch named after the game, or a
  `godot-switch-nro.cfg` in the game.
- **Engine change:** edit `platform/switch/`, build both backends, regenerate `exemplo`, then ask for a
  console test.
- **Godot or devkitPro update:** a separate, deliberate task: new patches, new pins, full retest.
