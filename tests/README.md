# Tests

The suites are split by what they need to run.

| Directory | Runs on | Needs |
| --- | --- | --- |
| `cpp/` | Your machine, in about a second | A C++ compiler |
| `midi/` | Your Mac, in about a second | The macosx dependencies |
| `engine/` | Inside a running engine | A simulator and the dependencies |
| `legacy-methods/` | Inside a running engine | The same, plus a Ruby on your machine |
| `host/` | - | It is what runs `engine/` and `legacy-methods/` |

Neither suite needs a launcher. `cpp/` needs nothing but this
repository.

## C++ tests

`cpp/` covers engine code that takes bytes and returns bytes. No
window, no GPU, no Ruby. It compiles the sources under test straight
from `src/`, so there is no library to build first.

```sh
tools/run-cpp-tests.sh
```

The script hides the engine's own stderr output unless a check fails.
Pass `--verbose` to see it anyway, or `--keep` to leave the binary in
place for a debugger.

To cover another file, write a `cpp/test_<name>.cpp` and add the
source to `ENGINE_SOURCES` in the script. `TEST(name)` registers a
case by itself, so no list needs an edit. Keep out anything that
pulls in SDL, PhysFS, or the Ruby binding. Those need the full engine
build, and they belong in `engine/` instead.

## MIDI tests

`midi/` covers the SoundFont that the engine gives to FluidSynth. It
links the static FluidSynth of the macosx dependencies and loads the
test SoundFonts in `deps/sources/fluidsynth/sf2`. CI runs it in the
`Engine for macosx` job.

```sh
tools/run-midi-tests.sh
```

## In-engine tests

`engine/` is a game folder. The engine boots it the way it boots any
game, which is the only way to reach the drawing code.

| File | Purpose |
| --- | --- |
| `harness.rb` | Assertions and result reporting. |
| `mkxp.json` | Config that turns the folder into a game. |
| `mega_bitmap.rb` | Mega-surface suite. |
| `ruby_compat.rb` | Behaviour every bundled interpreter must share. |
| `safe_write.rb` | A save that stops half way keeps the old file, and the folder lists hide the temp file, on each bundled interpreter. |
| `save_repair.rb` | A save with a broken sound name saves no sound, `save_data` too, a save with Shift_JIS symbols goes through, only a file with a `Game_System` that the game reads to the end counts as a save load, in Essentials 19 and later `Game.load` is the save load, a script that wraps `Game.load` after the engine does not make a loop, a block given to `Game.load` reaches the game, and the host gets one report for each kind of fault, on each bundled interpreter. |
| `dir_chdir.rb` | `Dir.chdir` with nil, an empty path, or a NUL raises or stays in the folder as RGSS does, on each bundled interpreter. |
| `legacy_methods_off.rb` | The legacy methods stay away when the transform is off. |
| `fast_forward_clock.rb` | `System.uptime` runs at the fast-forward speed, and the average frame rate does not. |
| `frozen_screen_rate.rb` | `Graphics.update` on a frozen screen takes one frame and counts one frame, also under fast-forward and after a slow step. |
| `pause_clock.rb` | The clocks and the average frame rate leave out a pause, a game dialog, and a cancelled pause. `tests/host/host.m` reads each dialog text as a command. |
| `sound_effect_read.rb` | The first play of a sound effect reads its file in blocks, not one byte for each read. |
| `file_lookup_order.rb` | A path without an extension loads the first matching file in name order, and a folder path loads no file. |
| `case_lookup.rb` | The file, IO, and folder calls find a file or a folder whose name is in another case, also a file that the game made after launch, on each bundled interpreter. |
| `gc_clone.rb` | A clone keeps its instance variables and singleton methods, and a copied array and an object from `ObjectSpace.each_object` stay alive, through a collection, on each bundled interpreter. |
| `gc_stress.rb` | Code like a game (clones of the playing sounds, battles) keeps its objects in GC stress mode, on each bundled interpreter. It runs in its own launch. |
| `system_launch.rb` | `System.launch` opens an http or https link and the game goes on, it returns for a link that is not UTF-8, and it raises for a program name. The step checks that Safari opens. It runs in its own launch, because the link sends the app to the background. |
| `rgss_ruby_version.rb` | Ruby 1.8 and 1.9 report `RUBY_VERSION` 1.8.1 and 1.9.2, as RPG Maker XP, VX, and VX Ace do. |
| `win32_wrap_encoding.rb` | Text conversion in the Win32API stand-ins, on each bundled interpreter. |
| `full_screen_resizer.rb` | The full-screen option of Pokemon Essentials gets a screen the size of the game from the Win32API stand-ins, on each bundled interpreter. |
| `run_suites.rb` | Runs several suites in one launch. |

