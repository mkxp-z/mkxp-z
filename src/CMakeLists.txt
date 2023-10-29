include(DependencySetup)

find_package(PhysFS REQUIRED GLOBAL)
find_package(OpenAL REQUIRED GLOBAL)
find_package(THEORA REQUIRED GLOBAL)
find_package(Vorbis REQUIRED GLOBAL)
find_package(OGG REQUIRED GLOBAL)
find_package(SDL2 REQUIRED GLOBAL)
find_package(SDL2_sound REQUIRED GLOBAL)
find_package(SDL2_ttf REQUIRED GLOBAL)
find_package(Freetype REQUIRED GLOBAL)
find_package(Pixman REQUIRED GLOBAL)
find_package(PNG REQUIRED GLOBAL)
find_package(ZLIB REQUIRED GLOBAL)
find_package(Uchardet REQUIRED GLOBAL)

find_package(SDL2_image REQUIRED GLOBAL)

# We need to do a little bit of Aliasing to get the library names to be correct
add_library(PhysFS::PhysFS INTERFACE IMPORTED)
target_include_directories(PhysFS::PhysFS INTERFACE ${PHYSFS_INCLUDE_DIR})
target_link_libraries(PhysFS::PhysFS INTERFACE ${PHYSFS_LIBRARY})
set_target_properties(PhysFS::PhysFS PROPERTIES IMPORTED_GLOBAL ON)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_library(SDL2::Core ALIAS SDL2::SDL2-static)
    add_library(SDL2::Image INTERFACE IMPORTED)
    target_link_libraries(SDL2::Image INTERFACE
            SDL2_image::SDL2_image-static
            SDL2_image::brotlidec-static
            SDL2_image::brotlicommon-static
            SDL2_image::hwy
            SDL2_image::jxl_dec-static)
    set_target_properties(SDL2::Image PROPERTIES IMPORTED_GLOBAL ON)
else()
    add_library(SDL2::Core ALIAS SDL2::SDL2)
    add_library(SDL2::Image ALIAS SDL2_image::SDL2_image)
endif()

find_package(Bzip2 REQUIRED GLOBAL)
find_package(Iconv REQUIRED GLOBAL)
if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    find_library(CHARSET_LIB charset REQUIRED)
    set(GLOBAL_DEPENDENCIES ${GLOBAL_DEPENDENCIES} ${CHARSET_LIB} PARENT_SCOPE)
endif()

if (ENABLE_HTTPS)
    find_package(OpenSSL GLOBAL)
    if (OpenSSL_FOUND)
        set(GLOBAL_DEPENDENCIES ${GLOBAL_DEPENDENCIES} OpenSSL::OpenSSL PARENT_SCOPE)
        set(GLOBAL_PREPROCESSORS ${GLOBAL_PREPROCESSORS} MKXPZ_SSL PARENT_SCOPE)
        if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
            set(GLOBAL_LINK_ARGS ${GLOBAL_LINK_ARGS} -lcrypt32 PARENT_SCOPE)
        endif()
    else()
        message(WARNING "Could not locate OpenSSL. HTTPS will be disabled.")
    endif()
endif()

set(EXPLICIT_LIBS "")
if (CMAKE_HOST_SYSTEM_NAME STREQUAL WINDOWS)
    # TODO: Determine if the bit about the explicit libs is really necessary
endif()

if (BUILD_STATIC)
    if (CMAKE_HOST_SYSTEM_NAME STREQUAL WINDOWS)
        set(GLOBAL_LINK_ARGS ${GLOBAL_LINK_ARGS} -static-libgcc -static-libstdc++ -Wl,-Bstatic -lgcc -lstdc++ -lpthread -Wl,-Bdynamic PARENT_SCOPE)
    else()
        set(GLOBAL_LINK_ARGS ${GLOBAL_LINK_ARGS} -static-libgcc -static-libstdc++ PARENT_SCOPE)
    endif()
    set(GLOBAL_PREPROCESSORS ${GLOBAL_PREPROCESSORS} AL_LIBTYPE_STATIC PARENT_SCOPE)
endif()

foreach(LIB ${EXPLICIT_LIBS})
    if (NOT LIB STREQUAL "")
        set(GLOBAL_LINK_ARGS ${GLOBAL_LINK_ARGS} -l:${LIB}.a PARENT_SCOPE)
    endif()
endforeach ()

set(ALCDEV_STRUCT ALCdevice_struct)
if (OpenAL_VERSION GREATER_EQUAL 1.20.1)
    set(ALCDEV_STRUCT ALCdevice)
