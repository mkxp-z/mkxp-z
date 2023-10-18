cmake_minimum_required(VERSION 3.24)

project(mkxp_z VERSION 2.4.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_SOURCE_DIR}/cmake/conan)

set(EXTERNAL_DEPS ${CMAKE_SOURCE_DIR}/windows/build-mingw64/)
set(EXTERNAL_INCLUDES ${EXTERNAL_DEPS}/include)
set(EXTERNAL_LIBS ${EXTERNAL_DEPS}/lib)

find_package(Ruby REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(Ogg REQUIRED)
find_package(PhysFS REQUIRED)
find_package(OpenAL REQUIRED)
find_package(Vorbis REQUIRED)
find_package(uchardet REQUIRED)
find_package(theora REQUIRED)

add_subdirectory(external)
add_subdirectory(assets)
add_subdirectory(shader)

add_library(mkxp_z SHARED)

add_subdirectory(binding)
add_subdirectory(src)

target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}
        )

target_link_libraries(mkxp_z PUBLIC
        MKXPZ_Assets
        MKXPZ_Shaders)

target_compile_definitions(mkxp_z PUBLIC
        MKXPZ_VERSION="${CMAKE_VERSION}"
        MKXPZ_GIT_HASH="63465f13b5f82b1e5b9f80d1b17972bfe747db20"
        )

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_subdirectory(windows)
endif()
