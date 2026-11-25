// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

// Test host shim.
//
// The engine's own main() blocks in mkxp_waitForGamePath() until a
// host names a game folder. A launcher does that when a person taps a
// game. This host has no interface, so it answers straight away with
// the game folder it prepared.
//
// WARNING: do not set the bridge state from the constructor itself.
// The constructor runs before the C++ static initializers in
// app_bridge.cpp, so every value written there is destroyed when those
// statics are constructed. The constructor therefore only queues a
// block on the main queue. mkxp_waitForGamePath pumps the main run
// loop while it waits, so the block runs from inside that wait, long
// after static initialization.
//
// tools/build-test-host-ios.sh copies each game folder into the bundle
// under Games/<name>. The host copies the chosen folder into Documents,
// because the app bundle is read only and the engine writes a path
// cache next to the game. tests/engine holds an mkxp.json with a
// customScript, so the engine runs the suite instead of a game.
//
// These variables pick what the run covers, all read at launch:
//
//   MKXP_TEST_GAME   the folder under Games/. Without it the host
//                    opens "engine" and sets the variable to that, so
//                    the suite can print the folder it runs in.
//   MKXP_TEST_RUBY   the interpreter: 18, 19 or 31. Without it the
//                    engine keeps its own default. A launcher picks
//                    the version per game, so a suite that covers
//                    interpreter behaviour needs the same choice here.
//   MKXP_TEST_SUITE  a file in the game folder to run in place of the
//                    one mkxp.json names.
//   MKXP_TEST_FAST_FORWARD  the fast-forward multiplier.
//   MKXP_TEST_CONFIG JSON that the engine reads on top of mkxp.json,
//                    as a launcher's settings do.
//
// They belong to the launch and not to the build, so one build of
// this host runs every game and suite on every interpreter.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "app_bridge.h"

// An app built with the iOS 27 SDK stops at launch unless Info.plist
// names a scene delegate class.
@interface EngineTestSceneDelegate : UIResponder <UIWindowSceneDelegate>
@end

@implementation EngineTestSceneDelegate
@end

// The app stays installed between runs, so each launch empties
// Documents first. Then no run sees the saves or caches of the run
// before it.
static void clearDocuments(NSString *documents) {
    NSFileManager *fm = NSFileManager.defaultManager;
    for (NSString *item in [fm contentsOfDirectoryAtPath:documents error:nil])
        [fm removeItemAtPath:[documents stringByAppendingPathComponent:item]
                       error:nil];
}

// Copy the suite out of the read only bundle. Returns the writable
// copy, or the bundle folder if the copy fails.
static NSString *prepareGameFolder(NSString *source, NSString *documents) {
    NSFileManager *fm = NSFileManager.defaultManager;
    NSString *target = [documents stringByAppendingPathComponent:@"Game"];

    NSError *error = nil;
    if ([fm copyItemAtPath:source toPath:target error:&error])
        return target;

    NSLog(@"[host] could not copy the game folder: %@", error);
    return source;
}

// The engine reads the suite name out of mkxp.json, so the choice
// has to land in the writable copy before the engine opens it.
static void applyRequestedSuite(NSString *game) {
    const char *want = getenv("MKXP_TEST_SUITE");
    if (!want || !*want)
        return;

    NSString *suite = @(want);
    if (![NSFileManager.defaultManager
            fileExistsAtPath:[game stringByAppendingPathComponent:suite]]) {
        NSLog(@"[host] MKXP_TEST_SUITE=%@ names no file in %@", suite, game);
        return;
    }

    NSString *path = [game stringByAppendingPathComponent:@"mkxp.json"];
    NSData *data = [NSData dataWithContentsOfFile:path];
    id parsed = data ? [NSJSONSerialization JSONObjectWithData:data
                                                       options:0
                                                         error:nil]
                     : nil;
    if (![parsed isKindOfClass:NSDictionary.class]) {
        NSLog(@"[host] %@ holds no JSON object", path);
        return;
    }

    NSMutableDictionary *config = [parsed mutableCopy];
    config[@"customScript"] = suite;

    NSData *out = [NSJSONSerialization dataWithJSONObject:config
                                                  options:0
                                                    error:nil];
    if (!out || ![out writeToFile:path atomically:YES]) {
        NSLog(@"[host] could not write %@", path);
        return;
    }

    NSLog(@"[host] suite: %@", suite);
}

static MKXPRubyVersion requestedRubyVersion(void) {
    const char *want = getenv("MKXP_TEST_RUBY");
    if (!want)
        return MKXP_RUBY_UNSET;
    if (!strcmp(want, "18")) return MKXP_RUBY_18;
    if (!strcmp(want, "19")) return MKXP_RUBY_19;
    if (!strcmp(want, "31")) return MKXP_RUBY_31;
    NSLog(@"[host] MKXP_TEST_RUBY=%s is not 18, 19 or 31", want);
    return MKXP_RUBY_UNSET;
}