endif()

set(GLOBAL_PREPROCESSORS ${GLOBAL_PREPROCESSORS} MKXPZ_ALCDEVICE=${ALCDEV_STRUCT} PARENT_SCOPE)

file(GLOB SRC_INCLUDES LIST_DIRECTORIES ON RELATIVE ${CMAKE_SOURCE_DIR}
        audio
        crypto
        display
        display/gl
        display/libnsgif
        display/libnsgif/utils
        etc
        filesystem
        filesystem/ghc
        input
        net
        singletons
        system
        util
        util/sdl
        util/sigslot
        util/sigslot/adapter PARENT_SCOPE)

set(GLOBAL_INCLUDE_DIRS ${GLOBAL_INCLUDE_DIRS} ${SRC_INCLUDES} PARENT_SCOPE)

list(APPEND GLOBAL_DEPENDENCIES
        OpenAL::OpenAL
        ZLIB::ZLIB
        BZip2::BZip2
        SDL2::Core
        SDL2::Sound
        Pixman::Pixman
        PhysFS::PhysFS
        THEORA::THEORA
        Vorbis::vorbisfile
        Vorbis::vorbis
        OGG::OGG
        SDL2::TTF
        Freetype::Freetype
        SDL2::Image
        PNG::PNG
        Iconv::Iconv
        uchardet::libuchardet
        )

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(GLOBAL_DEPENDENCIES ${GLOBAL_DEPENDENCIES} wsock32 PARENT_SCOPE)
endif()

if (SHARED_FLUID)
    find_package(FluidSynth REQUIRED GLOBAL)
    set(GLOBAL_PREPROCESSORS ${GLOBAL_PREPROCESSORS} SHARED_FLUID PARENT_SCOPE)
    set(GLOBAL_DEPENDENCIES ${GLOBAL_DEPENDENCIES} FluidSynth::FluidSynth PARENT_SCOPE)
    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        find_library(DSOUND_LIBRARY dsound REQUIRED)
        set(GLOBAL_DEPENDENCIES ${GLOBAL_DEPENDENCIES} ${DSOUND_LIBRARY} PARENT_SCOPE)
    endif()
endif()

if (CJK_FALLBACK_FONT)
    set(GLOBAL_PREPROCESSORS ${GLOBAL_PREPROCESSORS} MKXPZ_CJK_FONT PARENT_SCOPE)
endif()

file(GLOB SRC_FILES RELATIVE ${CMAKE_SOURCE_DIR}
        gamestate.cpp
        config.cpp
        eventthread.cpp
        settingsmenu.cpp
        sharedstate.cpp

        audio/alstream.cpp
        audio/audio.cpp
        audio/audiostream.cpp
        audio/fluid-fun.cpp
        audio/midisource.cpp
        audio/sdlsoundsource.cpp
        audio/soundemitter.cpp
        audio/vorbissource.cpp
        theoraplay/theoraplay.c

        crypto/rgssad.cpp

        display/autotiles.cpp
        display/autotilesvx.cpp
        display/bitmap.cpp
        display/font.cpp
        display/graphics.cpp
        display/plane.cpp
        display/sprite.cpp
        display/tilemap.cpp
        display/tilemapvx.cpp
        display/viewport.cpp
        display/window.cpp
        display/windowvx.cpp

        display/libnsgif/libnsgif.c
        display/libnsgif/lzw.c

        display/gl/gl-debug.cpp
        display/gl/gl-fun.cpp
        display/gl/gl-meta.cpp
        display/gl/glstate.cpp
        display/gl/scene.cpp
        display/gl/shader.cpp
        display/gl/texpool.cpp
        display/gl/tileatlas.cpp
        display/gl/tileatlasvx.cpp
        display/gl/tilequad.cpp
        display/gl/vertex.cpp

        util/iniconfig.cpp
        util/win-consoleutils.cpp

        etc/etc.cpp
        etc/table.cpp

        filesystem/filesystem.cpp
        filesystem/filesystemImpl.cpp

        input/input.cpp
        input/keybindings.cpp

        net/LUrlParser.cpp
        net/net.cpp

        singletons/ConfigManager.cpp

        system/systemImpl.cpp)

set(GLOBAL_SOURCES ${GLOBAL_SOURCES} ${SRC_FILES} PARENT_SCOPE)
set(EXE_SOURCES ${EXE_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp PARENT_SCOPE)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/mkxpz_global.h DESTINATION ${CMAKE_SOURCE_DIR}/ext/mkxp_z/include)