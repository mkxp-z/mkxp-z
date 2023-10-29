if (MRI_INCLUDES STREQUAL "")
    find_package(Ruby REQUIRED GLOBAL)
    set(Ruby_VERSION ${Ruby_VERSION_MAJOR}.${Ruby_VERSION_MINOR}.${Ruby_VERSION_PATCH})
    if (Ruby_VERSION LESS_EQUAL 1.8)
        list(APPEND LOCAL_PREPROCESSORS MKXPZ_LEGACY_RUBY)
    elseif(Ruby_VERSION GREATER_EQUAL 3.0 AND CMAKE_CXX_COMPILER_ID STREQUAL Clang)
        list(APPEND LOCAL_ARGS -fdeclspec)
    endif()
    list(APPEND LOCAL_DEPENDENCIES Ruby::Ruby)
else()
    list(APPEND LOCAL_INCLUDE_DIRS ${MRI_INCLUDES})
    find_library(RUBY_LIBRARY {MRI_LIBRARY} REQUIRED PATHS ${MRI_LIBPATH})
    list(APPEND LOCAL_DEPENDENCIES ${RUBY_LIBRARY})
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

set(BINDING_INCLUDE
        binding-mri.h
        binding-mri-win32.h
        binding-types.h
        binding-util.h
        disposable-binding.h
        flashable-binding.h
        sceneelement-binding.h
        serializable-binding.h
        viewportelement-binding.h)

if (STEAMWORKS)
    list(APPEND BINDING_SOURCE cusl-binding.cpp)
endif()

if (MINIFFI)
    list(APPEND BINDING_SOURCE
            miniffi-binding.cpp
            miniffi.cpp)
    list(APPEND BINDING_INCLUDE miniffi.h)
endif()

file(GLOB BINDING_SOURCE_PATHS RELATIVE ${CMAKE_SOURCE_DIR}
        ${BINDING_SOURCE})
file(GLOB BINDING_INCLUDE_PATHS RELATIVE ${CMAKE_SOURCE_DIR}
        ${BINDING_INCLUDE})

list(APPEND LOCAL_SOURCES ${BINDING_SOURCE_PATHS})
list(APPEND LOCAL_INCLUDES ${BINDING_INCLUDE_PATHS})
set(GEM_SOURCES ${GEM_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/mkxpz-binding.cpp PARENT_SCOPE)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/binding-mri.h DESTINATION ${CMAKE_SOURCE_DIR}/ext/mkxp_z/include)

propagate_locals_to_global()