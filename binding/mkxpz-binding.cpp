//
// Created by fcors on 10/22/2023.
//

#include <ruby.h>
#include <SDL_sound.h>

#include <thread>

#include "binding-util.h"
#include "binding.h"

#include "gamestate.h"
#include "ConfigManager.h"
#include "sharedstate.h"

class RubyGemBinding : public ScriptBinding {
public:
    void execute() override {
        rb_proc_call(m_proc, Qnil);
    }

    void terminate() override {
        // TODO: Figure out how best to terminate
    }

    void reset() override {
        // TODO: Figure out how to reset
    }

    template<class Function, class... Args >
    void startEventThread(Function&& f, Args&&... args) {
        m_eventThread = std::make_unique<std::jthread>(f, args...);
    }

    void stopEventThread() {
        m_eventThread->request_stop();
    }

    void setRgssThread(std::shared_ptr<RGSSThread> rgssThread) {
        m_rgssThread = std::move(rgssThread);
    }

    void setProc(VALUE proc) {
        m_proc = proc;
    }

    RbData &getRbData() {
        return m_rbData;
    }

private:
    std::unique_ptr<std::jthread> m_eventThread;
    std::shared_ptr<RGSSThread> m_rgssThread;
    VALUE m_proc = Qnil;
    RbData m_rbData;
};

RubyGemBinding rubyGemBinding;
ScriptBinding *scriptBinding = &rubyGemBinding;

void runEventThread(std::string &&windowName, bool windowVisible) {
    GameState::getInstance().initGameState(windowName, windowVisible);
}

RB_METHOD(initGameState) {
    RB_UNUSED_PARAM

    VALUE windowName;
    VALUE visible;
    rb_scan_args(argc, argv, "11", &windowName, &visible);

    if (NIL_P(visible))
        visible = Qtrue;

    std::string appName(rb_string_value_cstr(&windowName));
    bool windowVisible;
    rb_bool_arg(visible, &windowVisible);

    const auto &gs = GameState::getInstance();
    rubyGemBinding.startEventThread(&runEventThread, appName, windowVisible);
    while (!gs.rgssReady())
        std::this_thread::yield();

    auto rgssThread = GameState::getInstance().createRGSSThread();
    rubyGemBinding.setRgssThread(rgssThread);
    shState->setBindingData(&rubyGemBinding.getRbData());

    return Qnil;
}

RB_METHOD(rgssThreadOneShot) {
    RB_UNUSED_PARAM

    VALUE proc;
    rb_scan_args(argc, argv, "&", &proc);

    rubyGemBinding.setProc(proc);
    auto rgssThread = GameState::getInstance().createRGSSThread();
    rgssThread->executeBindings();

    return Qnil;
}

RB_METHOD(initRgssThread) {
    RB_UNUSED_PARAM
    auto rgssThread = GameState::getInstance().createRGSSThread();
    rubyGemBinding.setRgssThread(rgssThread);

    return Qnil;
}

void killGameState(VALUE arg) {
    rubyGemBinding.setRgssThread(nullptr);
    rubyGemBinding.stopEventThread();
    ConfigManager::killInstance();
}

void mkxpzBindingInit() {
    auto mkxpzModule = rb_define_module("MKXP_Z");
    _rb_define_module_function(mkxpzModule, "init_game_state", initGameState);
    _rb_define_module_function(mkxpzModule, "run_rgss_thread", rgssThreadOneShot);
    _rb_define_module_function(mkxpzModule, "init_rgss_thread", initRgssThread);


    rb_set_end_proc(killGameState, 0);
}