Run it on a simulator with one command:

```sh
tools/run-engine-tests.sh
```

The script builds `EngineTests.app`, installs it on a booted
simulator, and prints the `[TEST]` lines the suite reports. It exits
non-zero when a check fails or when the suite never finishes. Pass
`--no-build` to reuse the last build, `--device <udid>` to pick a
simulator, and `--keep` to hold on to the console log.

The host sends the audio to the null backend of OpenAL Soft. The
suites play no sound and never call CoreAudio, because the audio
service of the Simulator can stop the app.

`--suite <file>` runs another file in the same folder, and
`--ruby 18|19|31` picks the interpreter. The host reads both at
launch, so one build runs every suite on every interpreter. Add
`--no-build` to each run after the first:

```sh
tools/run-engine-tests.sh
for v in 18 19 31
do
    tools/run-engine-tests.sh --suite ruby_compat.rb --ruby "$v" --no-build
done
```

Give `--suite` more than once to run several suites in one launch, in
that order. `run_suites.rb` runs them, and the script checks the
result of each suite on its own. On CI, the first launch takes 4 to 7
minutes, because it boots the simulator and installs the app. After
that, each run of the script takes 10 to 70 seconds. Most of that time
goes to finding the simulator and checking the installed app, so one
launch for several suites saves time:

```sh
tools/run-engine-tests.sh --ruby 19 --no-build \
    --suite ruby_compat.rb --suite win32_wrap_encoding.rb
```

`--config <json>` gives the engine settings on top of `mkxp.json`,
as a launcher does. CI runs `file_lookup_order.rb` and
`case_lookup.rb` with and without `--config '{"pathCache":false}'`.

`--fast-forward <n>` starts the engine at that fast-forward
multiplier. Run `fast_forward_clock.rb` and `frozen_screen_rate.rb`
with and without it:

```sh
tools/run-engine-tests.sh --suite fast_forward_clock.rb --suite frozen_screen_rate.rb --no-build
tools/run-engine-tests.sh --suite fast_forward_clock.rb --suite frozen_screen_rate.rb --fast-forward 4 --no-build
```

`--game` picks the game folder. `build-test-host-ios.sh --game <dir>`
puts each folder that it gets into the app bundle, so one build can
hold all three:

```sh
ruby tests/legacy-methods/pack_scripts.rb
ruby tests/script-folder/pack_scripts.rb
tools/build-test-host-ios.sh \
    --game tests/engine --game tests/legacy-methods --game tests/script-folder
tools/run-engine-tests.sh --game tests/script-folder --ruby 18 --no-build
```

Without `--no-build`, `run-engine-tests.sh` builds the app with the
one folder of its `--game`.

`ruby_compat.rb` is that suite. Each check there states a behaviour a
game gets on all three. It covers `$DEBUG` today: Ruby 1.8 and 1.9
bind that name to the interpreter's own debug flag, which makes
`sprintf` raise for a call that passes a spare argument, so
`binding/binding-mri.cpp` gives the name storage of its own.

Run `tools/build-deps-ios.sh iphonesimulator` once first. It builds
the dependency libraries the engine links against.

`mkxp.json` names the suite to run in its `customScript` key. Change
that key to run another one, or pass `--suite`. At launch the host
empties its Documents folder, copies the game folder out of the
read-only bundle, and writes the key into that copy. So the file in
the repository keeps its own default, and no launch sees the files of
the launch before it. It
also sets `maxTextureSize`, which is how the mega suite reaches the
mega paths. See below.

Write that file as strict JSON. The engine reads JSON5 and would
accept comments and unquoted keys, but a host launcher may parse the
same file with a stricter reader to decide whether the folder is a
game.

Any other mkxp-z build can boot the same folder:

```sh
mkxp-z --gameFolder=/path/to/mkxp-z-apple-mobile/tests/engine
```

## Legacy methods

`legacy-methods/` covers the methods that Ruby 1.9 removed and that
`binding/binding-mri.cpp` defines again for the Ruby 3.1 build:
`Array#nitems`, `Array#choice`, `Object#id`, and the rest. Each one
runs only for a script that the syntax transform parsed, and raises
NoMethodError everywhere else.

The engine sets that flag on the sections of `Scripts.rxdata` alone,
so this folder is a game that boots from `Scripts.rxdata` rather than
from a `customScript`. Pack the file first:

```sh
ruby tests/legacy-methods/pack_scripts.rb
tools/run-engine-tests.sh --game tests/legacy-methods
```

`pack_scripts.rb` compresses `engine/harness.rb` and
`legacy-methods/legacy_methods.rb` into `legacy-methods/Data/`, which
git ignores. Edit the `.rb` files and pack again.

