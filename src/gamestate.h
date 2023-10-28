//
// Created by fcors on 10/27/2023.
//
#pragma once

#include <memory>

struct RGSSThreadData;
struct ALCcontext;

using ALCcontext_ptr = std::unique_ptr<ALCcontext, void(*)(ALCcontext*)>;

class RGSSThread {
public:
    explicit RGSSThread(std::shared_ptr<RGSSThreadData> rtData, ALCcontext_ptr &&ctx);
    ~RGSSThread();

    void executeBindings();

private:
    std::shared_ptr<RGSSThreadData> threadData;
    ALCcontext_ptr alcCtx;
};

class GameState {

private:
    GameState();
    ~GameState();

public:
    static GameState &getInstance();

    void initGameState(std::string_view windowName, bool showWindow = true);
    bool rgssReady() const;

    std::shared_ptr<RGSSThread> createRGSSThread();

private:
    bool m_rgssReady = false;
    std::shared_ptr<RGSSThreadData> rtData;
};