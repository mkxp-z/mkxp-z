project(MKXPZ_Core)

add_library(${PROJECT_NAME} STATIC
        )

target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/windows
        ${COMMON_INCLUDES}
        ${Ruby_INCLUDE_DIRS}
        ${uchardet_INCLUDE_DIRS}/uchardet
        )

target_link_libraries(${PROJECT_NAME} PUBLIC
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
        )