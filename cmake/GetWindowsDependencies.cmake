if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86")
    set(DEPS_ARCH mingw)
else()
    set(DEPS_ARCH mingw64)
endif()

set(DEPS_ROOT ${CMAKE_SOURCE_DIR}/windows/build-${DEPS_ARCH})
set(DEPS_LIB_DIR ${DEPS_ROOT}/include)
set(DEPS_LIB_DIR ${DEPS_ROOT}/lib)
set(DEPS_PKG_DIR ${DEPS_LIB_DIR}/pkgconfig)

set(CMAKE_PREFIX_PATH ${DEPS_LIB_DIR} ${DEPS_PKG_DIR} ${CMAKE_PREFIX_PATH})

find_package(PkgConfig REQUIRED)

macro(get_dep PREFIX LIB NAMESPACE TARGET_NAME)
    pkg_check_modules(${PREFIX} REQUIRED ${LIB})
    add_library(${TARGET_NAME} INTERFACE)
    target_include_directories(${TARGET_NAME} INTERFACE ${${PREFIX}_INCLUDE_DIRS})
    target_link_libraries(${TARGET_NAME} INTERFACE ${${PREFIX}_LIBRARIES} ${${PREFIX}_LINK_LIBRARIES})
    add_library(${NAMESPACE}::${TARGET_NAME} ALIAS ${TARGET_NAME})
    add_library(${NAMESPACE}::${TARGET_NAME}-static ALIAS ${TARGET_NAME})
endmacro()

get_dep(PHYSFS physfs, PhysFS PhysFS)
get_dep(OPENAL openal OpenAL OpenAL)
get_dep(THEORA theora THEORA THEORA)
get_dep(VORBISFILE vorbisfile Vorbis vorbisfile)
get_dep(VORBIS vorbis Vorbis vorbis)
get_dep(OGG ogg OGG ogg)

set(SDL2_DIR ${DEPS_LIB_DIR}/cmake/SDL2)
find_package(SDL2 REQUIRED)
set(SDL2_image_DIR ${DEPS_LIB_DIR}/cmake/SDL2_image)
find_package(SDL2_image REQUIRED)
get_dep(SDL2_TTF SDL2_ttf SDL2_ttf SDL2_ttf)


get_dep(FREETYPE freetype2 Freetype Freetype)
get_dep(PIXMAN pixman-1 Pixman Pixman)
get_dep(PNG libpng PNG PNG)
get_dep(UCHARDET uchardet uchardet libuchardet)
get_dep(FLUIDSYNTH fluidsynth FluidSynth FluidSynth)

find_package(BZip2 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Iconv REQUIRED)

find_library(SDL2_SOUND_LIBRARY SDL2_sound REQUIRED)
add_library(SDL2_sound INTERFACE)
target_link_libraries(SDL2_sound INTERFACE ${SDL2_SOUND_LIBRARY} SDL2::SDL2)
add_library(SDL2_sound::SDL2_sound ALIAS SDL2_sound)
