//
// Created by fcors on 10/22/2023.
//

#include "gem-binding.h"

#include <ruby.h>
#include <SDL_sound.h>

#include <thread>
#include <memory>
#include <alc.h>

#include "binding-util.h"

#include "sharedstate.h"

extern RGSSThreadData *externThreadData;

std::unique_ptr<std::jthread> eventThread;
ALCcontext *alcCtx = nullptr;

int startGameWindow(int argc, char *argv[], bool showWindow = true);
ALCcontext *startRgssThread(RGSSThreadData *threadData);
int killRgssThread(RGSSThreadData *threadData, ALCcontext *alcCtx);

void stopEventThread();
static void runEventThread(std::string windowName, std::vector<std::string> args, bool windowVisible);

RB_METHOD(initGameState) {
    RB_UNUSED_PARAM

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

    eventThread = std::make_unique<std::jthread>(&runEventThread, appName, argList, windowVisible);
    while (externThreadData == nullptr && eventThread->joinable())
        std::this_thread::yield();

    if (eventThread->joinable()) {
        alcCtx = startRgssThread(externThreadData);
        return Qtrue;
    }

    return Qfalse;
}

void killGameState(VALUE arg) {
    if (externThreadData != nullptr && alcCtx != nullptr)
        killRgssThread(externThreadData, alcCtx);
    stopEventThread();
}

extern "C" {
MKXPZ_GEM_EXPORT void Init_mkxpz() {
    auto mkxpzModule = rb_define_module("MKXP_Z");
    _rb_define_module_function(mkxpzModule, "init_game_state", initGameState);

    rb_set_end_proc(killGameState, 0);
}
}

static void runEventThread(std::string windowName, std::vector<std::string> args, bool windowVisible) {
    std::vector<char *> argv;
    argv.push_back(windowName.data());
    for (auto &a : args) {
        argv.push_back(a.data());
    }
    startGameWindow(argv.size(), argv.data(), windowVisible);
}

void stopEventThread()  {
    if (eventThread != nullptr)
        eventThread->request_stop();
}
