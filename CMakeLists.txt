cmake_minimum_required(VERSION 3.24)

project(mkxp_z VERSION 2.4.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_SOURCE_DIR}/cmake/conan)

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
add_subdirectory(assets)
add_subdirectory(shader)

add_library(mkxp_z SHARED)

add_subdirectory(binding)
add_subdirectory(interfaces)
add_subdirectory(src)

target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}
        )

target_link_libraries(mkxp_z PUBLIC
        MKXPZ_Assets
        MKXPZ_Shaders)

target_compile_definitions(mkxp_z PUBLIC
        MKXPZ_VERSION="${CMAKE_VERSION}"
        MKXPZ_GIT_HASH="63465f13b5f82b1e5b9f80d1b17972bfe747db20"
        )

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_subdirectory(windows)
endif()


install(TARGETS mkxp_z RUNTIME DESTINATION ${CMAKE_SOURCE_DIR}/../ext/mkxp_z/bin ARCHIVE DESTINATION ${CMAKE_SOURCE_DIR}/../ext/mkxp_z/lib)
install(FILES binding/binding-mri.h DESTINATION ${CMAKE_SOURCE_DIR}/../ext/mkxp_z/include)

set(CMAKE_CXX_COMPILER)

add_executable(test_app test/main.cpp)

target_include_directories(test_app PUBLIC
        ${Ruby_INCLUDE_DIRS})

target_link_libraries(test_app PUBLIC ${RUBY_LIBRARY})# mkxp_z)

get_target_property(ALL_INCLUDES mkxp_z INCLUDE_DIRECTORIES)

add_library(minimal_example SHARED binding/binding-util.cpp binding/etc-binding.cpp src/etc/etc.cpp src/etc/table.cpp minimal.cpp
        src/crypto/rgssad.cpp src/filesystem/filesystemImpl.cpp src/util/iniconfig.cpp src/config.cpp
        src/singletons/ConfigManager.cpp binding/font-binding.cpp src/display/font.cpp src/filesystem/filesystem.cpp
        binding/filesystem-binding.cpp src/display/bitmap.cpp binding/bitmap-binding.cpp src/singletons/DisplayManager.cpp
        src/display/gl/gl-meta.cpp src/display/gl/shader.cpp src/display/libnsgif/libnsgif.c src/singletons/TimeManager.cpp
        src/display/gl/vertex.cpp src/display/libnsgif/lzw.c src/display/gl/gl-debug.cpp src/display/gl/gl-fun.cpp
        src/display/gl/glstate.cpp src/system/systemImpl.cpp src/display/sprite.cpp binding/sprite-binding.cpp
        src/display/viewport.cpp binding/viewport-binding.cpp src/display/gl/scene.cpp src/display/window.cpp
        src/display/windowvx.cpp binding/window-binding.cpp binding/windowvx-binding.cpp src/display/gl/tilequad.cpp
        src/display/autotiles.cpp src/display/autotilesvx.cpp src/display/tilemap.cpp src/display/tilemapvx.cpp
        binding/tilemap-binding.cpp binding/tilemapvx-binding.cpp src/display/gl/tileatlas.cpp
        src/display/gl/tileatlasvx.cpp binding/table-binding.cpp src/display/plane.cpp binding/plane-binding.cpp
        src/audio/audio.cpp binding/audio-binding.cpp src/audio/alstream.cpp src/audio/audiostream.cpp
        src/audio/fluid-fun.cpp src/audio/midisource.cpp src/audio/sdlsoundsource.cpp src/audio/soundemitter.cpp
        src/audio/vorbissource.cpp src/singletons/AudioManager.cpp src/input/input.cpp src/input/keybindings.cpp
        binding/input-binding.cpp src/singletons/InputManager.cpp src/singletons/ThreadManager.cpp
        interfaces/AbstractEventThread.cpp src/theoraplay/theoraplay.c src/display/graphics.cpp
        binding/graphics-binding.cpp)

#src/net/LUrlParser.cpp src/net/net.cpp binding/http-binding.cpp)

message("Includes: ${ALL_INCLUDES}")
target_include_directories(minimal_example PUBLIC ${ALL_INCLUDES})

target_link_libraries(minimal_example PUBLIC
        ${RUBY_LIBRARY}
        ${Ruby_LIBRARY}
        SDL2::SDL2
        SDL2_image::SDL2_image
        SDL2_ttf::SDL2_ttf
        SDL2_sound::SDL2_sound-static
        Ogg::Ogg
        physfs::physfs
        OpenAL::OpenAL
        Vorbis::Vorbis
        uchardet::uchardet
        theora::theora
        pixman-1
        MKXPZ_Assets
        MKXPZ_Shaders
        wsock32
        Ws2_32
        )

target_compile_definitions(minimal_example PUBLIC
        RGSS_VERSION=1
        SDL_MAIN_HANDLED
        MKXPZ_ALCDEVICE=ALCdevice)