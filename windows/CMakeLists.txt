# TODO: Figure out if this compiles the resources correctly
set(CMAKE_RC_COMPILER_INIT windres)
enable_language(RC)
set(CMAKE_RC_COMPILE_OBJECT
        "<CMAKE_RC_COMPILER> <FLAGS> -O coff <DEFINES> -i <SOURCE> -o <OBJECT>")

set(WINDOWS_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resource.rc PARENT_SCOPE)