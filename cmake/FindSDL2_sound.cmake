# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#  Copyright 2019 Amine Ben Hassouna <amine.benhassouna@gmail.com>
#  Copyright 2000-2019 Kitware, Inc. and Contributors
#  All rights reserved.

#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:

#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.

#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.

#  * Neither the name of Kitware, Inc. nor the names of Contributors
#    may be used to endorse or promote products derived from this
#    software without specific prior written permission.

#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#[=======================================================================[.rst:
FindSDL2_sound
------------

Locate SDL2_sound library

This module defines the following 'IMPORTED' target:

::

  SDL2::Sound
    The SDL2_sound library, if found.
    Have SDL2::Core as a link dependency.



This module will set the following variables in your project:

::

  SDL2_SOUND_LIBRARIES, the name of the library to link against
  SDL2_SOUND_INCLUDE_DIRS, where to find the headers
  SDL2_SOUND_FOUND, if false, do not try to link against
  SDL2_SOUND_VERSION_STRING - human-readable string containing the
                            version of SDL2_sound



This module responds to the following cache variables:

::

  SDL2_SOUND_PATH
    Set a custom SDL2_sound Library path (default: empty)

  SDL2_SOUND_NO_DEFAULT_PATH
    Disable search SDL2_sound Library in default path.
      If SDL2_SOUND_PATH (default: ON)
      Else (default: OFF)

  SDL2_SOUND_INCLUDE_DIR
    SDL2_sound headers path.

  SDL2_SOUND_LIBRARY
    SDL2_sound Library (.dll, .so, .a, etc) path.


Additional Note: If you see an empty SDL2_SOUND_LIBRARY in your project
configuration, it means CMake did not find your SDL2_sound library
(SDL2_sound.dll, libsdl2_sound.so, etc). Set SDL2_SOUND_LIBRARY to point
to your SDL2_sound library, and  configure again. This value is used to
generate the final SDL2_SOUND_LIBRARIES variable and the SDL2::Sound target,
but when this value is unset, SDL2_SOUND_LIBRARIES and SDL2::Sound does not
get created.


$SDL2SOUNDDIR is an environment variable that would correspond to the
./configure --prefix=$SDL2SOUNDDIR used in building SDL2_sound.

$SDL2DIR is an environment variable that would correspond to the
./configure --prefix=$SDL2DIR used in building SDL2.



Created by Amine Ben Hassouna:
  Adapt FindSDL_sound.cmake to SDL2_sound (FindSDL2_sound.cmake).
  Add cache variables for more flexibility:
    SDL2_SOUND_PATH, SDL2_SOUND_NO_DEFAULT_PATH (for details, see doc above).
  Add SDL2 as a required dependency.
  Modernize the FindSDL2_sound.cmake module by creating a specific target:
    SDL2::Sound (for details, see doc above).

Original FindSDL_sound.cmake module:
  Created by Eric Wing.  This was influenced by the FindSDL.cmake
  module, but with modifications to recognize OS X frameworks and
  additional Unix paths (FreeBSD, etc).
#]=======================================================================]

# SDL2 Library required
find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
  set(SDL2_SOUND_SDL2_NOT_FOUND "Could NOT find SDL2 (SDL2 is required by SDL2_sound).")
  if(SDL2_sound_FIND_REQUIRED)
    message(FATAL_ERROR ${SDL2_SOUND_SDL2_NOT_FOUND})
  else()
      if(NOT SDL2_sound_FIND_QUIETLY)
        message(STATUS ${SDL2_SOUND_SDL2_NOT_FOUND})
      endif()
    return()
  endif()
  unset(SDL2_SOUND_SDL2_NOT_FOUND)
endif()


# Define options for searching SDL2_sound Library in a custom path

set(SDL2_SOUND_PATH "" CACHE STRING "Custom SDL2_sound Library path")

set(_SDL2_SOUND_NO_DEFAULT_PATH OFF)
if(SDL2_SOUND_PATH)
  set(_SDL2_SOUND_NO_DEFAULT_PATH ON)
endif()

set(SDL2_SOUND_NO_DEFAULT_PATH ${_SDL2_SOUND_NO_DEFAULT_PATH}
    CACHE BOOL "Disable search SDL2_sound Library in default path")
unset(_SDL2_SOUND_NO_DEFAULT_PATH)

set(SDL2_SOUND_NO_DEFAULT_PATH_CMD)
if(SDL2_SOUND_NO_DEFAULT_PATH)
  set(SDL2_SOUND_NO_DEFAULT_PATH_CMD NO_DEFAULT_PATH)
endif()

