//
// Created by fcors on 10/22/2023.
//

#include <ruby.h>
#include <SDL_sound.h>

#include <thread>

#include "binding-util.h"

#include "ThreadManager.h"
#include "ConfigManager.h"

std::unique_ptr<std::jthread> g_eventThread;

void runEventThread(bool windowVisible) {
    auto &tm = ThreadManager::getInstance();
    tm.init(windowVisible);
}

RB_METHOD(initGameState) {
    RB_UNUSED_PARAM

    VALUE visible;
    rb_scan_args(argc, argv, "01", &visible);

    if (NIL_P(visible))
        visible = Qtrue;

    bool windowVisible;
    rb_bool_arg(visible, &windowVisible);

    auto &tm = ThreadManager::getInstance();
    g_eventThread = std::make_unique<std::jthread>(&runEventThread, windowVisible);
    while (!tm.isInitialized())
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    tm.startRgssThread();

    return Qnil;
}

RB_METHOD(startEventLoop) {
    RB_UNUSED_PARAM

    VALUE proc;
    rb_scan_args(argc, argv, "&", &proc);

    auto &tm = ThreadManager::getInstance();
    tm.startEventLoop();

    return Qnil;
}

void killGameState(VALUE arg) {
    g_eventThread->request_stop();
    ThreadManager::killInstance();
    ConfigManager::killInstance();
}

void mkxpzBindingInit() {
    auto mkxpzModule = rb_define_module("MKXP_Z");
    _rb_define_module_function(mkxpzModule, "init_game_state", initGameState);
    _rb_define_module_function(mkxpzModule, "start_event_loop", startEventLoop);

    rb_set_end_proc(killGameState, 0);
}