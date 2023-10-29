if (MRI_INCLUDES STREQUAL "")
    find_package(Ruby REQUIRED GLOBAL)
    if (Ruby_VERSION LESS_EQUAL 1.8)
        set(GLOBAL_PREPROCESSORS ${GLOBAL_PREPROCESSORS} MKXPZ_LEGACY_RUBY PARENT_SCOPE)
    elseif(Ruby_VERSION GREATER_EQUAL 3.0 AND CMAKE_CXX_COMPILER_ID STREQUAL Clang)
        set(GLOBAL_ARGS ${GLOBAL_ARGS} -fdeclspec PARENT_SCOPE)
    endif()
    set(GLOBAL_DEPENDENCIES ${GLOBAL_DEPENDENCIES} Ruby::Ruby PARENT_SCOPE)
else()
    set(GLOBAL_INCLUDE_DIRS ${GLOBAL_INCLUDE_DIRS} ${MRI_INCLUDES} PARENT_SCOPE)
    find_library(RUBY_LIBRARY {MRI_LIBRARY} REQUIRED PATHS ${MRI_LIBPATH})
    set(GLOBAL_DEPENDENCIES ${GLOBAL_DEPENDENCIES} ${RUBY_LIBRARY} PARENT_SCOPE)
endif()

set(BINDING_SOURCE
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
        tilemapvx-binding.cpp
        http-binding.cpp)

if (STEAMWORKS)
    list(APPEND BINDING_SOURCE cusl-binding.cpp)
endif()

if (MINIFFI)
    list(APPEND BINDING_SOURCE
            miniffi-binding.cpp
            miniffi.cpp)
endif()

file(GLOB BINDING_SOURCE_PATHS RELATIVE ${CMAKE_SOURCE_DIR}
        ${BINDING_SOURCE})

set(GLOBAL_SOURCES ${GLOBAL_SOURCES} ${BINDING_SOURCE_PATHS} PARENT_SCOPE)
set(GEM_SOURCES ${GEM_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/mkxpz-binding.cpp PARENT_SCOPE)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/binding-mri.h DESTINATION ${CMAKE_SOURCE_DIR}/ext/mkxp_z/include)