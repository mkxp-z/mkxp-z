if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86")
        set(DEPS_ARCH mingw)
    else()
        set(DEPS_ARCH mingw64)
    endif()

    set(DEPS_ROOT ${CMAKE_SOURCE_DIR}/windows/build-${DEPS_ARCH})
    set(DEPS_INCLUDE_DIR ${DEPS_ROOT}/include)
    set(DEPS_LIB_DIR ${DEPS_ROOT}/lib)
    set(DEPS_CMAKE_DIR ${DEPS_LIB_DIR}/cmake)

    set(CMAKE_PREFIX_PATH ${DEPS_LIB_DIR} ${DEPS_INCLUDE_DIR} ${CMAKE_PREFIX_PATH})

    set(SDL2_DIR ${DEPS_CMAKE_DIR}/SDL2)
    set(SDL2_image_DIR ${DEPS_CMAKE_DIR}/SDL2_image)
    set(OpenAL_DIR ${DEPS_CMAKE_DIR}/OpenAL)
    set(PhysFS_DIR ${DEPS_CMAKE_DIR}/PhysFS)

    list(APPEND CMAKE_MODULE_PATH ${DEPS_CMAKE_DIR})
    file(GLOB CMAKE_PATHS LIST_DIRECTORIES ON ${DEPS_CMAKE_DIR}/*)
    foreach(PATH ${CMAKE_PATHS})
        if (IS_DIRECTORY ${PATH})
            list(APPEND CMAKE_MODULE_PATH ${PATH})
        endif()
    endforeach()
endif()

