project(MKXPZ_Gem)

add_library(mkxp_z SHARED mkxp_z.h mkxp_z.c)

target_include_directories(mkxp_z PRIVATE ${CMAKE_SOURCE_DIR}/binding ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(mkxp_z PRIVATE ${CMAKE_PROJECT_NAME}-gem Ruby::Ruby)

set_target_properties(mkxp_z PROPERTIES PREFIX "")
set_target_properties(mkxp_z PROPERTIES SUFFIX ".so")