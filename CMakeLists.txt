cmake_minimum_required(VERSION 3.24)

project(mkxp-z VERSION 2.4.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/cmake)

set(MRI_INCLUDES "" CACHE STRING "Ruby manual include path")
set(MRI_LIBPATH "" CACHE PATH "Ruby manual lib path")
set(MRI_LIBRARY "" CACHE FILEPATH "Ruby manual link name")
option(FORCE32 "Force compiler to build a 32-bit executable" OFF)

option(CXX11_EXPERIMENTAL "Toggles between using ghc/filesystem or C++11 <experimental/filesystem>" OFF)
option(SHARED_FLUID "Dynamically link fluidsynth at build time" ON)
option(CJK_FALLBACK_FONT "Use WenQuanYi Micro Hei as the fallback font" OFF)
option(USE_MINIFFI "Enable MiniFFI Ruby module (Win32API)" ON)
option(ENABLE_HTTPS "Support HTTPS for get/post requests. Requires OpenSSL." ON)
option(WORKDIR_CURRENT "Keep current directory on startup" OFF)

option(STATIC_EXECUTABLE "Build a static executable (Windows-only)" ON)
set(APPIMAGEKIT_PATH "" CACHE PATH "Path to AppImageTool, used for building AppImages")
option("APPIMAGE" OFF "Whether to install to an AppImage or just copy everything")
option(USE_STEAMWORKS OFF "Should this library link against Steamworks")
set(STEAM_APPID "" CACHE STRING "Steam AppID. Set this to use SteamAPI_RestartAppIfNecessary")
option(STEAMSHIM_DEBUG "Whether to show Steamshim debug messages in the console" OFF)

set(GFX_BACKEND GL CACHE STRING "Graphics rendering API to use.")
set_property(CACHE GFX_BACKEND PROPERTY STRINGS GL GLES)

if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    message(FATAL_ERROR "This CMake project does not support macOS. Please use the Xcode project instead.")
endif()

find_program(XXD_PROGRAM xxd)

execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(GLOBAL_SOURCES "")
set(GLOBAL_INCLUDES "")
set(GLOBAL_DEPENDENCIES "")
set(GLOBAL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR})
set(GLOBAL_PREPROCESSORS "")
set(GLOBAL_ARGS "")
set(GLOBAL_LINK_ARGS "")

set(GEM_SOURCES "")
set(EXE_SOURCES "")

set(WIN64 OFF)
if (NOT CMAKE_SIZEOF_VOIDP EQUAL CMAKE_SIZEOF_LONG)
    set(WIN64 ON)
endif()

list(APPEND GLOBAL_PREPROCESSORS
        MKXPZ_BUILD_CMAKE
        MKXPZ_VERSION="${CMAKE_PROJECT_VERSION}"
        MKXPZ_GIT_HASH="${GIT_HASH}"
        HAVE_NANOSLEEP
        )

# ====================
# Ext libs
# ====================
set(STEAMWORKS OFF)
if(USE_STEAMWORKS)
    find_package(Steamworks)
    if (Steamworks_FOUND)
        list(APPEND GLOBAL_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/steamshim)
        list(APPEND GLOBAL_PREPROCESSORS MKXPZ_STEAM)
        list(APPEND GLOBAL_SOURCES ${CMAKE_SOURCE_DIR}/steamshim/steamshim_child.c)
        set(STEAMWORKS ON)
    endif()
endif()

if (GFX_BACKEND STREQUAL "GLES")
    list(APPEND GLOBAL_PREPROCESSORS GLES2_HEADER)
elseif (GFX_BACKEND STREQUAL "GL")
    find_package(OpenGL REQUIRED)
    list(APPEND GLOBAL_DEPENDENCIES OpenGL::GL)
endif()

