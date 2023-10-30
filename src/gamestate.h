//
// Created by fcors on 10/27/2023.
//
#pragma once

#include <memory>

struct RGSSThreadData;
struct ALCcontext;
class Config;
struct ScriptBinding;

using ALCcontext_ptr = std::unique_ptr<ALCcontext, void(*)(ALCcontext*)>;

class RGSSThread {
public:
    explicit RGSSThread(std::shared_ptr<RGSSThreadData> rtData, ALCcontext_ptr &&ctx,
                        ScriptBinding &scriptBinding);
    ~RGSSThread();

    void executeBindings();

private:
    std::shared_ptr<RGSSThreadData> threadData;
    ALCcontext_ptr alcCtx;
    ScriptBinding &m_scriptBinding;
};

class GameState {

private:
    GameState();
    ~GameState();

public:
    static GameState &getInstance();

    void initGameState(int argc, char *argv[], bool showWindow = true);
    bool rgssReady() const;

    std::unique_ptr<RGSSThread> createRGSSThread(ScriptBinding &scriptBinding);

private:
    bool m_rgssReady = false;
    std::unique_ptr<Config> conf;
    std::shared_ptr<RGSSThreadData> rtData;
};