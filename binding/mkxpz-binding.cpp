//
// Created by fcors on 10/22/2023.
//

#include <ruby.h>
#include <SDL_events.h>
#include <SDL_sound.h>
#include <SDL_ttf.h>
#include <SDL_image.h>

#include "binding-util.h"

#include "ThreadManager.h"
#include "debugwriter.h"
#include "ConfigManager.h"

RB_METHOD(initGameState) {
    RB_UNUSED_PARAM;

    VALUE visible;
    rb_scan_args(argc, argv, "01", &visible);

    if (NIL_P(visible))
        visible = Qtrue;

    bool windowVisible;
    rb_bool_arg(visible, &windowVisible);

    auto &tm = ThreadManager::getInstance();
    tm.init(windowVisible);
    tm.startRgssThread();

    return Qnil;
}

RB_METHOD(startEventLoop) {
    RB_UNUSED_PARAM

    auto &tm = ThreadManager::getInstance();
    tm.startEventLoop();

    return Qnil;
}

void killGameState(VALUE arg) {
    ThreadManager::killInstance();
    ConfigManager::killInstance();
}

void mkxpzBindingInit() {
    auto mkxpzModule = rb_define_module("MKXP_Z");
    _rb_define_module_function(mkxpzModule, "init_game_state", initGameState);
    _rb_define_module_function(mkxpzModule, "start_event_loop", startEventLoop);

    rb_set_end_proc(killGameState, 0);
}