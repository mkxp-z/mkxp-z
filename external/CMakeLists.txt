add_subdirectory(pixman)

set(SDLSOUND_BUILD_SHARED OFF CACHE BOOL "No shared library" FORCE)
set(SDLSOUND_BUILD_STATIC ON CACHE BOOL "Yes static library" FORCE)
set(SDLSOUND_BUILD_TEST OFF CACHE BOOL "No SDL tests" FORCE)
add_subdirectory(SDL_sound)