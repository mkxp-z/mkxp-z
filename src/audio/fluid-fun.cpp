#include "fluid-fun.h"

#include <string.h>
#ifndef MKXPZ_RETRO
#  include <SDL_loadso.h>
#  include <SDL_platform.h>
#endif // MKXPZ_RETRO

#include "debugwriter.h"

#if defined(__LINUX__) || defined(__ANDROID__)
#  define FLUID_LIB "libfluidsynth.so.3"
#elif defined(MKXPZ_BUILD_XCODE)
#  define FLUID_LIB "@rpath/libfluidsynth.dylib"
#elif defined(__APPLE__)
#  define FLUID_LIB "libfluidsynth.3.dylib"
#elif defined(__WIN32__)
#  define FLUID_LIB "fluidsynth.dll"
#elif !defined(MKXPZ_RETRO) && !defined(SHARED_FLUID)
#  error "platform not recognized"
#endif

struct FluidFunctions fluid;
#if !defined(MKXPZ_RETRO) && !defined(SHARED_FLUID)
static void *so;
#endif

void initFluidFunctions()
{
#if defined(MKXPZ_RETRO) || defined(SHARED_FLUID)

#define FLUID_FUN(name, type) \
	fluid.name = fluid_##name;

#define FLUID_FUN2(name, type, real_name) \
	fluid.name = real_name;

#else
	so = SDL_LoadObject(FLUID_LIB);

	if (!so)
		goto fail;

#define FLUID_FUN(name, type) \
	fluid.name = (type) SDL_LoadFunction(so, "fluid_" #name); \
	if (!fluid.name) \
		goto fail;

#define FLUID_FUN2(name, type, real_name) \
	fluid.name = (type) SDL_LoadFunction(so, #real_name); \
	if (!fluid.name) \
		goto fail;
#endif

FLUID_FUNCS
FLUID_FUNCS2

	return;

#if !defined(MKXPZ_RETRO) && !defined(SHARED_FLUID)
fail:
	Debug() << "Failed to load " FLUID_LIB ". Midi playback is disabled.";

	memset(&fluid, 0, sizeof(fluid));
	SDL_UnloadObject(so);
	so = 0;
#endif
}
