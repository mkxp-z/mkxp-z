//
// Created by fcors on 11/8/2023.
//
#pragma once

#if defined(_WIN32)
    #define MKXPZ_GEM_EXPORT  __declspec(dllexport)
#else
    #if defined(__GNUC__) && __GNUC__ >= 4
        #define MKXPZ_GEM_EXPORT  __attribute__((visibility("default")))
    #else
        #define MKXPZ_GEM_EXPORT
    #endif
#endif