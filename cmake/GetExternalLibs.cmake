option(DEPS_SHARED_LIBS "Used shared libs (Windows only)" OFF)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND NOT DEPS_SHARED_LIBS)
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
else()
    get_filename_component(COMPILER_ROOT ${CMAKE_CXX_COMPILER} DIRECTORY)
    set(DEPS_INCLUDE_DIR ${COMPILER_ROOT}/../include)
    set(DEPS_LIB_DIR ${COMPILER_ROOT}/../lib)
    set(DEPS_CMAKE_DIR ${DEPS_LIB_DIR}/cmake)
endif()

list(APPEND CMAKE_MODULE_PATH ${DEPS_CMAKE_DIR})
file(GLOB CMAKE_PATHS LIST_DIRECTORIES ON ${DEPS_CMAKE_DIR}/*)
foreach(PATH ${CMAKE_PATHS})
    if (IS_DIRECTORY ${PATH})
        list(APPEND CMAKE_MODULE_PATH ${PATH})
    endif()
endforeach()

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(SDL2_sound REQUIRED)
find_package(OGG REQUIRED)
find_package(PhysFS REQUIRED)
find_package(OpenAL REQUIRED)
find_package(Iconv REQUIRED)
find_package(pixman-1 REQUIRED)
find_package(Vorbis REQUIRED)
find_package(uchardet REQUIRED)
find_package(THEORA REQUIRED)
find_package(FluidSynth REQUIRED)
find_package(Freetype REQUIRED)
find_package(PNG REQUIRED)
find_package(Bzip2 REQUIRED)
find_package(ZLIB REQUIRED)

add_library(PhysFS::PhysFS INTERFACE IMPORTED)
target_include_directories(PhysFS::PhysFS INTERFACE ${PHYSFS_INCLUDE_DIR})
target_link_libraries(PhysFS::PhysFS INTERFACE ${PHYSFS_LIBRARY})

if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND NOT DEPS_SHARED_LIBS)
    add_library(SDL2::Core ALIAS SDL2::SDL2-static)
    add_library(SDL2::Image INTERFACE IMPORTED)
    target_link_libraries(SDL2::Image INTERFACE
            SDL2_image::SDL2_image-static
            SDL2_image::brotlidec-static
            SDL2_image::brotlicommon-static
            SDL2_image::hwy
            SDL2_image::jxl_dec-static)
else()
    add_library(SDL2::Core ALIAS SDL2::SDL2)
    add_library(SDL2::Image ALIAS SDL2_image::SDL2_image)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    install(FILES
            ${COMPILER_ROOT}/libpixman-1-0.dll
            ${COMPILER_ROOT}/libogg-0.dll
            ${COMPILER_ROOT}/libopenal-1.dll
            ${COMPILER_ROOT}/SDL2.dll
            ${COMPILER_ROOT}/SDL2_image.dll
            ${COMPILER_ROOT}/SDL2_ttf.dll
            ${COMPILER_ROOT}/SDL2_sound.dll
            ${COMPILER_ROOT}/libphysfs.dll
            ${COMPILER_ROOT}/libtheora-0.dll
            ${COMPILER_ROOT}/libuchardet.dll
            ${COMPILER_ROOT}/libvorbis-0.dll
            ${COMPILER_ROOT}/libvorbisfile-3.dll
            DESTINATION ${CMAKE_SOURCE_DIR}/../ext/mkxp_z/bin
            )
            endif()