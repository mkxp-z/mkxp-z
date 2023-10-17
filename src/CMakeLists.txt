project(MKXPZ_Core)

add_library(${PROJECT_NAME} STATIC
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

        system/systemImpl.cpp)

target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/assets
        ${CMAKE_SOURCE_DIR}/shader
        ${CMAKE_SOURCE_DIR}/windows
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
        ${CMAKE_CURRENT_SOURCE_DIR}/system
        ${CMAKE_CURRENT_SOURCE_DIR}/util
        ${CMAKE_CURRENT_SOURCE_DIR}/util/sigslot
        ${CMAKE_CURRENT_SOURCE_DIR}/util/sigslot/adapter
        ${EXTERNAL_INCLUDES}
        ${EXTERNAL_INCLUDES}/AL
        ${EXTERNAL_INCLUDES}/fluidsynth
        ${EXTERNAL_INCLUDES}/freetype2
        ${EXTERNAL_INCLUDES}/libpng16
        ${EXTERNAL_INCLUDES}/ogg
        ${EXTERNAL_INCLUDES}/openssl
        ${EXTERNAL_INCLUDES}/pixman-1
        ${EXTERNAL_INCLUDES}/SDL2
        ${EXTERNAL_INCLUDES}/theora
        ${EXTERNAL_INCLUDES}/uchardet
        ${EXTERNAL_INCLUDES}/vorbis)

target_link_libraries(${PROJECT_NAME} PUBLIC
        MKXPZ_Assets
        MKXPZ_Shaders
        ${EXTERNAL_LIBS}/libbrotlicommon-static.a
        ${EXTERNAL_LIBS}/libbrotlidec-static.a
        ${EXTERNAL_LIBS}/libcrypto.a
        ${EXTERNAL_LIBS}/libfluidsynth.a
        ${EXTERNAL_LIBS}/libfreetype.a
        ${EXTERNAL_LIBS}/libhwy.a
        ${EXTERNAL_LIBS}/libjxl_dec-static.a
        ${EXTERNAL_LIBS}/libogg.a
        ${EXTERNAL_LIBS}/libOpenAL32.a
        ${EXTERNAL_LIBS}/libphysfs.a
        ${EXTERNAL_LIBS}/libpixman-1.a
        ${EXTERNAL_LIBS}/libpng.a
        ${EXTERNAL_LIBS}/libpng16.a
        ${EXTERNAL_LIBS}/libSDL2.a
        ${EXTERNAL_LIBS}/libSDL2_image.a
        ${EXTERNAL_LIBS}/libSDL2_sound.a
        ${EXTERNAL_LIBS}/libSDL2_test.a
        ${EXTERNAL_LIBS}/libSDL2_ttf.a
        ${EXTERNAL_LIBS}/libSDL2main.a
        ${EXTERNAL_LIBS}/libssl.a
        ${EXTERNAL_LIBS}/libtheora.a
        ${EXTERNAL_LIBS}/libtheoradec.a
        ${EXTERNAL_LIBS}/libtheoraenc.a
        ${EXTERNAL_LIBS}/libuchardet.a
        ${EXTERNAL_LIBS}/libvorbis.a
        ${EXTERNAL_LIBS}/libvorbisenc.a
        ${EXTERNAL_LIBS}/libvorbisfile.a
        )