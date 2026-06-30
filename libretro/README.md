The build process for the libretro core is divided into two stages.

# Stage 1

All the files produced by this stage are platform-agnostic, so you can run this build stage on any computer, regardless of which operating system or CPU architecture the libretro core is for.

Required software:
* C and C++ compilers for the build machine that support C17 and C++17
* GNU Make
* GNU Autotools
* [Git](https://git-scm.com)
* [curl](https://curl.se)
* [Info-ZIP's Zip](https://infozip.sourceforge.net/Zip.html) (the `zip` package found in many package managers)
* [WASI SDK](https://github.com/WebAssembly/wasi-sdk) version 30 (the makefile will not permit earlier or later versions because using a different WASI SDK version breaks save state compatibility)
* [Binaryen](https://github.com/WebAssembly/binaryen) version 117 or later (the exact version doesn't matter because differing Binaryen versions don't break save state compatibility)
* Either [Universal Ctags](https://github.com/universal-ctags/ctags) or [Exuberant Ctags](https://ctags.sourceforge.net)
* Ruby (any reasonably recent version)

Go to the directory that this README.md is in and run this command, filling in the paths to WASI SDK, `wasm-opt` from Binaryen and `ctags` from Universal Ctags or Exuberant Ctags accordingly:

```
make WASI_SDK=/path/to/wasi-sdk WASM_OPT=/path/to/binaryen/bin/wasm-opt CTAGS=/path/to/ctags
```

This will produce the directory "libretro/build/libretro-stage1".

# Stage 2

This stage produces the actual core file. You need to build stage 1 first and make sure it's located at libretro/build/libretro-stage1 relative to the root directory of this repository.

Required software:
* C and C++ compilers for the build machine that support C99 and C++11
* C and C++ compilers for the host machine that support C99 and C++17
* [Git](https://git-scm.com)
* [Meson](https://mesonbuild.com)
* [Ninja](https://ninja-build.org)
* [CMake](https://cmake.org)

No software libraries are required other than the system libraries.

If the host machine has the same operating system and CPU architecture as the build machine, to build a libretro core, go to the root directory of this repository and run:

```
meson setup build -Dlibretro=true
cd build
ninja
```

To build a libretro core for a host machine that has a different operating system and/or CPU architecture as the build machine, you need to create a cross file first to tell Meson about the host machine. If you're building for a game console, you can use one of the "meson-" files already in this directory as the cross file. In most other cases, a file of the following format will be sufficient, replacing the angle-bracket strings with the correct values:

```ini
[binaries]
c = '<path to host C compiler>'
cpp = '<path to host C++ compiler>'
ar = '<path to the ar program that creates static libraries for the host machine>'
cmake = '<path to CMake binary>'
[host_machine]
system = '<see https://mesonbuild.com/Reference-tables.html#operating-system-names>'
cpu_family = '<see https://mesonbuild.com/Reference-tables.html#cpu-families>'
cpu = '<the output of uname -m on the host machine, or the same as cpu_family if this is not an option>'
endian = '<big|little>'
```

When building for Android, you need to add additional lines of the following format at the bottom of the cross file, replacing the angle-bracket strings with the correct values:

```ini
[cmake]
CMAKE_SYSTEM_PROCESSOR = '<armv7-a|aarch64|i686|x86_64>'
```

When building for Emscripten, you need to add additional lines of the following format at the bottom of the cross file, replacing the angle-bracket strings with the correct values:

```ini
[properties]
cmake_toolchain_file = '<path to Emscripten root directory>/cmake/Modules/Platform/Emscripten.cmake'
```

Once the cross file is set up, go to the root directory of this repository and run these commands to build the core, replacing the angle-bracket strings with the correct values.

```
meson setup build -Dlibretro=true --cross-file <path to cross file>
cd build
ninja
```
