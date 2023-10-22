//
// Created by fcors on 10/22/2023.
//

#include <ruby.h>

#include "binding-util.h"

#include "ThreadManager.h"

RB_METHOD(initGameState) {
    RB_UNUSED_PARAM

    auto &tm = ThreadManager::getInstance();
    tm.init();
    tm.startRgssThread();

    return Qnil;
}

RB_METHOD(killGameState) {
    RB_UNUSED_PARAM

    ThreadManager::killThreadManager();

    return Qnil;
}

void mkxpzBindingInit() {
    auto mkxpzModule = rb_define_module("MKXP_Z");
    _rb_define_module_function(mkxpzModule, "init_game_state", initGameState);
    _rb_define_module_function(mkxpzModule, "kill_game_state", killGameState);
}