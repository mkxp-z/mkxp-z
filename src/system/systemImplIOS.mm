// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif
#import "system.h"
#import "app_bridge.h"

std::string systemImpl::getSystemLanguage() {
    @autoreleasepool {
        // Return the ISO 639-1 language code only ("en", "fr",
        // "ja"), NOT the full locale identifier ("en_US"). Game
        // scripts that branch on language (Insurgence's
        // `getRegion`, Reborn's localization shim, etc.) only care
        // about the language. Including the country code splits
        // "en_US" / "en_GB" / "en_AU" into different cohorts which
        // is rarely what those scripts want. NSLocale.languageCode
        // gives us the language code straight from the user's iOS
        // language preference (Settings > General > Language &
        // Region), with `preferredLanguages.firstObject` as a
        // fallback for the rare case where currentLocale's
        // languageCode is nil.
        NSString *lang = NSLocale.currentLocale.languageCode;
        if (lang.length == 0) {
            lang = NSLocale.preferredLanguages.firstObject;
            // preferredLanguages entries can include a region tag
            // (e.g. "en-US" or "zh-Hans-CN") - take only the first
            // component to keep the contract identical.
            if (lang.length != 0) {
                NSRange dash = [lang rangeOfString:@"-"];
                if (dash.location != NSNotFound) {
                    lang = [lang substringToIndex:dash.location];
                }
            }
        }
        return std::string(lang.length != 0 ? lang.UTF8String : "en");
    }
}

std::string systemImpl::getUserName() {
    @autoreleasepool {
        return std::string("Player");
    }
}

static float displayScale() {
#if TARGET_OS_IPHONE
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
        if ([scene isKindOfClass:[UIWindowScene class]]) {
            UIWindowScene *ws = (UIWindowScene *)scene;
            return ws.traitCollection.displayScale;
        }
    }
    return 2.0f;
#else
    return NSScreen.mainScreen ? NSScreen.mainScreen.backingScaleFactor : 2.0f;
#endif
}

int systemImpl::getScalingFactor() {
    return (int)displayScale();
}

bool systemImpl::isWine() {
    return false;
}

bool systemImpl::isRosetta() {
    return false;
}

systemImpl::WineHostType systemImpl::getRealHostType() {
    return WineHostType::Mac;
}

#if TARGET_OS_IPHONE
// UIKit updates applicationState and posts its notifications only when
// the main run loop runs. UIKit can get a move to the background seconds
// after the system moved the app, so a check before the audio open can
// miss the move that makes the open fail.
static void handleQueuedEvents() {
    while (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true) == kCFRunLoopRunHandledSource) {
    }
}

void systemImpl::waitUntilForeground() {
    handleQueuedEvents();
    while (UIApplication.sharedApplication.applicationState == UIApplicationStateBackground)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, true);
}

unsigned systemImpl::backgroundMoveCount() {
    static unsigned moves;
    static id observer = [NSNotificationCenter.defaultCenter
        addObserverForName:UIApplicationDidEnterBackgroundNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification *) {
                  moves++;
                }];
    (void)observer;
    handleQueuedEvents();
    return moves;
}
#endif

void openSettingsWindow() {
    // No settings window on iOS
}

bool isMetalSupported() {
    return true;
}

std::string getPlistValue(const char *key) {
    @autoreleasepool {
        NSString *hash = [[NSBundle mainBundle] objectForInfoDictionaryKey:@(key)];
        if (hash != nil) {
            return std::string(hash.UTF8String);
        }
        return "";
    }
}

float mkxp_getScreenScale(void) {
    // Screen scale is a device constant (e.g. 3.0 on iPhone Pro) that
    // never changes at runtime.  Cache it to avoid dispatch_sync to the
    // main queue on every resize, that call can stall the RGSS thread
    // during rapid rotation while UIKit is processing layout animations.
    static float cachedScale = 0.0f;
    if (cachedScale > 0.0f)
        return cachedScale;

    __block float scale = 2.0f;

    void (^queryBlock)(void) = ^{
        scale = displayScale();
    };

    if ([NSThread isMainThread]) {
        queryBlock();
    } else {
        dispatch_sync(dispatch_get_main_queue(), queryBlock);
    }

    cachedScale = scale;
    return scale;
}
