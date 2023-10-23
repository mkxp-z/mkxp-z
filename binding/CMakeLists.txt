project(MKXPZ_Bindings)

target_sources(mkxp_z PUBLIC
        binding-mri.cpp
        mkxpz-binding.cpp
        binding-util.cpp
        table-binding.cpp
        etc-binding.cpp
        bitmap-binding.cpp
        font-binding.cpp
        graphics-binding.cpp
        input-binding.cpp
        sprite-binding.cpp
        viewport-binding.cpp
        plane-binding.cpp
        window-binding.cpp
        tilemap-binding.cpp
        audio-binding.cpp
        module_rpg.cpp
        filesystem-binding.cpp
        windowvx-binding.cpp
        tilemapvx-binding.cpp)
        #http-binding.cpp)

target_include_directories(mkxp_z PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${Ruby_INCLUDE_DIRS}
        ${uchardet_INCLUDE_DIRS}/uchardet
        )

target_link_libraries(mkxp_z PUBLIC
        ${Ruby_LIBRARY}
        )

target_compile_definitions(mkxp_z PUBLIC
        SDL_MAIN_HANDLED
        MKXPZ_ALCDEVICE=ALCdevice
        )

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/binding-mri.h DESTINATION ${CMAKE_SOURCE_DIR}/../ext/mkxp_z/include)