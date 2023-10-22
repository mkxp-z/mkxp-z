//
// Created by fcors on 10/22/2023.
//
#pragma once

#if defined _WIN32 || defined __CYGWIN__
#ifdef MKXPZ_VERSION
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