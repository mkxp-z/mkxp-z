# - Try to find Iconv includes dirs and libraries
#
# Usage of this module as follows:
#
#     find_package(Iconv)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
# Variables defined by this module:
#
#  Iconv_FOUND            System has Iconv, include and lib dirs found.
#  Iconv_INCLUDE_DIR      The Iconv includes directories.
#  Iconv_LIBRARY          The Iconv library.

find_path(Iconv_INCLUDE_DIR NAMES iconv.h)
find_library(Iconv_LIBRARY NAMES libiconv.a)

if(Iconv_INCLUDE_DIR AND Iconv_LIBRARY)
    set(Iconv_FOUND TRUE)
    mark_as_advanced(
        Iconv_INCLUDE_DIR
        Iconv_LIBRARY
    )
endif()

if(NOT Iconv_FOUND)
    message(FATAL_ERROR "Iconv doesn't exist")
endif()


add_library(Iconv INTERFACE)
target_link_libraries(Iconv INTERFACE ${Iconv_LIBRARY})
target_include_directories(Iconv INTERFACE ${Iconv_INCLUDE_DIR})
add_library(Iconv::Iconv ALIAS Iconv)