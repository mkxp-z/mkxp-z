get_filename_component(COMPILER_ROOT ${CMAKE_CXX_COMPILER} DIRECTORY)
set(NATIVE_INCLUDE_ROOT ${COMPILER_ROOT}/../include)
set(NATIVE_LIB_ROOT ${COMPILER_ROOT}/../lib)
set(NATIVE_LIB_CMAKE_DIR ${NATIVE_LIB_ROOT}/cmake)
list(APPEND CMAKE_MODULE_PATH ${NATIVE_LIB_CMAKE_DIR})
file(GLOB CMAKE_PATHS LIST_DIRECTORIES ON ${NATIVE_LIB_CMAKE_DIR}/*)
foreach(PATH ${CMAKE_PATHS})
    if (IS_DIRECTORY ${PATH})
        list(APPEND CMAKE_MODULE_PATH ${PATH})
    endif()
endforeach()

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(SDL2_sound REQUIRED)
find_package(Ogg REQUIRED)
find_package(PhysFS REQUIRED)
find_package(OpenAL REQUIRED)
find_package(Iconv REQUIRED)
find_package(pixman-1 REQUIRED)
find_package(Vorbis REQUIRED)
find_package(uchardet REQUIRED)
find_package(THEORA REQUIRED)

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