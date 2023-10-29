# - Find Uchardet
# Find the Uchardet libraries
#
#  This module defines the following variables:
#     UCHARDET_FOUND        - true if UCHARDET_INCLUDE_DIR & UCHARDET_LIBRARY are found
#     UCHARDET_LIBRARIES    - Set when UCHARDET_LIBRARY is found
#     UCHARDET_INCLUDE_DIRS - Set when UCHARDET_INCLUDE_DIR is found
#
#     UCHARDET_INCLUDE_DIR  - where to find pixman.h, etc.
#     UCHARDET_LIBRARY      - the Pixman library
#

#=============================================================================
# Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#=============================================================================

find_path(UCHARDET_INCLUDE_DIR NAMES uchardet.h PATH_SUFFIXES uchardet)

find_library(UCHARDET_LIBRARY NAMES libuchardet.a)

find_package_handle_standard_args(uchardet DEFAULT_MSG UCHARDET_LIBRARY UCHARDET_INCLUDE_DIR)

if(UCHARDET_FOUND)
    set(UCHARDET_LIBRARIES ${UCHARDET_LIBRARY})
    set(UCHARDET_INCLUDE_DIRS ${UCHARDET_INCLUDE_DIR})
endif()

mark_as_advanced(UCHARDET_INCLUDE_DIR UCHARDET_LIBRARY)

add_library(uchardet::libuchardet INTERFACE IMPORTED)
target_include_directories(uchardet::libuchardet INTERFACE ${UCHARDET_INCLUDE_DIRS})
target_link_libraries(uchardet::libuchardet INTERFACE ${UCHARDET_LIBRARIES})