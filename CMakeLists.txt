cmake_minimum_required(VERSION 3.24)

project(MKXP-Z VERSION 2.4.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/cmake)
include(conan)


conan_cmake_configure(REQUIRES
        ogg/1.3.5
        openal/1.22.2
        physfs/3.0.2
        sdl/2.28.2
        sdl_image/2.0.5
        sdl_ttf/2.20.2
        theora/1.1.1
        uchardet/0.0.7
        vorbis/1.3.7
        zlib/1.2.13
        GENERATORS cmake_find_package)

conan_cmake_autodetect(settings)

message("Compilers: ${CMAKE_C_COMPILER}, ${CMAKE_CXX_COMPILER}")
conan_cmake_install(PATH_OR_REFERENCE .
        BUILD missing
        REMOTE conancenter
        SETTINGS ${settings}
        ENV CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER})

set(EXTERNAL_DEPS ${CMAKE_SOURCE_DIR}/windows/build-mingw64/)
set(EXTERNAL_INCLUDES ${EXTERNAL_DEPS}/include)
set(EXTERNAL_LIBS ${EXTERNAL_DEPS}/lib)

find_package(Ruby REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(Ogg REQUIRED)
find_package(PhysFS REQUIRED)
find_package(OpenAL REQUIRED)
find_package(Vorbis REQUIRED)
find_package(uchardet REQUIRED)
find_package(theora REQUIRED)

add_subdirectory(external)

add_compile_definitions(
        _WIN32
        SDL_MAIN_HANDLED
        MKXPZ_ALCDEVICE=ALCdevice
        MKXPZ_VERSION="${CMAKE_VERSION}"
        MKXPZ_GIT_HASH="63465f13b5f82b1e5b9f80d1b17972bfe747db20"
)

add_subdirectory(assets)
add_subdirectory(shader)

add_executable(MKXP-Z
        src/main.cpp
        src/config.cpp
        src/eventthread.cpp
        src/settingsmenu.cpp
        src/sharedstate.cpp

        src/audio/alstream.cpp
        src/audio/audio.cpp
        src/audio/audiostream.cpp
        src/audio/fluid-fun.cpp
        src/audio/midisource.cpp
        src/audio/sdlsoundsource.cpp
        src/audio/soundemitter.cpp
        src/audio/vorbissource.cpp
        src/theoraplay/theoraplay.c

        src/crypto/rgssad.cpp

        src/display/autotiles.cpp
        src/display/autotilesvx.cpp
        src/display/bitmap.cpp
        src/display/font.cpp
        src/display/graphics.cpp
        src/display/plane.cpp
        src/display/sprite.cpp
        src/display/tilemap.cpp
        src/display/tilemapvx.cpp
        src/display/viewport.cpp
        src/display/window.cpp
        src/display/windowvx.cpp

        src/display/libnsgif/libnsgif.c
        src/display/libnsgif/lzw.c

        src/display/gl/gl-debug.cpp
        src/display/gl/gl-fun.cpp
        src/display/gl/gl-meta.cpp
        src/display/gl/glstate.cpp
        src/display/gl/scene.cpp
        src/display/gl/shader.cpp
        src/display/gl/texpool.cpp
        src/display/gl/tileatlas.cpp
        src/display/gl/tileatlasvx.cpp
        src/display/gl/tilequad.cpp
        src/display/gl/vertex.cpp

        src/util/iniconfig.cpp
        src/util/win-consoleutils.cpp

        src/etc/etc.cpp
        src/etc/table.cpp

        src/filesystem/filesystem.cpp
        src/filesystem/filesystemImpl.cpp

        src/input/input.cpp
        src/input/keybindings.cpp

        src/net/LUrlParser.cpp
        src/net/net.cpp

        src/system/systemImpl.cpp

        binding/binding-mri.cpp
        binding/binding-util.cpp
        binding/table-binding.cpp
        binding/etc-binding.cpp
        binding/bitmap-binding.cpp
        binding/font-binding.cpp
        binding/graphics-binding.cpp
        binding/input-binding.cpp
        binding/sprite-binding.cpp
        binding/viewport-binding.cpp
        binding/plane-binding.cpp
        binding/window-binding.cpp
        binding/tilemap-binding.cpp
        binding/audio-binding.cpp
        binding/module_rpg.cpp
        binding/filesystem-binding.cpp
        binding/windowvx-binding.cpp
        binding/tilemapvx-binding.cpp
        binding/http-binding.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/windows
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/binding
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/audio
        ${CMAKE_SOURCE_DIR}/src/crypto
        ${CMAKE_SOURCE_DIR}/src/display
        ${CMAKE_SOURCE_DIR}/src/display/gl
        ${CMAKE_SOURCE_DIR}/src/display/libnsgif
        ${CMAKE_SOURCE_DIR}/src/display/libnsgif/utils
        ${CMAKE_SOURCE_DIR}/src/etc
        ${CMAKE_SOURCE_DIR}/src/filesystem
        ${CMAKE_SOURCE_DIR}/src/filesystem/ghc
        ${CMAKE_SOURCE_DIR}/src/input
        ${CMAKE_SOURCE_DIR}/src/net
        ${CMAKE_SOURCE_DIR}/src/system
        ${CMAKE_SOURCE_DIR}/src/util
        ${CMAKE_SOURCE_DIR}/src/util/sigslot
        ${CMAKE_SOURCE_DIR}/src/util/sigslot/adapter
        ${Ruby_INCLUDE_DIRS}
        ${uchardet_INCLUDE_DIRS}/uchardet
        )

target_link_libraries(MKXP-Z PUBLIC
        MKXPZ_Assets
        MKXPZ_Shaders
        ${Ruby_LIBRARY}
        SDL2::SDL2
        SDL2_image::SDL2_image
        SDL2_ttf::SDL2_ttf
        SDL2_sound::SDL2_sound
        Ogg::Ogg
        physfs::physfs
        OpenAL::OpenAL
        Vorbis::Vorbis
        uchardet::uchardet
        theora::theora
        pixman-1
        wsock32
        Ws2_32)
