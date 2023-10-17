project(MKXPZ_WindowsResourcesX)

target_sources(MKXP-Z PUBLIC resource.rc)

target_include_directories(MKXP-Z PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        )

target_link_libraries(MKXP-Z PUBLIC
        wsock32
        Ws2_32)

target_compile_definitions(MKXP-Z PUBLIC
        _WIN32
)