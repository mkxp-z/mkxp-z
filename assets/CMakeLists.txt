set(EMBEDED_ASSETS
        icon.png
        gamecontrollerdb.txt)

if (CJK_FALLBACK_FONT)
    list(APPEND EMBEDED_ASSETS wqymicrohei.ttf)
else()
    list(APPEND EMBEDED_ASSETS liberation.ttf)
endif()


foreach(ASSET ${EMBEDED_ASSETS})
    execute_process(COMMAND xxd -i ../assets/${ASSET} ${CMAKE_CURRENT_BINARY_DIR}/${ASSET}.xxd
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

list(APPEND LOCAL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

propagate_locals_to_global()