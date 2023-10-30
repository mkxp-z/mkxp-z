include(DependencySetup)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    find_package(PhysFS REQUIRED GLOBAL NO_CMAKE_SYSTEM_PACKAGE_REGISTRY )
    find_package(OpenAL REQUIRED GLOBAL NO_CMAKE_SYSTEM_PACKAGE_REGISTRY )
else()
    find_package(PhysFS REQUIRED GLOBAL)
    find_package(OpenAL REQUIRED GLOBAL)
endif()
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
add_library(PhysFS::PhysFS ALIAS PhysFS::PhysFS-static)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CMAKE_FIND_PACKAGE_NO_SYSTEM_PACKAGE_REGISTRY ${OLD_CMAKE_FIND_PACKAGE_NO_SYSTEM_PACKAGE_REGISTRY})
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

find_package(BZip2 REQUIRED GLOBAL)
find_package(Iconv REQUIRED GLOBAL)
if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    find_library(CHARSET_LIB charset REQUIRED)
    list(APPEND LOCAL_DEPENDENCIES ${CHARSET_LIB})
endif()

if (ENABLE_HTTPS)
    find_package(OpenSSL GLOBAL)
    if (OpenSSL_FOUND)
        list(APPEND LOCAL_DEPENDENCIES OpenSSL::SSL)
        list(APPEND LOCAL_PREPROCESSORS MKXPZ_SSL)
        if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
            list(APPEND LOCAL_DEPENDENCIES crypt32)
        endif()
    else()
        message(WARNING "Could not locate OpenSSL. HTTPS will be disabled.")
    endif()
endif()

set(EXPLICIT_LIBS "")
if (CMAKE_HOST_SYSTEM_NAME STREQUAL WINDOWS)
    # TODO: Determine if the bit about the explicit libs is really necessary
    list(APPEND EXPLICIT_LIBS libucrt libgcc libmingwex libgmp)
endif()

if (STATIC_EXECUTABLE)
    if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        list(APPEND LOCAL_LINK_ARGS -static-libgcc -static-libstdc++ -Wl,-Bstatic -lgcc -lstdc++ -lpthread -Wl,-Bdynamic)
    else()
        list(APPEND LOCAL_LINK_ARGS -static-libgcc -static-libstdc++)
    endif()
    list(APPEND LOCAL_PREPROCESSORS AL_LIBTYPE_STATIC)
endif()

foreach(LIB ${EXPLICIT_LIBS})
    if (NOT LIB STREQUAL "")
        list(APPEND LOCAL_LINK_ARGS -l:${LIB}.a)
    endif()
endforeach ()

set(ALCDEV_STRUCT ALCdevice_struct)
if (OPENAL_VERSION_STRING GREATER_EQUAL 1.20.1)
    set(ALCDEV_STRUCT ALCdevice)
endif()

list(APPEND LOCAL_PREPROCESSORS MKXPZ_ALCDEVICE=${ALCDEV_STRUCT})

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

list(APPEND LOCAL_INCLUDE_DIRS ${SRC_INCLUDES})

list(APPEND LOCAL_DEPENDENCIES
        OpenAL::OpenAL
        ZLIB::ZLIB
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
        BZip2::BZip2
        SDL2::Image
        PNG::PNG
        Iconv::Iconv
        uchardet::libuchardet
        )

if (SHARED_FLUID)
    find_package(FluidSynth REQUIRED GLOBAL)
    list(APPEND LOCAL_PREPROCESSORS SHARED_FLUID FLUIDSYNTH_NOT_A_DLL)
    list(APPEND LOCAL_DEPENDENCIES FluidSynth::FluidSynth)
    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        list(APPEND LOCAL_DEPENDENCIES dsound winmm)
    endif()
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND LOCAL_DEPENDENCIES wsock32 Ws2_32)
endif()

if (CJK_FALLBACK_FONT)
    list(APPEND LOCAL_PREPROCESSORS MKXPZ_CJK_FONT)
endif()

file(GLOB SRC_FILES RELATIVE ${CMAKE_SOURCE_DIR}
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



file(GLOB_RECURSE SRC_HEADERS RELATIVE ${CMAKE_SOURCE_DIR}
        binding.h
        config.h
        eventthread.h
        settingsmenu.h
        sharedstate.h

        audio/al-util.h
        audio/aldatasource.h
        audio/alstream.h
        audio/audio.h
        audio/audiostream.h
        audio/fluid-fun.h
        audio/sharedmidistate.h
        audio/soundemitter.h

        crypto/rgssad.h

        display/bitmap.h
        display/flashable.h
        display/font.h
        display/gl/gl-debug.h
        display/gl/gl-fun.h
        display/gl/gl-meta.h
        display/gl/gl-util.h
        display/gl/global-ibo.h
        display/gl/glstate.h
        display/gl/quad.h
        display/gl/quadarray.h
        display/gl/scene.h
        display/gl/shader.h
        display/gl/texpool.h
        display/gl/tileatlas.h
        display/gl/tileatlasvx.h
        display/gl/tilequad.h
        display/gl/transform.h
        display/gl/vertex.h
        display/graphics.h
        display/libnsgif/libnsgif.h
        display/libnsgif/lzw.h
        display/libnsgif/utils/log.h
        display/plane.h
        display/sprite.h
        display/tilemap-common.h
        display/tilemap.h
        display/tilemapvx.h
        display/viewport.h
        display/window.h
        display/windowvx.h

        etc/etc-internal.h
        etc/etc.h
        etc/table.h

        filesystem/filesystem.h
        filesystem/filesystemImpl.h

        input/input.h
        input/keybindings.h

        net/LUrlParser.h
        net/httplib.h
        net/net.h

        singletons/ConfigManager.h

        system/system.h

        theoraplay/theoraplay.h
        theoraplay/theoraplay_cvtrgb.h

        util/boost-hash.h
        util/debugwriter.h
        util/disposable.h
        util/encoding.h
        util/exception.h
        util/iniconfig.h
        util/intrulist.h
        util/sdl-util.h
        util/serial-util.h
        util/serializable.h
        util/string-util.h
        util/util.h
        util/win-consoleutils.h)

list(APPEND LOCAL_SOURCES ${SRC_FILES})
list(APPEND LOCAL_INCLUDES ${SRC_HEADERS})
set(EXE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp PARENT_SCOPE)
set(GEM_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/mkxpz_global.h
        ${CMAKE_CURRENT_SOURCE_DIR}/gamestate.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/gamestate.h PARENT_SCOPE)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/mkxpz_global.h DESTINATION ${CMAKE_SOURCE_DIR}/ext/mkxp_z/include)

propagate_locals_to_global()