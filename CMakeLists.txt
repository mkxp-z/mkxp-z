cmake_minimum_required(VERSION 3.24)

project(MKXP-Z LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(conan)

conan_cmake_configure(REQUIRES sdl/2.28.2
        GENERATORS cmake_find_package)

conan_cmake_autodetect(settings)

conan_cmake_install(PATH_OR_REFERENCE .
        BUILD missing
        REMOTE conancenter
        SETTINGS ${settings})

set(EXTERNAL_DEPS ${CMAKE_SOURCE_DIR}/windows/build-mingw64/)
set(EXTERNAL_INCLUDES ${EXTERNAL_DEPS}/include)
set(EXTERNAL_LIBS ${EXTERNAL_DEPS}/lib)

find_package(Ruby REQUIRED)

add_subdirectory(assets)
add_subdirectory(shader)
add_subdirectory(src)
add_subdirectory(binding)

add_executable(MKXP-Z src/main.cpp)

target_link_libraries(MKXP-Z PUBLIC MKXPZ_Core MKXPZ_Bindings MKXPZ_Assets MKXPZ_Shaders)