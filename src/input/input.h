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

#include <cstdint>
#include <unordered_map>
#include <string>
#include <vector>

#ifdef MKXPZ_RETRO
enum SDL_GameControllerAxis
{
	SDL_CONTROLLER_AXIS_LEFTX = 0,
	SDL_CONTROLLER_AXIS_LEFTY = 1,
	SDL_CONTROLLER_AXIS_RIGHTX = 2,
	SDL_CONTROLLER_AXIS_RIGHTY = 3,
	SDL_CONTROLLER_AXIS_TRIGGERLEFT = 4,
	SDL_CONTROLLER_AXIS_TRIGGERRIGHT = 5,
};

enum SDL_GameControllerButton
{
	SDL_CONTROLLER_BUTTON_A = 0,
	SDL_CONTROLLER_BUTTON_B = 1,
	SDL_CONTROLLER_BUTTON_X = 2,
	SDL_CONTROLLER_BUTTON_Y = 3,
	SDL_CONTROLLER_BUTTON_BACK = 4,
	SDL_CONTROLLER_BUTTON_GUIDE = 5,
	SDL_CONTROLLER_BUTTON_START = 6,
	SDL_CONTROLLER_BUTTON_LEFTSTICK = 7,
	SDL_CONTROLLER_BUTTON_RIGHTSTICK = 8,
	SDL_CONTROLLER_BUTTON_LEFTSHOULDER = 9,
	SDL_CONTROLLER_BUTTON_RIGHTSHOULDER = 10,
	SDL_CONTROLLER_BUTTON_DPAD_UP = 11,
	SDL_CONTROLLER_BUTTON_DPAD_DOWN = 12,
	SDL_CONTROLLER_BUTTON_DPAD_LEFT = 13,
	SDL_CONTROLLER_BUTTON_DPAD_RIGHT = 14,
	SDL_CONTROLLER_BUTTON_MISC1 = 15,
	SDL_CONTROLLER_BUTTON_PADDLE1 = 16,
	SDL_CONTROLLER_BUTTON_PADDLE2 = 17,
	SDL_CONTROLLER_BUTTON_PADDLE3 = 18,
	SDL_CONTROLLER_BUTTON_PADDLE4 = 19,
	SDL_CONTROLLER_BUTTON_TOUCHPAD = 20,
};

enum SDL_JoystickPowerLevel
{
	SDL_JOYSTICK_POWER_UNKNOWN = 0,
	SDL_JOYSTICK_POWER_EMPTY = 1,
	SDL_JOYSTICK_POWER_LOW = 2,
	SDL_JOYSTICK_POWER_MEDIUM = 3,
	SDL_JOYSTICK_POWER_FULL = 4,
	SDL_JOYSTICK_POWER_WIRED = 5,
};
#else
#include <SDL_gamecontroller.h>
extern std::unordered_map<int, int> vKeyToScancode;
extern std::unordered_map<std::string, int> strToScancode;
extern std::unordered_map<std::string, SDL_GameControllerButton> strToGCButton;
#endif // MKXPZ_RETRO

struct InputPrivate;
struct RGSSThreadData;

#define NUM_INPUT_PORTS 3
#define NUM_BUTTONCODES 43
#define NUM_SCANCODES 291
#define NUM_CONTROLLER_BUTTONS 21

#ifdef MKXPZ_RETRO
enum mkxp_input_retro_mapping_type {
	DEFAULT = 0,
	NONE,
	BUTTON,
	JOYPAD,
	LIGHTGUN,
	MOUSE,
};

struct mkxp_input_retro_binding {
	enum mkxp_input_retro_mapping_type type;
	uint8_t id;
	uint8_t port;
};

extern struct mkxp_input_retro_binding mkxpButtonMapping[NUM_BUTTONCODES];
extern const struct mkxp_input_retro_binding mkxpDefaultButtonMapping[NUM_BUTTONCODES];
extern struct mkxp_input_retro_binding mkxpScancodeMapping[NUM_SCANCODES];
extern const struct mkxp_input_retro_binding mkxpDefaultScancodeMapping[NUM_SCANCODES];
extern struct mkxp_input_retro_binding mkxpControllerMapping[NUM_CONTROLLER_BUTTONS];
extern const struct mkxp_input_retro_binding mkxpDefaultControllerMapping[NUM_CONTROLLER_BUTTONS];
#endif // MKXPZ_RETRO

class Input
{
public:
	enum ButtonCode
	{
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
    
    void recalcRepeat(unsigned int fps);

    double getDelta();
	void update();
    
	bool isPressed(int button);
	bool isTriggered(int button);
	bool isRepeated(int button);
    bool isReleased(int button);
    unsigned int count(int button);
    double repeatTime(int button);
    
    bool isPressedEx(int code, bool isVKey);
    bool isTriggeredEx(int code, bool isVKey);
    bool isRepeatedEx(int code, bool isVKey);
    bool isReleasedEx(int code, bool isVKey);
    unsigned int repeatcount(int code, bool isVKey);
    double repeatTimeEx(int code, bool isVKey);
    
    bool controllerIsPressedEx(int button);
    bool controllerIsTriggeredEx(int button);
    bool controllerIsRepeatedEx(int button);
    bool controllerIsReleasedEx(int button);
    unsigned int controllerRepeatcount(int button);
    double controllerRepeatTimeEx(int button);
    
    uint8_t *rawKeyStates();
    unsigned int rawKeyStatesLength();
    uint8_t *rawButtonStates();
    unsigned int rawButtonStatesLength();
    int16_t *rawAxes();
    unsigned int rawAxesLength();
    
    short getControllerAxisValue(SDL_GameControllerAxis axis);

	int dir4Value();
	int dir8Value();

	int mouseX();
	int mouseY();
    int scrollV();
    bool mouseInWindow();
    
    bool getControllerConnected();
    const char *getControllerName();
    int getControllerPowerLevel();
    
    bool getTextInputMode();
    void setTextInputMode(bool mode);
    const char *getText();
    void clearText();
    
    std::string getClipboardText();
    void setClipboardText(const char *text);
    
    const char *getAxisName(SDL_GameControllerAxis axis);
    const char *getButtonName(SDL_GameControllerButton button);

#ifdef MKXPZ_RETRO
	Input();
	~Input();
private:
#else
private:
	Input(const RGSSThreadData &rtData);
	~Input();

	friend struct SharedStatePrivate;
#endif // MKXPZ_RETRO

	InputPrivate *p;
};

#endif // INPUT_H
