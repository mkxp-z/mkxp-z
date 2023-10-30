# Locate Steamworks library
# This module defines
# STEAMWORKS_LIBRARY, the name of the library to link against
# STEAMWORKS_INCLUDE_DIR, the directory containing steam/steam_api.h
# STEAMWORKS_FOUND, if false, do not try to link to Steamworks
#
# If a path is set in the SteamworksDIR environment variable it will check there
#
# This has been tested on OS X
# This may or may not with Linux - it should, but i think it could be broken easily
#

SET(STEAMWORKS_SEARCH_PATHS
		~/Library/Frameworks
		/Library/Frameworks
		/usr/local
		/usr
		/sw # Fink
		/opt/local # DarwinPorts
		/opt/csw # Blastwave
		/opt
		)

SET (STEAMWORKS_SEARCH_PREFIXES
		lib
		lib64
		)

IF (APPLE)
	SET (STEAMWORKS_SEARCH_PREFIXES ${STEAMWORKS_SEARCH_PREFIXES}
			redistributable_bin/osx32/
			)
ELSEIF (UNIX)
	IF( CMAKE_SYSTEM_PROCESSOR MATCHES "i.86" )
		SET( BUILD_ARCH "32" )
	ELSEIF( CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
		SET( BUILD_ARCH "64" )
	ENDIF()
	SET (STEAMWORKS_SEARCH_PREFIXES ${STEAMWORKS_SEARCH_PREFIXES} redistributable_bin/linux${BUILD_ARCH}/ )
ENDIF()

FIND_PATH(STEAMWORKS_INCLUDE_DIR steam/steam_api.h
		HINTS
		$ENV{SteamworksDIR}
		PATH_SUFFIXES public/ include/ Include/
		PATHS ${STEAMWORKS_SEARCH_PATHS}
		)

FIND_LIBRARY(STEAMWORKS_LIBRARY_TEMP
		NAMES steam_api
		HINTS
		$ENV{SteamworksDIR}
		PATH_SUFFIXES ${STEAMWORKS_SEARCH_PREFIXES}
		PATHS ${STEAMWORKS_SEARCH_PATHS}
		)

IF(STEAMWORKS_LIBRARY_TEMP)
	# Set the final string here so the GUI reflects the final state.
	SET(STEAMWORKS_LIBRARY ${STEAMWORKS_LIBRARY_TEMP} CACHE STRING "Where the Steamworks Library can be found")
	# Set the temp variable to INTERNAL so it is not seen in the CMake GUI
	SET(STEAMWORKS_LIBRARY_TEMP "${STEAMWORKS_LIBRARY_TEMP}" CACHE INTERNAL "")
ENDIF(STEAMWORKS_LIBRARY_TEMP)

INCLUDE(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(Steamworks REQUIRED_VARS STEAMWORKS_LIBRARY STEAMWORKS_INCLUDE_DIR)

if(Steamworks_FOUND)

endif()