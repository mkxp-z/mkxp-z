//
//  system.h
//  Player
//
//  Created by ゾロアーク on 11/22/20.
//

#ifndef system_h
#define system_h

#include <TargetConditionals.h>
#include <string>

/* Platform identifier preserved for the Ruby-visible `System::platform`
 * method. Historically this selected between Windows / macOS / Linux;
 * on the iOS-only fork the value is fixed at runtime to MKXPZ_PLATFORM_IOS.
 * The constant values for Windows/Mac/Linux are kept so game scripts
 * comparing against them still behave sensibly (they'll see IOS and
 * take the "unknown platform" branch, which is the intended behaviour). */
#define MKXPZ_PLATFORM_WINDOWS 0
#define MKXPZ_PLATFORM_MACOS   1
#define MKXPZ_PLATFORM_LINUX   2
#define MKXPZ_PLATFORM_IOS     3

#define MKXPZ_PLATFORM MKXPZ_PLATFORM_IOS

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

#if TARGET_OS_IPHONE
/* Call these on the main thread. The count goes up when the app becomes
 * active or stops being active, when an interruption (a call, Siri)
 * starts or ends, and when the media services of iOS start again. iOS
 * can refuse or allow sound again after each one. */
unsigned soundEventCount();
/* Runs the main run loop until the count is no longer `since`. Returns
 * false at once when the count is still `since` and iOS allows sound
 * now. The check turns the sound session on. */
bool waitForSoundEvent(unsigned since);
#endif
}

std::string getPlistValue(const char *key);

/* Historical settings-window entry point. On iOS this is a no-op
 * (the host app owns its own settings UI) but the symbol is still
 * referenced from eventthread.cpp legacy branches. Definition lives
 * in systemImplIOS.mm. */
void openSettingsWindow();

/* Metal support probe. On iOS this always returns true - every
 * supported device has Metal. Kept as a function so existing call
 * sites (config.cpp) compile without changes. */
bool isMetalSupported();

namespace mkxp_sys = systemImpl;

#endif /* system_h */