# ====================
# Main source
# ====================
if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    if (NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        list(APPEND GLOBAL_ARGS -masm=intel)
    endif()
endif()

if (USE_MINIFFI)
    list(APPEND GLOBAL_PREPROCESSORS MKXPZ_MINIFFI)
endif()

if (WORKDIR_CURRENT)
    list(APPEND GLOBAL_PREPROCESSORS WORKDIR_CURRENT)
endif()

if (CXX11_EXPERIMENTAL)
    list(APPEND GLOBAL_PREPROCESSORS MKXPZ_EXP_FS)
endif()

if (FORCE32)
    list(APPEND GLOBAL_ARGS -m32)
endif()

list(APPEND GLOBAL_PREPROCESSORS MKXPZ_INIT_GL_LATER)

include(LocalToGlobalConverter)
add_subdirectory(src)
add_subdirectory(binding)
add_subdirectory(shader)
add_subdirectory(assets)

list(APPEND GLOBAL_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/binding)

set(RPATH "")

if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")

    set(CMAKE_RC_COMPILER_INIT windres)
    enable_language(RC)
    set(CMAKE_RC_ARCHIVE_CREATE "")
    set(CMAKE_RC_ARCHIVE_FINISH "")
    set(CMAKE_RC_COMPILE_OBJECT
            "<CMAKE_RC_COMPILER> <FLAGS> -O coff <DEFINES> -i <SOURCE> -o <OBJECT>")

    add_subdirectory(windows)
    list(APPEND GLOBAL_SOURCES ${WINDOWS_RESOURCES})
    list(APPEND GLOBAL_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/windows)
else()
    add_subdirectory(linux)
    set(RPATH $env{ORIGIN}/lib)
    if (NOT APPIMAGE)
        if (CMAKE_SIZEOF_LONG EQUAL 8 AND NOT FORCE32)
            set(RPATH ${RPATH}64)
        else()
            set(RPATH ${RPATH}32)
        endif()
    endif()
endif()

set(EXE_NAME ${CMAKE_PROJECT_NAME})

if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND NOT APPIMAGE)
    set(EXE_NAME ${EXE_NAME}.${CMAKE_HOST_SYSTEM_PROCESSOR})
endif()

if (STEAMWORKS)
    set(EXE_NAME steam_${EXE_NAME})
    set(LA "")
    if (STATIC_EXECUTABLE)
        if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
            list(APPEND LA -static)
        else()
            list(APPEND LA -static-libgcc -static-libstdc++)
        endif()
    endif()

    # TODO: Figure out how to configure the Steamworks -I argument
    set(SHIM_PREPROCESSORS GAME_LAUNCH_NAME="${EXE_NAME}")

    if (NOT STEAM_APPID STREQUAL "")
        list(APPEND SHIM_PREPROCESSORS STEAM_APPID=${STEAM_APPID})
    endif()

    if (STEAMSHIM_DEBUG)
        list(APPEND SHIM_PREPROCESSORS STEAMSHIM_DEBUG)
        set(SHIM_WS console)
    else()
        set(SHIM_WS windows)
    endif()

    add_executable(${CMAKE_PROJECT_NAME} steamshim/steamshim_parent.cpp)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Steamworks::Steamworks)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE ${SHIM_PREPROCESSORS})
    target_link_options(${CMAKE_PROJECT_NAME} PRIVATE ${LA})
    if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -subsystems,${SHIM_WS})
    else()
        install(TARGETS ${CMAKE_PROJECT_NAME} RUNTIME)
    endif()
endif()


add_library(${CMAKE_PROJECT_NAME}-common STATIC ${GLOBAL_SOURCES} ${GLOBAL_INCLUDES})
target_link_libraries(${CMAKE_PROJECT_NAME}-common PUBLIC ${GLOBAL_DEPENDENCIES})
target_include_directories(${CMAKE_PROJECT_NAME}-common PUBLIC ${GLOBAL_INCLUDE_DIRS})
target_compile_definitions(${CMAKE_PROJECT_NAME}-common PUBLIC ${GLOBAL_PREPROCESSORS})
target_compile_options(${CMAKE_PROJECT_NAME}-common PUBLIC ${GLOBAL_ARGS})
target_link_options(${CMAKE_PROJECT_NAME}-common PUBLIC ${GLOBAL_LINK_ARGS})

add_library(${CMAKE_PROJECT_NAME}-gem SHARED ${GEM_SOURCES})
target_link_libraries(${CMAKE_PROJECT_NAME}-gem PRIVATE ${CMAKE_PROJECT_NAME}-common)
set_target_properties(${CMAKE_PROJECT_NAME}-gem PROPERTIES OUTPUT_NAME mkxp_z)
install(TARGETS ${CMAKE_PROJECT_NAME}-gem RUNTIME DESTINATION ${CMAKE_SOURCE_DIR}/ext/mkxp_z/bin ARCHIVE DESTINATION ${CMAKE_SOURCE_DIR}/ext/mkxp_z/lib)

add_executable(${EXE_NAME} ${EXE_SOURCES})
target_link_libraries(${EXE_NAME} PRIVATE ${CMAKE_PROJECT_NAME}-common)
if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    target_link_options(${EXE_NAME} PRIVATE -subsystem,windows)
else()
    install(${EXE_NAME} RUNTIME DESTINATION ${RPATH})
endif()