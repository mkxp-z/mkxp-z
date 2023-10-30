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
    execute_process(COMMAND ${XXD_PROGRAM} -i ../shader/${SHADER} ${CMAKE_CURRENT_BINARY_DIR}/${SHADER}.xxd
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

list(APPEND LOCAL_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/common.h)
list(APPEND LOCAL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

propagate_locals_to_global()