//
// Created by fcors on 10/22/2023.
//

#include <ruby.h>

#include "binding-util.h"

#include "ThreadManager.h"
#include "DisplayManager.h"
#include "AudioManager.h"
#include "FontManager.h"
#include "InputManager.h"
#include "TimeManager.h"

RB_METHOD(initGameState) {
    RB_UNUSED_PARAM

    ThreadManager::getInstance().init();

    return Qnil;
}

void mkxpzBindingInit() {
    auto mkxpzModule = rb_define_module("MKXP_Z");
    _rb_define_module_function(mkxpzModule, "init_game_state", initGameState);
}