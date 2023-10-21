//
// Created by fcors on 10/20/2023.
//
#pragma once

#if defined _WIN32 || defined __CYGWIN__
#ifdef RGSS_VERSION
#ifdef __GNUC__
#define MKXPZ_EXPORT __attribute__ ((dllexport))
#else
#define MKXPZ_EXPORT __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
#endif
#else
#ifdef __GNUC__
#define MKXPZ_EXPORT __attribute__ ((dllimport))
#else
#define MKXPZ_EXPORT __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
#endif
#endif
#else
#if __GNUC__ >= 4
    #define MKXPZ_EXPORT __attribute__ ((visibility ("default")))
  #else
    #define MKXPZ_EXPORT
  #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

MKXPZ_EXPORT void initBindings();

#ifdef __cplusplus
}
#endif