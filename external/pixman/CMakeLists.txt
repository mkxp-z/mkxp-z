add_library(pixman-1 INTERFACE)

target_link_libraries(pixman-1 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/lib/libpixman-1.a)

target_include_directories(pixman-1 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/include/pixman-1)