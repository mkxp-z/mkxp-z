# mkxp-z for Nintendo Switch

A homebrew (`.nro`) build of mkxp-z for the Nintendo Switch, using devkitPro's
devkitA64 toolchain and libnx. It runs RPG Maker XP/VX/VX Ace games, such as
Pokémon Essentials fan games, from the SD card.

## Building

The port builds like any other mkxp-z cross build, with devkitPro's
devkitA64, libnx and switch-tools (e.g. the `devkitpro/devkita64` Docker
image), Meson, CMake, Ruby and bison:

```bash
meson setup --cross-file switch/switch.ini --buildtype release build
ninja -C build          # build/switch/nro/mkxp-z.nro
meson test -C build     # unit tests, run on the build machine
```

The tests (`switch/tests/`) need a native C++ compiler and SDL2's development
files (e.g. `g++ libsdl2-dev pkg-config`); without them they're skipped.

`switch/switch.ini` takes SDL2 and OpenAL from devkitPro's portlibs, since
they use the system APIs; everything else comes from mkxp-z's subprojects, as
on other platforms. Ruby and PhysFS get small Switch patches
(`subprojects/packagefiles/ruby-3.1-switch.patch`, `physfs-switch.patch`),
each behind `__SWITCH__`. `switch/nro/` packages the executable as a `.nro`
with a RomFS made of `switch/romfs-root/` and `scripts/preload/win32_wrap.rb`.

To build in Docker, start a shell in devkitPro's image, install the same
packages as the CI job (`.github/workflows/autobuild.yml`), then run the
commands above:

```bash
docker run --rm -it -v "$PWD":/src -w /src devkitpro/devkita64:20251117 bash
apt update && apt install -y bison git ruby python3-pip g++ libsdl2-dev pkg-config
pip3 install --break-system-packages meson
```

`switch/find_bad_tls.py` checks a disassembly of the ELF for devkitA64's
soft-TLS miscompile, which the cross file avoids with `-ftls-model=local-exec`
(CI runs it too):

```bash
/opt/devkitpro/devkitA64/bin/aarch64-none-elf-objdump -d build/mkxp-z.cortex-a57 > dis.txt
python3 switch/find_bad_tls.py dis.txt
```

## Installing and playing

1. Copy `build/switch/nro/mkxp-z.nro` to the SD card as `/switch/mkxp-z.nro`.
2. Put each game in its own folder under `/switch/mkxp-z/Games/`, with its
   `Game.ini`. A `mkxp.json` in the game folder is read as usual.
3. Start the Homebrew Menu by holding R while launching a game, not from the
   Album: the Album gives homebrew too little memory for large games.
4. Pick a game. Closing it returns to the picker; B in the picker exits. In
   handheld mode you can also tap a game to select it, tap it again to start
   it, and drag the row to scroll.

The picker's Settings tile edits controller profiles. The system keyboard is
used for text entry.

## Graphics

The port uses desktop OpenGL by default: it asks SDL for a 4.3 compatibility
context, under which the engine's existing GLSL 1.10 shaders run unchanged.
OpenGL ES 2.0 remains available as an explicit opt-in: configure with
`-Dgfx_backend=gles` (or set `SDL_OPENGL_ES_DRIVER=1`).

## Fast-forward

Games using Marin's "Better Fast-forward" script (Pokémon Fire Ash and many
other Essentials games) show fast-forward at 60 fps, so frames line up with
the screen's refreshes. In 40 fps games the script's own 2x and 3x stutter;
here 3x stays 3x and 2x becomes 1.5x, the closest speed that spaces evenly.
The game's own toggle button and settings still work. See
`romfs-root/preload/fast_forward.rb`.

## Map transfers

RPG Maker XP's built-in `RPG::Cache.clear`, which games call on map
transfers, runs its full garbage collection incrementally (`cache_gc.rb`),
so the pause is spread over the next few frames.

## Debugging

Every launch writes `sdmc:/mkxpz_log.txt`. See [DEBUGGING.md](DEBUGGING.md)
for the other logs, the detailed tracing switch and crash reports.
