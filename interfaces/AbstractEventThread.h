//
// Created by fcors on 10/21/2023.
//

#pragma once

#include <SDL_scancode.h>
#include <SDL_mouse.h>
#include <SDL_mutex.h>
#include <SDL_atomic.h>
#include <SDL_gamecontroller.h>

#include <string>

constexpr int MAX_FINGERS = 4;

class RGSSThreadData;

class AbstractEventThread {
public:
    struct ControllerState {
        int axes[SDL_CONTROLLER_AXIS_MAX];
        bool buttons[SDL_CONTROLLER_BUTTON_MAX];
    };

    struct MouseState {
        int x, y;
        bool inWindow;
        bool buttons[32];
    };

    struct FingerState {
        bool down;
        int x, y;
    };

    struct TouchState {
        FingerState fingers[MAX_FINGERS];
    };

    static uint8_t keyStates[SDL_NUM_SCANCODES];
    static ControllerState controllerState;
    static MouseState mouseState;
    static TouchState touchState;
    static SDL_atomic_t verticalScrollDistance;

    std::string textInputBuffer;

    virtual ~AbstractEventThread();

    virtual void lockText(bool lock) = 0;

    virtual void process(RGSSThreadData &rtData) = 0;

    virtual void cleanup() = 0;

    /* Called from RGSS thread */
    virtual void requestFullscreenMode(bool mode) = 0;

    virtual void requestWindowResize(int width, int height) = 0;

    virtual void requestWindowReposition(int x, int y) = 0;

    virtual void requestWindowCenter() = 0;

    virtual void requestWindowRename(const char *title) = 0;

    virtual void requestShowCursor(bool mode) = 0;

    virtual void requestTextInputMode(bool mode) = 0;

    virtual void requestSettingsMenu() = 0;

    virtual void requestTerminate() = 0;

    virtual bool getFullscreen() const = 0;

    virtual bool getShowCursor() const = 0;

    virtual bool getControllerConnected() const = 0;

    virtual SDL_GameController *controller() const = 0;

    virtual void showMessageBox(const char *body, int flags = 0) = 0;

    /* RGSS thread calls this once per frame */
    virtual void notifyFrame() = 0;

    /* Called on game screen (size / offset) changes */
    virtual void notifyGameScreenChange(const SDL_Rect &screen) = 0;
};
