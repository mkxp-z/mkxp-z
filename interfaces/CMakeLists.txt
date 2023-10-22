project(MKXPZ_Assets)

target_sources(mkxp_z PUBLIC
        AbstractEventThread.cpp)

target_include_directories(mkxp_z PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})