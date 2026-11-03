# Debug logging on the Switch

This page covers the logs the Switch build writes, how to turn on the detailed
ones, how to get them off the console, and how to read them.

## Quick start

1. Create an empty file named `debug.txt` in `sdmc:/switch/mkxp-z/` (next to
   the `Games` folder). Either put the SD card in a PC, or use USB mode with an
   MTP app such as DBI and copy the file over.
2. Start the game and reproduce the problem.
3. Close the game completely: press HOME, highlight the game you started the
   Homebrew Menu from, press X and choose Close. Until you do, the files are
   still open, and the logs can be cut short or missing over MTP.
4. Copy these files off the root of the SD card: `mkxpz_log.txt`,
   `mkxpz_mem.log` and `mkxpz_boot.log`. If the game crashed, also copy the
   newest files from `sdmc:/atmosphere/crash_reports/`.
5. Delete `debug.txt` when you're done. Tracing adds a little overhead: it
   times drawing calls and writes to the SD card.

Each launch starts its logs over, so copy them before you launch again.

## Flag files

Each feature turns on when its file exists in `sdmc:/switch/mkxp-z/`. The
contents don't matter, so an empty file works.

| File | Effect |
|---|---|
| `debug.txt` | Detailed tracing: everything in [Debug tracing](#debug-tracing) below. |
| `alwave.txt` | Needs `debug.txt` as well. OpenAL mixes into `sdmc:/mkxpz_al.wav` instead of the speakers, so there is no sound while it's on. It tells mixer problems apart from Switch audio-output problems. The game writes `alsoft_wave.conf` next to it; delete both files afterwards. |
| `noboost.txt` | Keeps the stock 1020 MHz CPU clock instead of raising it to 1785 MHz while the game runs. |

## Controller profiles (`controls.json`)

Profiles made in the game picker's Settings tile are saved to
`sdmc:/switch/mkxp-z/controls.json`, with the name of the active profile.
Keys are stored by their SDL names (`"Return"`, `"Left Shift"`, `"F6"`), and
inputs by the names in `switch/switch_controls.cpp` (`"A"`, `"ZL"`,
`"LStickUp"`, ...). The file can be edited by hand. If it's missing or
unreadable, the built-in Default profile is used. Default keeps the engine's
own controller bindings, so deleting the file resets everything. With
`debug.txt`, `mkxpz_log.txt` shows which profile each game started with:

```
controls: using custom profile 'Lefty'
```

## Logs written on every launch

### `sdmc:/mkxpz_boot.log`

These are breadcrumbs written before and during startup. A separate file
opened and closed per line, so it survives crashes that happen before normal
logging starts (for example in the game picker).

```
userAppInit: start
userAppInit: romfsInit ok
cpu: 1020 MHz -> 1785 MHz ok
...
logging: stdout -> sdmc:/mkxpz_log.txt ok
main: RGSS thread started
```

- The **`cpu:`** line shows the clock before and after the boost. `FAILED` means
  the clock service refused; a sys-clk profile for the game you launched
  through also overrides it.
- If **`logging:`** says `FAILED`, the main log went to
  `sdmc:/switch/mkxpz_log.txt` instead, or failing that, to the end of this
  file.

### `sdmc:/mkxpz_log.txt`

This is everything mkxp-z and the game print, including Ruby error messages.
The first lines describe the launch:

```
--- MKXP-Z Switch Log Start --- (debug tracing on)
launch: applet type = application (full memory)
launch: memory total=3285MB used=3281MB
mkxp-z: game folder 'sdmc:/switch/mkxp-z/Games/Fire Ash 3.6 Part 2'
```

`applet type` matters. `library applet (Album launch, limited memory)` means
the Homebrew Menu was opened from the Album, which gives homebrew only about
440 MB. Large games such as Pokémon Fire Ash need about 500 MB. Open the
Homebrew Menu by holding R while starting any game instead.

## Debug tracing

With `debug.txt` present, `mkxpz_log.txt` gets far more detail, and
`mkxpz_mem.log` is written as well.

OpenAL also logs its setup and any warnings (lines starting with `[ALSOFT]`),
such as buffer underruns.

### Slow frames (`mkxpz_log.txt`)

`romfs:/preload/perf_trace.rb` logs every frame that takes longer than
150 ms, with a breakdown of where the time went:

```
[perf] GC.start called from eval:77:clear < 032:Scene_Map:86:transfer_player < 032:Scene_Map:164:block in update
[perf] slow frame 793ms scene=Scene_Map present=23ms ruby=542ms gc=1(major 1) live=656642 | Bitmap.new(file)=115ms(12) Bitmap#blt=34ms(754) Audio.bgm_play=28ms(1) Bitmap#draw_text=17ms(56) slowest_file=Graphics/Tilesets/Outside(55ms) slowest_call=load_data(Data/Map260.rxdata)=6ms
[perf]   major gc by :force (gc_by :method): old objects 172426/689704 (25%), wb-unprotected 0/0 (0%), oldmalloc 19397KB/262144KB (7%), free slots 22454 of 1260287
```

| Field | Meaning |
|---|---|
| `slow frame` | Time since the previous `Graphics.update` call started. |
| `scene` | Class of `$scene` (`Scene_Map` is the overworld; Essentials runs battles and menus inside it). |
| `present` | Time spent in the previous `Graphics.update`, which draws and shows the frame. |
| `ruby` | Time not covered by any timed call: the game's own Ruby code plus garbage collection. |
| `gc=N(major M)` | Garbage collections during the frame. Major ones scan everything and cost hundreds of milliseconds. |
| `live` | Live Ruby objects. |
| after the `\|` | The four engine calls that took longest, as `name=total time(calls)`. Covers bitmap loading and drawing, sprites, windows, tilemaps, transitions, audio, `Marshal`, `load_data` and Ruby's `File`, `IO` and `Dir` calls. Nested timed calls count only once. |
| `slowest_file` | The slowest image loaded from a file during the frame. |
| `slowest_call` | The slowest `File`/`IO`/`Dir`/`Marshal`/`load_data` call, with its path (or, for `Marshal.load` from memory, the data size). |
| `major_gc_in` | For each major GC in the frame, the innermost timed call that was running when it happened. If it's missing, the GC happened in plain Ruby code. |

The indented `major gc` line appears when a slow frame included a major
collection. It gives Ruby's reason (`GC.latest_gc_info(:major_by)`) and how
close each trigger was in the frame before:

- `:oldgen`: too many long-lived objects.
- `:shady`: too many objects without write barriers. Most engine objects
  (bitmaps, sprites, tables) are this kind.
- `:oldmalloc`: too much memory allocated by long-lived objects.
- `:nofree`: the heap ran out of free slots.
- `:force`: a full collection was requested explicitly rather than triggered
  by a limit.

`gc_by` says what started that collection: `:method` means Ruby code called
`GC.start`, `:capi` means C code (the engine, or Ruby itself, for example when
opening a file fails for lack of file handles), `:malloc` means a memory
allocation, and `:newobj` means the object heap filled up. Every call to
`GC.start` or `ObjectSpace.garbage_collect` is also logged on its own line
with the script and line it came from:

```
[perf] GC.start called from eval:77:clear < 032:Scene_Map:86:transfer_player < 032:Scene_Map:164:block in update
```

`eval:77:clear` is mkxp-z's built-in `RPG::Cache.clear`
(`binding/module_rpg1.rb.xxd`). Like RPG Maker XP's own, it runs a full GC
so the cleared images are freed. Pokémon Essentials calls it on every map
transfer and battle start, which costs about 0.35 s each time on the Switch.

Ruby's own GC time counter does not work on the Switch: it uses CPU-time
clocks that return 0. Compare `ruby=` in frames with and without a major GC
instead.

Only `Graphics.update` frames are measured. `Graphics.transition` presents
its frames inside one call, so a transition shows up as a timed
`Graphics.transition` entry rather than as lost time.

### Memory, frame rate and audio health (`sdmc:/mkxpz_mem.log`)

The engine appends one line every 5 seconds:

```
t=95s fps=38.6 worst_frame=91ms cpu=1785MHz malloc used=452MB peak=544MB heap=683MB al_connected=1 al_clock=94807ms sdl_audio=0,1,0
```

| Field | Meaning |
|---|---|
| `t` | Seconds since logging started (after the game picker). |
| `fps` | Frames presented in the last 5 s, per second. Pokémon Essentials games aim for 40 when they use EBDX, otherwise 60. |
| `worst_frame` | Longest gap between two presented frames in the last 5 s. Unlike the Ruby slow-frame log, this includes transitions and freezes outside Ruby. |
| `cpu` | Current CPU clock. It should stay at 1785 MHz while the game is in focus. |
| `malloc used / peak / heap` | C heap in use, the highest it has been, and the heap size. Compare against `launch: memory total`. |
| `al_connected` | 1 while the OpenAL output device is alive. |
| `al_clock` | OpenAL's mixer clock. It should advance by about 5000 ms per line; if it stops, audio output has stalled. |
| `sdl_audio` | SDL audio device states for devices 1–3 (0 stopped, 1 playing, 2 paused). |

## Crashes

If the Switch says "The software was closed because an error occurred",
Atmosphère writes a report to `sdmc:/atmosphere/crash_reports/`, named
`<timestamp>_<program id>.log`. The program ID is the game you launched the
Homebrew Menu through, not mkxp-z. The report lists the faulting thread's PC,
LR and stack trace as `mkxp-z + 0x...` offsets.

To turn the offsets into function names, run `addr2line` on the ELF from the
same build:

```bash
/opt/devkitpro/devkitA64/bin/aarch64-none-elf-addr2line -f -C -i \
  -e build/mkxp-z.cortex-a57 0x133160 0xc8c08 0xfa24
```

The ELF must come from the same build as the NRO that crashed: offsets change
with every rebuild. A truncated `mkxpz_log.txt` together with a crash report
is normal, because a crash cuts the log off before its final size reaches
the SD card.

A Ruby error normally shows a message box and is written to `mkxpz_log.txt`
with its backtrace.

## Where the code lives

| What | Where |
|---|---|
| Flag files, boot log, launch info, memory log, CPU boost, OpenAL debug | `switch/switch_main.cpp` |
| `SWITCH_TRACE` macro, for tracing in `switch/` | `switch/switch-trace.h` |
| Frame counter for `mkxpz_mem.log` | `EventThread::notifyFrame` in `src/eventthread.cpp` |
| Slow-frame tracer | `switch/romfs-root/preload/perf_trace.rb` |
| Unbuffered Ruby output, so Ruby's output appears in the log immediately | `switch/romfs-root/preload/log_sync.rb` |
