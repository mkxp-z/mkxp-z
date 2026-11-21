# SDL2: patches and build notes

## Source

- **Upstream**: SDL 2.28.1
- **Fork**: <https://github.com/mkxp-z/SDL> branch `mkxp-z-2.28.1`
- **Base commit**: `4761467b2` ("Updated to version 2.28.1 for release")

## Patches in mkxp-z/SDL (submodule)

Three custom commits on top of upstream SDL 2.28.1:

1. **`07550ddbf`** (Struma): Remove `-mwindows` linker flag
2. **`5042c1559`** (Struma): Disable NEON, fix loading ANGLE on macOS
3. **`d3ac4c374`** (Splendide Imaginarius): Disable NEON in `SDL_stretch.c`

The NEON patches prevent build and runtime issues on ARM platforms
where the NEON intrinsics cause problems with the cross-compilation
toolchain.

## iOS patches (applied at build time)

The SDL submodule stays pinned to the published `mkxp-z-2.28.1`
tip. The build applies more iOS fixes from `deps/sdl2/` with
`sdl2.patches.lst` + `apply-sdl-patches.sh` (the same model as Ruby).

**`ios.patch`** - iOS runtime fixes on top of the fork tip:

- Defer renderbuffer resize to the GL-owning thread (rotation crash)
- Synchronous present to prevent SIGSEGV during rapid rotation
- Detect a broken GL context and bail out cleanly
- Create UIKit windows from the active `UIWindowScene`. The UIScene
  lifecycle on the iOS 27 SDK requires this, because iOS does not
  display legacy `initWithFrame:` windows.
- Keep the view frame the host set when the keyboard shows or hides,
  after the host moves SDL's view into its own window
- Open a URL with `openURL:options:completionHandler:` on the main
  thread. iOS 18.2 and later refuse the old `openURL:`, so
  `SDL_OpenURL` did nothing (libsdl-org/SDL#11728)

The first three change `SDL_uikitopengles.m` and
`SDL_uikitopenglview.m`. Those files do not compile while
`SDL_OPENGLES` is OFF, so the three fixes do nothing now. They stay in
the patch for a build that turns GL ES on again.

**`macos-no-opengl.patch`** - the macOS build of `SDL_cocoawindow.m`
calls `movedToNewScreen` on SDL's OpenGL contexts. That code does not
compile while `SDL_OPENGL` is OFF, so the patch puts it behind
`SDL_VIDEO_OPENGL_CGL`.

**`macos.patch`** - `SDL_OpenURL` on macOS calls `CFRelease` on a
NULL URL when the link is not UTF-8, and the process crashes. The
patch returns an error.

After you edit the SDL submodule, regenerate the patch:

```sh
cd deps/sources/sdl2
git diff origin/mkxp-z-2.28.1..HEAD > ../../sdl2/ios.patch
```

## iOS build instructions

The build uses CMake (out-of-tree in `cmakebuild/`):

```text
cmake .. \
  -DBUILD_SHARED_LIBS=no \
  -DSDL_OPENGL=OFF \
  -DSDL_OPENGLES=OFF \
  -DSDL_METAL=ON \
  -DSDL_RENDER_METAL=ON \
  <common CMAKE_ARGS from common.make>
```

Key flags:

- Desktop OpenGL disabled (`SDL_OPENGL=OFF`)
- OpenGL ES disabled (`SDL_OPENGLES=OFF`): mkxp-z drives ANGLE through
  EGL itself and never asks SDL for a GL context. With it ON,
  `SDL_uikitopenglview` linked in and pulled `EAGLContext`,
  `CAEAGLLayer` and four OES entry points with it, which forced
  `-weak_framework OpenGLES` on the MkxpCore link.
- Metal enabled for SDL's internal use

The build inherits common cross-compilation flags from `common.make`
(sysroot, architecture, deployment target, etc.).