static void after(double seconds, dispatch_block_t block) {
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(seconds * NSEC_PER_SEC)),
                   dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), block);
}

// pause_clock.rb and input_repeat.rb show game dialogs whose text is
// a command for this host, to act as a launcher does:
//
//   pause N           pause the engine for N seconds
//   hold N            keep the dialog open for N seconds
//   pause-cancel D S  ask for a pause after D seconds, and cancel it
//                     S seconds later, while the suite draws no frame
//   key C S           hold the key with scancode C for S seconds
//   fast-forward N    set the fast-forward multiplier to N, or back
//                     to the launch value when N is 0
//
// Every other dialog closes at once.
static void answerGameDialog(const char *message, void *userdata) {
    (void)userdata;
    double first = 0, second = 0;
    int scancode = 0, multiplier = 0;
    if (sscanf(message, "key %d %lf", &scancode, &first) == 2) {
        mkxp_signalInfoDismissed();
        mkxp_injectKeyEvent(scancode, 1);
        after(first, ^{ mkxp_injectKeyEvent(scancode, 0); });
    } else if (sscanf(message, "fast-forward %d", &multiplier) == 1) {
        if (multiplier == 0) {
            const char *launch = getenv("MKXP_TEST_FAST_FORWARD");
            multiplier = launch ? atoi(launch) : 1;
        }
        mkxp_setFastForwardMultiplier(multiplier);
        mkxp_signalInfoDismissed();
    } else if (sscanf(message, "pause-cancel %lf %lf", &first, &second) == 2) {
        mkxp_signalInfoDismissed();
        // One timer, because a late second timer would let the engine
        // reach a frame while the pause is still asked for.
        after(first, ^{
            mkxp_requestPause();
            usleep((useconds_t)(second * 1e6));
            mkxp_requestResume();
        });
    } else if (sscanf(message, "pause %lf", &first) == 1) {
        mkxp_requestPause();
        mkxp_signalInfoDismissed();
        after(first, ^{ mkxp_requestResume(); });
    } else if (sscanf(message, "hold %lf", &first) == 1) {
        after(first, ^{ mkxp_signalInfoDismissed(); });
    } else {
        NSLog(@"[host] game dialog: %s", message);
        mkxp_signalInfoDismissed();
    }
}

static void engineTestHostStart(void) {
    @autoreleasepool {
        NSString *resources = NSBundle.mainBundle.resourcePath;
        NSString *documents = NSSearchPathForDirectoriesInDomains(
            NSDocumentDirectory, NSUserDomainMask, YES).firstObject;

        const char *name = getenv("MKXP_TEST_GAME");
        if (!name || !*name) {
            name = "engine";
            setenv("MKXP_TEST_GAME", name, 1);
        }
        NSString *bundled = [[resources stringByAppendingPathComponent:@"Games"]
            stringByAppendingPathComponent:@(name)];
        clearDocuments(documents);
        NSString *game = prepareGameFolder(bundled, documents);
        applyRequestedSuite(game);

        MKXPRubyVersion ruby = requestedRubyVersion();
        if (ruby != MKXP_RUBY_UNSET)
            mkxp_setActiveRubyVersion(ruby);

        const char *fastForward = getenv("MKXP_TEST_FAST_FORWARD");
        if (fastForward)
            mkxp_setFastForwardMultiplier(atoi(fastForward));

        mkxp_setConfigOverlayJSON(getenv("MKXP_TEST_CONFIG"));
        mkxp_setInfoMessageCallback(answerGameDialog, NULL);

        mkxp_setLauncherIdentity("mkxp-z engine tests", NULL);
        mkxp_setUserDataDirectory(documents.fileSystemRepresentation);
        mkxp_setSharedFontsDirectory(
            [resources stringByAppendingPathComponent:@"Assets.bundle/Fonts"]
                .fileSystemRepresentation);
        mkxp_setCABundlePath(
            [resources stringByAppendingPathComponent:@"Assets.bundle/cacert.pem"]
                .fileSystemRepresentation);

        // The engine reads mkxp.json from the managed config directory
        // when a host sets one. Point it at the game folder so the
        // read never depends on the working directory.
        mkxp_setManagedConfigDir(game.fileSystemRepresentation);

        NSLog(@"[host] game folder: %@", game);
        mkxp_setGamePath(game.fileSystemRepresentation);
    }
}

__attribute__((constructor)) static void engineTestHostInit(void) {
    // The Simulator's audio service can stop answering, and CoreAudio
    // then aborts the app: "RPC timeout. Apparently deadlocked." See
    // https://developer.apple.com/forums/thread/738346. OpenAL Soft's
    // null backend mixes in real time with no audio device, so the
    // suites never call CoreAudio.
    setenv("ALSOFT_DRIVERS", "null", 0);
    dispatch_async(dispatch_get_main_queue(), ^{
        engineTestHostStart();
    });
}
