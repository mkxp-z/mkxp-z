# Ruby 1.8: patches and build notes

## Source

- **Upstream**: JoiPlay's `ruby_1_8` branch: <https://github.com/joiplay/ruby>
- **Branch**: `ruby_1_8` (submodule at `sources/ruby18`)
- **Commit**: `50783b8` ("\* 2014-01-28"): git-svn mirror of the
  official Ruby SVN repository (revision 44718)
- **Reported version**: 1.8.8 (per `version.h`)
- **Note**: Last maintenance snapshot of Ruby 1.8, frozen since 2014.

## Why Ruby 1.8?

Authors wrote most RPG Maker XP games (RGSS1) against Ruby 1.8. A host
picks the Ruby for each game (`mkxp_setActiveRubyVersion`) and sends
RGSS1 games to this build. Vintage Pokemon Essentials forks then run on the
parser their authors used, not through Ruby 3.1's syntax-transform
patches.

## Patches

`deps/ruby18.patches.lst` lists the patches in their order. The
makefile applies each one with `git apply` before `autoconf`:

### `config.guess` and `config.sub`: updated for aarch64

The original 2014-era autoconf helper scripts do not recognize modern
platform triplets like `aarch64-apple-darwin`. We replaced both files
with current versions from the GNU config project so that `./configure
--host=aarch64-apple-darwin` works correctly.

### `lazy-sweep.patch`: sweep in small steps

A collection marks, then sweeps only until it has 2048 free slots.
`rb_newobj()` sweeps the next part each time the free list is empty,
so one collection does not stop the game.

A live object keeps its mark bit until the sweep gets to its slot.
`rb_obj_clone()` and `CLONESETUP` give a new object all the flags of
another object, so the patch removes the mark bit from those flags. Without that, the next collection does not mark what the copy
holds and frees it. For example, the clone of a playing sound loses
its name.

A collection can also start after the new object exists and before it
gets its flags, for example while `Array#reverse` allocates the buffer
of its copy. That collection marks the new object, so `OBJSETUP` and
`rb_obj_clone()` keep that mark bit. `DUPSETUP` gets this through
`OBJSETUP`. Without it, the sweep frees the new object itself.

`ObjectSpace.each_object` finishes the sweep after each call of its
block. A block that starts a collection otherwise gets a dead object
from a slot that the sweep did not get to yet, and the sweep then frees
that object while the game holds it.

### Engine-side accommodations (in mkxp-z, not in Ruby source)

These are not patches to Ruby itself. They are engine adaptations that
Ruby 1.8 needs on iOS:

1. **4MB RGSS thread stack**: Ruby 1.8's GC (`mark_locations_array`)
   scans the entire thread stack for object references. The default 512KB
   iOS pthread stack causes SIGBUS when GC hits the guard page. The fix
   uses `SDL_CreateThreadWithStackSize` with 4MB.

2. **GC stack base update**: `rb_gc_stack_start` (a global in `gc.c`)
   records the stack base at `ruby_init()` time. `ruby_init()` runs only
   once, because CRuby 1.8 cannot restart. Later RGSS threads have
   different stacks, but GC still scans the old one and hits SIGSEGV.
   The fix force-updates `rb_gc_stack_start` at the start of each
   session via `extern VALUE *rb_gc_stack_start`.

3. **VM persistence**: CRuby 1.8's VM cannot restart
   (`ruby_cleanup()` + `ruby_init()` causes SIGSEGV). The engine calls
   `ruby_init()` and `Init_*()` only once. On later game sessions, it
   clears leftover Ruby state with `rb_eval_string_protect`.

4. **RAPI clamping**: the engine clamps `RAPI_FULL=188` to `187` so it
   selects the RGSS1 binding codepath.

## iOS build instructions

The standard makefile system now builds Ruby 1.8:

```bash
cd deps
make -f iphoneos.make ruby18       # device build
make -f iphonesimulator.make ruby18  # simulator build
```

Or as part of the full build:

```bash
make -f iphoneos.make
```

The makefile automatically:

1. Applies the patches of `deps/ruby18.patches.lst` via `git apply`
2. Runs `autoconf` to generate `configure`
3. Cross-compiles with `-std=gnu89` and the applicable `-Wno-*` flags
4. Builds the core library (`libruby18-static.a`)
5. Builds extensions (zlib, stringio, strscan, thread, digest, fcntl) into `libruby18-ext.a`
6. Installs libs to `$(LIBDIR)` and headers to `$(INCLUDEDIR)/ruby18/`

### Key build flags

- `-std=gnu89`: required because Ruby 1.8 is K&R-style C code
- `-Wno-implicit-function-declaration`, `-Wno-implicit-int`, etc.:
  suppress warnings-turned-errors for legacy C code
- `--host=aarch64-apple-darwin --build=x86_64-apple-darwin`:
  cross-compilation triple

### Output

| Artifact             | Description                                                           |
| -------------------- | --------------------------------------------------------------------- |
| `libruby18-static.a` | Core Ruby 1.8 interpreter (VM, parser, GC, core classes)              |
| `libruby18-ext.a`    | Bundled C extensions (zlib, stringio, strscan, thread, digest, fcntl) |
| `include/ruby18/*.h` | 17 public headers (ruby.h, intern.h, etc.)                            |

### Linking

The Xcode project (`project.yml`) links Ruby 1.8 via:

```yaml
OTHER_LDFLAGS:
  - -lruby18-static
  - -lruby18-ext
```

Header search path: `$(DEPENDENCY_SEARCH_PATH)/include/ruby18`
