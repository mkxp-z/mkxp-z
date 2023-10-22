//
// Created by fcors on 10/21/2023.
//

#pragma once

#include <unordered_map>
#include <SDL_gamecontroller.h>
#include <string>
#include <vector>

class IInput {

public:
    enum ButtonCode {
        None = 0,

        Down = 2, Left = 4, Right = 6, Up = 8,

        A = 11, B = 12, C = 13,
        X = 14, Y = 15, Z = 16,
        L = 17, R = 18,

        Shift = 21, Ctrl = 22, Alt = 23,

        F5 = 25, F6 = 26, F7 = 27, F8 = 28, F9 = 29,

        /* Non-standard extensions */
        MouseLeft = 38, MouseMiddle = 39, MouseRight = 40,
        MouseX1 = 41, MouseX2 = 42
    };

    virtual ~IInput() = default;

    virtual void recalcRepeat(unsigned int fps) = 0;

    virtual double getDelta() = 0;

    virtual void update() = 0;

    virtual std::vector<std::string> getBindings(ButtonCode code) = 0;

    virtual bool isPressed(int button) = 0;

    virtual bool isTriggered(int button) = 0;

    virtual bool isRepeated(int button) = 0;

    virtual bool isReleased(int button) = 0;

    virtual unsigned int count(int button) = 0;

    virtual double repeatTime(int button) = 0;

    virtual bool isPressedEx(int code, bool isVKey) = 0;

    virtual bool isTriggeredEx(int code, bool isVKey) = 0;

    virtual bool isRepeatedEx(int code, bool isVKey) = 0;

    virtual bool isReleasedEx(int code, bool isVKey) = 0;

    virtual unsigned int repeatcount(int code, bool isVKey) = 0;

    virtual double repeatTimeEx(int code, bool isVKey) = 0;

    virtual bool controllerIsPressedEx(int button) = 0;

    virtual bool controllerIsTriggeredEx(int button) = 0;

    virtual bool controllerIsRepeatedEx(int button) = 0;

    virtual bool controllerIsReleasedEx(int button) = 0;

    virtual unsigned int controllerRepeatcount(int button) = 0;

    virtual double controllerRepeatTimeEx(int button) = 0;

    virtual uint8_t *rawKeyStates() = 0;

    virtual unsigned int rawKeyStatesLength() = 0;

    virtual uint8_t *rawButtonStates() = 0;

    virtual unsigned int rawButtonStatesLength() = 0;

    virtual int16_t *rawAxes() = 0;

    virtual unsigned int rawAxesLength() = 0;

    virtual short getControllerAxisValue(SDL_GameControllerAxis axis) = 0;

    virtual int dir4Value() = 0;

    virtual int dir8Value() = 0;

    virtual int mouseX() = 0;

    virtual int mouseY() = 0;

    virtual int scrollV() = 0;

    virtual bool mouseInWindow() = 0;

    virtual bool getControllerConnected() = 0;

    virtual const char *getControllerName() = 0;

    virtual int getControllerPowerLevel() = 0;

    virtual bool getTextInputMode() = 0;

    virtual void setTextInputMode(bool mode) = 0;

    virtual const char *getText() = 0;

    virtual void clearText() = 0;

    virtual char *getClipboardText() = 0;

    virtual void setClipboardText(char *text) = 0;

    virtual const char *getAxisName(SDL_GameControllerAxis axis) = 0;

    virtual const char *getButtonName(SDL_GameControllerButton button) = 0;
};