# Search for the SDL2_sound include directory
find_path(SDL2_SOUND_INCLUDE_DIR SDL_sound.h
  HINTS
    ENV SDL2SOUNDDIR
    ENV SDL2DIR
    ${SDL2_SOUND_NO_DEFAULT_PATH_CMD}
  PATH_SUFFIXES SDL2
                # path suffixes to search inside ENV{SDL2DIR}
                # and ENV{SDL2SOUNDDIR}
                include/SDL2 include
  PATHS ${SDL2_SOUND_PATH}
  DOC "Where the SDL2_sound headers can be found"
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(VC_LIB_PATH_SUFFIX lib/x64)
else()
  set(VC_LIB_PATH_SUFFIX lib/x86)
endif()

# Search for the SDL2_sound library
find_library(SDL2_SOUND_LIBRARY
  NAMES SDL2_sound
  HINTS
    ENV SDL2SOUNDDIR
    ENV SDL2DIR
    ${SDL2_SOUND_NO_DEFAULT_PATH_CMD}
  PATH_SUFFIXES lib ${VC_LIB_PATH_SUFFIX}
  PATHS ${SDL2_SOUND_PATH}
  DOC "Where the SDL2_sound Library can be found"
)

# Read SDL2_sound version
if(SDL2_SOUND_INCLUDE_DIR AND EXISTS "${SDL2_SOUND_INCLUDE_DIR}/SDL_sound.h")
  file(STRINGS "${SDL2_SOUND_INCLUDE_DIR}/SDL_sound.h" SDL2_SOUND_VERSION_MAJOR_LINE REGEX "^#define[ \t]+SDL_SOUND_MAJOR_VERSION[ \t]+[0-9]+$")
  file(STRINGS "${SDL2_SOUND_INCLUDE_DIR}/SDL_sound.h" SDL2_SOUND_VERSION_MINOR_LINE REGEX "^#define[ \t]+SDL_SOUND_MINOR_VERSION[ \t]+[0-9]+$")
  file(STRINGS "${SDL2_SOUND_INCLUDE_DIR}/SDL_sound.h" SDL2_SOUND_VERSION_PATCH_LINE REGEX "^#define[ \t]+SDL_SOUND_PATCHLEVEL[ \t]+[0-9]+$")
  string(REGEX REPLACE "^#define[ \t]+SDL_SOUND_MAJOR_VERSION[ \t]+([0-9]+)$" "\\1" SDL2_SOUND_VERSION_MAJOR "${SDL2_SOUND_VERSION_MAJOR_LINE}")
  string(REGEX REPLACE "^#define[ \t]+SDL_SOUND_MINOR_VERSION[ \t]+([0-9]+)$" "\\1" SDL2_SOUND_VERSION_MINOR "${SDL2_SOUND_VERSION_MINOR_LINE}")
  string(REGEX REPLACE "^#define[ \t]+SDL_SOUND_PATCHLEVEL[ \t]+([0-9]+)$" "\\1" SDL2_SOUND_VERSION_PATCH "${SDL2_SOUND_VERSION_PATCH_LINE}")
  set(SDL2_SOUND_VERSION_STRING ${SDL2_SOUND_VERSION_MAJOR}.${SDL2_SOUND_VERSION_MINOR}.${SDL2_SOUND_VERSION_PATCH})
  unset(SDL2_SOUND_VERSION_MAJOR_LINE)
  unset(SDL2_SOUND_VERSION_MINOR_LINE)
  unset(SDL2_SOUND_VERSION_PATCH_LINE)
  unset(SDL2_SOUND_VERSION_MAJOR)
  unset(SDL2_SOUND_VERSION_MINOR)
  unset(SDL2_SOUND_VERSION_PATCH)
endif()

set(SDL2_SOUND_LIBRARIES ${SDL2_SOUND_LIBRARY})
set(SDL2_SOUND_INCLUDE_DIRS ${SDL2_SOUND_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(SDL2_sound
                                  REQUIRED_VARS SDL2_SOUND_LIBRARIES SDL2_SOUND_INCLUDE_DIRS
                                  VERSION_VAR SDL2_SOUND_VERSION_STRING)


mark_as_advanced(SDL2_SOUND_PATH
                 SDL2_SOUND_NO_DEFAULT_PATH
                 SDL2_SOUND_LIBRARY
                 SDL2_SOUND_INCLUDE_DIR)


if(SDL2_SOUND_FOUND)

  # SDL2::Sound target
  if(SDL2_SOUND_LIBRARY AND NOT TARGET SDL2::Sound)
    add_library(SDL2::Sound UNKNOWN IMPORTED)
    set_target_properties(SDL2::Sound PROPERTIES
                          IMPORTED_LOCATION "${SDL2_SOUND_LIBRARY}"
                          INTERFACE_INCLUDE_DIRECTORIES "${SDL2_SOUND_INCLUDE_DIR}"
                          INTERFACE_LINK_LIBRARIES SDL2::Core)
  endif()
endif()