`engine/legacy_methods_off.rb` covers the other side. The transform
target is a whole-run setting, so with the transform off no script can
call these methods. The engine must then leave the names alone, because
a game tests for them with `Module#method_defined?` and installs its own
copy. `engine/` has no `syntaxTransform` key, which the engine reads as
off:

```sh
tools/run-engine-tests.sh --suite legacy_methods_off.rb --ruby 31
```

## Script folder

`script-folder/` is a game whose one `Scripts.rxdata` section evals
every file of `Data/Scripts`, the way the Essentials v21 script
extractor leaves a game. The last of those files runs the game, so
the engine splits that section into one section per file and runs the
postloads before the last one. The suite checks that the postloads
see the classes of `Data/Scripts`, that the files keep the order of
the loader, and that `$RGSS_SCRIPTS` still holds the sections of
`Scripts.rxdata`:

```sh
ruby tests/script-folder/pack_scripts.rb
tools/run-engine-tests.sh --game tests/script-folder
```

The split is in `scripts/preload/script_folder_split.rb`.
`tools/test_script_folder.rb` tests it on the host, with no simulator.

## The test host

`host/` is the app that runs `engine/`. It is a shim, not a launcher:
one file that names a game folder and gets out of the way.

The engine's `main()` waits in `mkxp_waitForGamePath()` until a host
answers. A launcher answers when a person taps a game. This host
answers with the folder it copied out of its own bundle, and the
engine boots it like any game.

`tools/build-test-host-ios.sh` builds the app without Xcode. An iOS
app bundle is a directory with a binary, an `Info.plist`, and
resources, so the script compiles the shim, links it against the
engine, fills the bundle, and signs it ad hoc.

WARNING: the shim writes no bridge state from its constructor. That
constructor runs before the C++ static initializers in
`src/app_bridge.cpp`, and those wipe anything written first. The
constructor queues the work on the main queue instead, where
`mkxp_waitForGamePath` runs it while it waits.

## Reading the results

Both suites print the same line grammar. Every line starts with a
`[TEST]` tag:

```text
[TEST] SUITE mega-bitmap
[TEST] INFO game=engine
[TEST] INFO max_size=2048
[TEST] ok small: clear empties the whole bitmap
[TEST] PEND mega: blur softens an edge -- Operation not supported for mega surfaces
[TEST] FAIL small: blt copies a source rect into place -- first copied pixel at [4, 4]: expected [255, 0, 0, 255], got [0, 255, 0, 255]
[TEST] DONE passed=33 failed=0 pending=11
```

`PEND` comes from the in-engine suites only. It means the engine
refused the operation with a known "not supported" error. Treat it as
a to-do, not a defect: the suite states what the operation should do,
and the engine does not do it yet. When the engine gains the
operation, the line becomes `ok` on its own.

A suite prints one line per check. Compare a run before a change with
a run after it, and `diff` shows what moved.

## Mega surfaces

A bitmap too large for one GPU texture stays in main memory as an
SDL surface. Every drawing operation then needs a second
implementation that works on the CPU, and those are the paths the
mega suite covers.

The suites do not build huge bitmaps to reach those paths. They set
`maxTextureSize` in `mkxp.json` instead, which lowers the size at
which the engine switches to a mega surface. Keep that value above
the screen size. Below it, ordinary rendering starts taking the mega
paths too, and a failure no longer says which path broke.

## Writing an in-engine suite

Start from `mega_bitmap.rb`. A suite:

1. Loads the harness on its first lines. The engine skips
   `preloadScript` for a game that boots from a `customScript`, so
   each suite loads the harness itself.
2. Calls `EngineTest.suite('<name>')`. Name it after the file, minus
   `.rb` and with every `_` written as `-`. `run-engine-tests.sh`
   finds the result of each `--suite` by that name, so a suite the
   host failed to select cannot report a clean pass.
3. Wraps each check in `EngineTest.test('<name>') { ... }`.
4. Ends with `EngineTest.finish` and `exit`.

`mega_bitmap.rb` runs every check against two kinds of bitmap, so it
registers the checks with a local `check` helper first and runs the
list once per kind. A suite with nothing to vary can call
`EngineTest.test` directly.

Write each check so it passes on an ordinary bitmap today. Let the
harness report `PEND` for the paths the engine has not implemented:
do not delete the check, and do not write it to expect the error.

The suites must parse on Ruby 1.8, 1.9, and 3.x, because the engine
picks the interpreter per game.

## Credit

`mega_bitmap.rb` covers the same operations as
`tests/mega-bitmap/mega-bitmap-test.rb` in upstream mkxp-z, by
Splendide Imaginarius (GPLv2+). The checks were rewritten to assert
their results instead of writing images for a person to compare.
