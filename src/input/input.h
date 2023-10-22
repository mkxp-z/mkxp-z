/*
** input.h
**
** This file is part of mkxp.
**
** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
**
** mkxp is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 of the License, or
** (at your option) any later version.
**
** mkxp is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef INPUT_H
#define INPUT_H

#include <memory>
#include "IInput.h"

extern std::unordered_map<int, int> vKeyToScancode;
extern std::unordered_map<std::string, int> strToScancode;
extern std::unordered_map<std::string, SDL_GameControllerButton> strToGCButton;

struct InputPrivate;
struct RGSSThreadData;

class Input : public IInput {
public:

    void recalcRepeat(unsigned int fps) override;

    double getDelta() override;

    void update() override;

    std::vector<std::string> getBindings(ButtonCode code) override;

    bool isPressed(int button) override;

    bool isTriggered(int button) override;

    bool isRepeated(int button) override;

    bool isReleased(int button) override;

    unsigned int count(int button) override;

    double repeatTime(int button) override;

    bool isPressedEx(int code, bool isVKey) override;

    bool isTriggeredEx(int code, bool isVKey) override;

    bool isRepeatedEx(int code, bool isVKey) override;

    bool isReleasedEx(int code, bool isVKey) override;

    unsigned int repeatcount(int code, bool isVKey) override;

    double repeatTimeEx(int code, bool isVKey) override;

    bool controllerIsPressedEx(int button) override;

    bool controllerIsTriggeredEx(int button) override;

    bool controllerIsRepeatedEx(int button) override;

    bool controllerIsReleasedEx(int button) override;

    unsigned int controllerRepeatcount(int button) override;

    double controllerRepeatTimeEx(int button) override;

    uint8_t *rawKeyStates() override;

    unsigned int rawKeyStatesLength() override;

    uint8_t *rawButtonStates() override;

    unsigned int rawButtonStatesLength() override;

    int16_t *rawAxes() override;

    unsigned int rawAxesLength() override;

    short getControllerAxisValue(SDL_GameControllerAxis axis) override;

    int dir4Value() override;

    int dir8Value() override;

    int mouseX() override;

    int mouseY() override;

    int scrollV() override;

    bool mouseInWindow() override;

    bool getControllerConnected() override;

    const char *getControllerName() override;

    int getControllerPowerLevel() override;

    bool getTextInputMode() override;

    void setTextInputMode(bool mode) override;

    const char *getText() override;

    void clearText() override;

    char *getClipboardText() override;

    void setClipboardText(char *text) override;

    const char *getAxisName(SDL_GameControllerAxis axis) override;

    const char *getButtonName(SDL_GameControllerButton button) override;

private:
    explicit Input(const RGSSThreadData &rtData);

    ~Input() override;

    friend struct SharedStatePrivate;

    friend class InputManager;

    friend std::unique_ptr<Input>::deleter_type;

    std::unique_ptr<InputPrivate> p;
};

#endif // INPUT_H
