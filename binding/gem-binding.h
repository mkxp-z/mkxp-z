//
// Created by fcors on 11/8/2023.
//
#pragma once

#if defined(_WIN32)
    #define MKXPZ_GEM_EXPORT  __declspec(dllexport)
#else
    #if defined(__GNUC__) && __GNUC__ >= 4
        #define MKXPZ_GEM_EXPORT  __attribute__((visibility("default")))
    #else
        #define MKXPZ_GEM_EXPORT
    #endif
#endif

#include <memory>
#include <vector>
#include <string>
#include <thread>

struct ALCcontext;
struct RGSSThreadData;

class GemBinding {
private:
    GemBinding();
    ~GemBinding();

public:
    static GemBinding &getInstance();

    void stopEventThread();
    void runEventThread(std::string windowName, std::vector<std::string> args, bool windowVisible);

    inline bool isEventThreadKilled() const {
        return eventThreadKilled;
    }

    inline void setEventThread(std::unique_ptr<std::jthread> &&thread){
        eventThread = std::move(thread);
    }

    inline ALCcontext *getAlcContext() const {
        return alcCtx;
    }

    inline void setAlcContext(ALCcontext *ctx) {
        alcCtx = ctx;
        alcCtxSet = true;
    }

    inline bool alcContextSet() const {
        return alcCtxSet;
    }

private:
    std::unique_ptr<std::jthread> eventThread;
    ALCcontext *alcCtx = nullptr;
    bool alcCtxSet = false;
    bool eventThreadKilled = false;
};