//
// Created by fcors on 10/22/2023.
//

#include "gem-binding.h"
#include "binding-util.h"
#include "rgssthreadmanager.h"
#include "debugwriter.h"

#include <ruby.h>
#include <alc.h>

#include "eventthread.h"

ALCcontextPtr startRgssThread(RGSSThreadData *threadData);

int killRgssThread(RGSSThreadData *threadData);

int startGameWindow(int argc, char *argv[], bool showWindow = true);

void killGameState(VALUE);

RB_METHOD(initGameState) {
    RB_UNUSED_PARAM

    Debug() << "MKXP-Z starting up!";
    VALUE windowName;
    VALUE args;
    VALUE visible;
    rb_scan_args(argc, argv, "21", &windowName, &args, &visible);

    if (NIL_P(visible))
        visible = Qtrue;


    std::string appName(rb_string_value_cstr(&windowName));
    std::vector<std::string> argList;
    if (TYPE(args) == T_ARRAY) {
        long len = rb_array_len(args);
        for (long i = 0; i < len; i++) {
            auto a = rb_ary_entry(args, i);
            if (TYPE(a) == T_STRING) {
                argList.emplace_back(rb_string_value_cstr(&a));
            }
        }
    }
    bool windowVisible;
    rb_bool_arg(visible, &windowVisible);

    auto &gemBinding = GemBinding::getInstance();
    gemBinding.setEventThread(
            std::make_unique<std::jthread>(&GemBinding::runEventThread, &gemBinding, appName, argList, windowVisible));

    const auto &threadManager = RgssThreadManager::getInstance();
    while (threadManager.getThreadData() == nullptr) {
        if (gemBinding.isEventThreadKilled())
            return Qfalse;
        std::this_thread::yield();
    }

    try {
        gemBinding.setAlcContext(startRgssThread(threadManager.getThreadData()));
#ifndef _WIN32
        rb_set_end_proc(killGameState, 0);
#endif
        return Qtrue;
    } catch (const std::system_error &e) {
        Debug() << e.what();
        return Qfalse;
    }
}

void killGameState(VALUE) {
    auto &gemBinding = GemBinding::getInstance();
    if (const auto &threadManager = RgssThreadManager::getInstance(); threadManager.getThreadData() != nullptr) {
        killRgssThread(threadManager.getThreadData());
        gemBinding.clearAlcContext();
    }
    gemBinding.stopEventThread();
}

extern "C" {
MKXPZ_GEM_EXPORT void Init_mkxpz() {
    auto mkxpzModule = rb_define_module("MKXP_Z");
    _rb_define_module_function(mkxpzModule, "init_game_state", initGameState);

#ifdef _WIN32
    rb_set_end_proc(killGameState, 0);
#endif
}
}

GemBinding::GemBinding() : alcCtx(nullptr, alcDestroyContext) {

}

GemBinding::~GemBinding() = default;

GemBinding &GemBinding::getInstance() {
    static GemBinding gemBinding;
    return gemBinding;
}

void GemBinding::stopEventThread() {
    if (eventThread != nullptr && !eventThreadKilled)
        eventThread->join();
}

void GemBinding::runEventThread(std::string windowName, std::vector<std::string> args, bool windowVisible) {
    std::vector<char *> argv;
    argv.push_back(windowName.data());
    for (auto &a : args) {
        argv.push_back(a.data());
    }
    startGameWindow((int) argv.size(), argv.data(), windowVisible);
    eventThreadKilled = true;
}
