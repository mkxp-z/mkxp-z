cmake_minimum_required(VERSION 3.24)

project(mkxp_z VERSION 2.4.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/cmake)


include(GetExternalLibs)

find_package(Ruby REQUIRED)

add_subdirectory(assets)
add_subdirectory(shader)

add_library(mkxp_z SHARED)

add_subdirectory(binding)
add_subdirectory(interfaces)
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


install(TARGETS mkxp_z RUNTIME DESTINATION ${CMAKE_SOURCE_DIR}/../ext/mkxp_z/bin ARCHIVE DESTINATION ${CMAKE_SOURCE_DIR}/../ext/mkxp_z/lib)

set(CMAKE_CXX_COMPILER)

add_executable(test_app test/main.cpp)

target_include_directories(test_app PUBLIC
        ${Ruby_INCLUDE_DIRS})

target_link_libraries(test_app PUBLIC mkxp_z)

get_target_property(ALL_INCLUDES mkxp_z INCLUDE_DIRECTORIES)