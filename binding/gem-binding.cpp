//
// Created by fcors on 10/22/2023.
//

#include <ruby.h>
#include <SDL_sound.h>

#include <thread>
#include <memory>

#include "binding-util.h"
#include "gem-binding.h"

#include "gamestate.h"
#include "sharedstate.h"
#include "binding.h"
#include "binding-mri.h"

class RubyGemBinding : public ScriptBinding {
public:
    void execute() final;
    void terminate() final;
    void reset() final;

    template<class Function, class... Args >
    void startEventThread(Function&& f, Args&&... args);

    void stopEventThread();
    void setRgssThread(std::unique_ptr<RGSSThread> &&rgssThread);

    RbData &getRbData();

    static RubyGemBinding instance;

private:
    std::unique_ptr<std::jthread> m_eventThread;
    std::shared_ptr<RGSSThread> m_rgssThread;
    RbData m_rbData;
};

static void runEventThread(std::string &&windowName, std::vector<std::string> &&args, bool windowVisible);

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

    const auto &gs = GameState::getInstance();
    RubyGemBinding::instance.startEventThread(&runEventThread, appName, argList, windowVisible);
    while (!gs.rgssReady())
        std::this_thread::yield();

    auto rgssThread = GameState::getInstance().createRGSSThread(RubyGemBinding::instance);
    RubyGemBinding::instance.setRgssThread(std::move(rgssThread));
    shState->setBindingData(&RubyGemBinding::instance.getRbData());

    return Qnil;
}

void killGameState(VALUE arg) {
    RubyGemBinding::instance.setRgssThread(nullptr);
    RubyGemBinding::instance.stopEventThread();
}

extern "C" {
MKXPZ_EXPORT void mkxpzBindingInit() {
    auto mkxpzModule = rb_define_module("MKXP_Z");
    _rb_define_module_function(mkxpzModule, "init_game_state", initGameState);

    rb_set_end_proc(killGameState, 0);
}
}

static void runEventThread(std::string &&windowName, std::vector<std::string> &&args, bool windowVisible) {
    std::vector<char *> argv;
    argv.push_back(windowName.data());
    for (auto &a : args) {
        argv.push_back(a.data());
    }
    GameState::getInstance().initGameState(argv.size(), argv.data(), windowVisible);
}


RubyGemBinding RubyGemBinding::instance;

template<class Function, class... Args >
void RubyGemBinding::startEventThread(Function&& f, Args&&... args) {
m_eventThread = std::make_unique<std::jthread>(f, args...);
}

void RubyGemBinding::stopEventThread()  {
    m_eventThread->request_stop();
}

void RubyGemBinding::setRgssThread(std::unique_ptr<RGSSThread> &&rgssThread) {
    m_rgssThread = std::move(rgssThread);
}

RbData &RubyGemBinding::getRbData() {
    return m_rbData;
}

void RubyGemBinding::execute() {
    mriBindingInit();
}

void RubyGemBinding::terminate() {

}

void RubyGemBinding::reset() {

}
