project(MKXPZ_Shaders)

set(EMBEDED_SHADERS
        common.h
        transSimple.frag
        trans.frag
        hue.frag
        sprite.frag
        plane.frag
        gray.frag
        bitmapBlit.frag
        flatColor.frag
        simple.frag
        simpleColor.frag
        simpleAlpha.frag
        simpleAlphaUni.frag
        tilemap.frag
        flashMap.frag
        lanczos3.frag
        minimal.vert
        simple.vert
        simpleColor.vert
        sprite.vert
        tilemap.vert
        tilemapvx.vert
        blur.frag
        blurH.vert
        blurV.vert
        simpleMatrix.vert)

foreach(SHADER ${EMBEDED_SHADERS})
    execute_process(COMMAND xxd -i ../shader/${SHADER} ${CMAKE_CURRENT_BINARY_DIR}/${SHADER}.xxd
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})