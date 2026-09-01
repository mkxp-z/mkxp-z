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
#define MKXPZ_PLATFORM_BSD 3

#ifdef __WIN32__
#define MKXPZ_PLATFORM MKXPZ_PLATFORM_WINDOWS
#elif defined __APPLE__
#define MKXPZ_PLATFORM MKXPZ_PLATFORM_MACOS
#elif defined __linux__
#define MKXPZ_PLATFORM MKXPZ_PLATFORM_LINUX
#elif defined __DragonFly__ || defined __FreeBSD__ || defined __NetBSD__ || defined __OpenBSD__
#define MKXPZ_PLATFORM MKXPZ_PLATFORM_BSD
#else
#error "Can't identify platform."
#endif

namespace systemImpl {
enum WineHostType {
    Windows,
    Linux,
    Mac,
    Bsd,
};
std::string getSystemLanguage();
std::string getUserName();
int getScalingFactor();

bool isWine();
bool isRosetta();
WineHostType getRealHostType();
}

#ifdef __APPLE__
void openSettingsWindow();
#endif

namespace mkxp_sys = systemImpl;

#endif /* system_h */
