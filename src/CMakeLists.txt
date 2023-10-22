project(MKXPZ_Core)

target_sources(mkxp_z PUBLIC
        #main.cpp
        gamelauncher.cpp
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

        util/sdl/SDL_Instance.cpp

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

        singletons/AudioManager.cpp
        singletons/ConfigManager.cpp
        singletons/DisplayManager.cpp
        singletons/InputManager.h
        singletons/ThreadManager.cpp
        singletons/TimeManager.cpp

        system/systemImpl.cpp)

target_include_directories(mkxp_z PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/audio
        ${CMAKE_CURRENT_SOURCE_DIR}/crypto
        ${CMAKE_CURRENT_SOURCE_DIR}/display
        ${CMAKE_CURRENT_SOURCE_DIR}/display/gl
        ${CMAKE_CURRENT_SOURCE_DIR}/display/libnsgif
        ${CMAKE_CURRENT_SOURCE_DIR}/display/libnsgif/utils
        ${CMAKE_CURRENT_SOURCE_DIR}/etc
        ${CMAKE_CURRENT_SOURCE_DIR}/filesystem
        ${CMAKE_CURRENT_SOURCE_DIR}/filesystem/ghc
        ${CMAKE_CURRENT_SOURCE_DIR}/input
        ${CMAKE_CURRENT_SOURCE_DIR}/net
        ${CMAKE_CURRENT_SOURCE_DIR}/singletons
        ${CMAKE_CURRENT_SOURCE_DIR}/system
        ${CMAKE_CURRENT_SOURCE_DIR}/util
        ${CMAKE_CURRENT_SOURCE_DIR}/util/sdl
        ${CMAKE_CURRENT_SOURCE_DIR}/util/sigslot
        ${CMAKE_CURRENT_SOURCE_DIR}/util/sigslot/adapter
        ${Ruby_INCLUDE_DIRS}
        ${uchardet_INCLUDE_DIRS}/uchardet
        )

target_link_libraries(mkxp_z PUBLIC
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
        )

target_compile_definitions(mkxp_z PUBLIC
        SDL_MAIN_HANDLED
        MKXPZ_ALCDEVICE=ALCdevice
        )