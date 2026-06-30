//
//  system.h
//  Player
//
//  Created by ゾロアーク on 11/22/20.
//

#ifndef system_h
#define system_h

#include <string>

#define MKXPZ_PLATFORM_WINDOWS 0
#define MKXPZ_PLATFORM_MACOS 1
#define MKXPZ_PLATFORM_LINUX 2

#if defined(__linux__) || defined(MKXPZ_RETRO)
#  define MKXPZ_PLATFORM MKXPZ_PLATFORM_LINUX
#elif defined _WIN32
#  define MKXPZ_PLATFORM MKXPZ_PLATFORM_WINDOWS
#elif defined __APPLE__
#  define MKXPZ_PLATFORM MKXPZ_PLATFORM_MACOS
#else
#  error "Can't identify platform."
#endif

namespace systemImpl {
enum WineHostType {
    Windows,
    Linux,
    Mac
};
std::string getSystemLanguage();
std::string getUserName();
int getScalingFactor();

bool isWine();
bool isRosetta();
WineHostType getRealHostType();
}

#ifdef MKXPZ_BUILD_XCODE
std::string getPlistValue(const char *key);
void openSettingsWindow();
bool isMetalSupported();
#endif

namespace mkxp_sys = systemImpl;

#endif /* system_h */
