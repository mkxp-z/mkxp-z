project(MKXPZ_Assets)

set(EMBEDED_ASSETS
        icon.png
        gamecontrollerdb.txt)

option(CJK_FALLBACK_FONT "Fallback to a different font" OFF)
if (CJK_FALLBACK_FONT)
    list(APPEND EMBEDED_ASSETS wqymicrohei.ttf)
else()
    list(APPEND EMBEDED_ASSETS liberation.ttf)
endif()


foreach(ASSET ${EMBEDED_ASSETS})
    execute_process(COMMAND xxd -i ../assets/${ASSET} ${CMAKE_CURRENT_BINARY_DIR}/${ASSET}.xxd
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})