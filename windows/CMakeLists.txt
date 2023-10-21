project(MKXPZ_WindowsResourcesX)

target_sources(mkxp_z PUBLIC resource.rc)

target_include_directories(mkxp_z PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        )

target_link_libraries(mkxp_z PUBLIC
        wsock32
        Ws2_32
        )

target_compile_definitions(mkxp_z PUBLIC
        _WIN32
)