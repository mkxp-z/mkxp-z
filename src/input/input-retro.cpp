/*
 ** input-retro.cpp
 **
 ** This file is part of mkxp.
 **
 ** Copyright (C) 2025 - 2026 The mkxp-z authors
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

#include <cstring>
#include "input.h"
#include "core.h"
#include "sharedstate.h"
#include "graphics.h"
#include "mkxp-polyfill.h" // std::lround

#define JOYPAD_BUTTON_MAX 16
#define REPEAT_START (rgssVer >= 2 ? 0.375 : 0.400)
#define REPEAT_DELAY 0.1

#define DIRINDEX_TO_BUTTONCODE(dir) (((dir) + 1) * 2)
#define BUTTONCODE_TO_DIRINDEX(button) ((button) / 2 - 1)

const struct mkxp_input_retro_binding mkxpDefaultButtonMapping[NUM_BUTTONCODES] = {
    {},
    {},
    /* Input::Down */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_DOWN},
    {},
    /* Input::Left */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_LEFT},
    {},
    /* Input::Right */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_RIGHT},
    {},
    /* Input::Up */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_UP},
    {},
    {},
    /* Input::A */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_X},
    /* Input::B */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_B},
    /* Input::C */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_A},
    /* Input::X */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_Y},
    /* Input::Y */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_L3},
    /* Input::Z */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_R3},
    /* Input::L */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_L},
    /* Input::R */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_R},
    {},
    {},
    /* Input::Shift */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_R2},
    /* Input::Ctrl */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_L2},
    /* Input::Alt */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_START},
    {},
    /* Input::F5 */ {NONE},
    /* Input::F6 */ {NONE},
    /* Input::F7 */ {NONE},
    /* Input::F8 */ {NONE},
    /* Input::F9 */ {NONE},
    {},
    {},
    {},
    {},
    {},
    {},
    {},
    {},
    /* Input::MouseLeft */ {MOUSE, RETRO_DEVICE_ID_MOUSE_LEFT},
    /* Input::MouseMiddle */ {MOUSE, RETRO_DEVICE_ID_MOUSE_MIDDLE},
    /* Input::MouseRight */ {MOUSE, RETRO_DEVICE_ID_MOUSE_RIGHT},
    /* Input::MouseX1 */ {MOUSE, RETRO_DEVICE_ID_MOUSE_BUTTON_4},
    /* Input::MouseX2 */ {MOUSE, RETRO_DEVICE_ID_MOUSE_BUTTON_5},
};

struct mkxp_input_retro_binding mkxpButtonMapping[NUM_BUTTONCODES];

const struct mkxp_input_retro_binding mkxpDefaultScancodeMapping[NUM_SCANCODES] = {
    {}, {}, {}, {},
    /* A */ {BUTTON, Input::X},
    /* B */ {NONE},
    /* C */ {NONE},
    /* D */ {BUTTON, Input::Z},
    /* E */ {NONE},
    /* F */ {NONE},
    /* G */ {NONE},
    /* H */ {NONE},
    /* I */ {NONE},
    /* J */ {NONE},
    /* K */ {NONE},
    /* L */ {NONE},
    /* M */ {NONE},
    /* N */ {NONE},
    /* O */ {NONE},
    /* P */ {NONE},
    /* Q */ {BUTTON, Input::L},
    /* R */ {NONE},
    /* S */ {BUTTON, Input::Y},
    /* T */ {NONE},
    /* U */ {NONE},
    /* V */ {NONE},
    /* W */ {BUTTON, Input::R},
    /* X */ {BUTTON, Input::B},
    /* Y */ {NONE},
    /* Z */ {BUTTON, Input::C},
    /* 1 */ {NONE},
    /* 2 */ {NONE},
    /* 3 */ {NONE},
    /* 4 */ {NONE},
    /* 5 */ {NONE},
    /* 6 */ {NONE},
    /* 7 */ {NONE},
    /* 8 */ {NONE},
    /* 9 */ {NONE},
    /* 0 */ {NONE},
    /* RETURN */ {BUTTON, Input::C},
    /* ESCAPE */ {BUTTON, Input::B},
    /* BACKSPACE */ {NONE},
    /* TAB */ {NONE},
    /* SPACE */ {BUTTON, Input::C},
    /* MINUS */ {NONE},
    /* EQUALS */ {NONE},
    /* LEFTBRACKET */ {NONE},
    /* RIGHTBRACKET */ {NONE},
    /* BACKSLASH */ {NONE},
    /* NONUSHASH */ {NONE},
    /* SEMICOLON */ {NONE},
    /* APOSTROPHE */ {NONE},
    /* GRAVE */ {NONE},
    /* COMMA */ {NONE},
    /* PERIOD */ {NONE},
    /* SLASH */ {NONE},
    /* CAPSLOCK */ {NONE},
    /* F1 */ {NONE},
    /* F2 */ {NONE},
    /* F3 */ {NONE},
    /* F4 */ {NONE},
    /* F5 */ {BUTTON, Input::F5},
    /* F6 */ {BUTTON, Input::F6},
    /* F7 */ {BUTTON, Input::F7},
    /* F8 */ {BUTTON, Input::F8},
    /* F9 */ {BUTTON, Input::F9},
    /* F10 */ {NONE},
    /* F11 */ {NONE},
    /* F12 */ {NONE},
    /* PRINTSCREEN */ {NONE},
    /* SCROLLLOCK */ {NONE},
    /* PAUSE */ {NONE},
    /* INSERT */ {NONE},
    /* HOME */ {NONE},
    /* PAGEUP */ {NONE},
    /* DELETE */ {NONE},
    /* END */ {NONE},
    /* PAGEDOWN */ {NONE},
    /* RIGHT */ {BUTTON, Input::Right},
    /* LEFT */ {BUTTON, Input::Left},
    /* DOWN */ {BUTTON, Input::Down},
    /* UP */ {BUTTON, Input::Up},
    /* NUMLOCKCLEAR */ {NONE},
    /* KP_DIVIDE */ {NONE},
    /* KP_MULTIPLY */ {NONE},
    /* KP_MINUS */ {NONE},
    /* KP_PLUS */ {NONE},
    /* KP_ENTER */ {NONE},
    /* KP_1 */ {NONE},
    /* KP_2 */ {NONE},
    /* KP_3 */ {NONE},
    /* KP_4 */ {NONE},
    /* KP_5 */ {NONE},
    /* KP_6 */ {NONE},
    /* KP_7 */ {NONE},
    /* KP_8 */ {NONE},
    /* KP_9 */ {NONE},
    /* KP_0 */ {BUTTON, Input::B},
    /* KP_PERIOD */ {NONE},
    /* NONUSBACKSLASH */ {NONE},
    /* APPLICATION */ {NONE},
    /* POWER */ {NONE},
    /* KP_EQUALS */ {NONE},
    /* F13 */ {NONE},
    /* F14 */ {NONE},
    /* F15 */ {NONE},
    /* F16 */ {NONE},
    /* F17 */ {NONE},
    /* F18 */ {NONE},
    /* F19 */ {NONE},
    /* F20 */ {NONE},
    /* F21 */ {NONE},
    /* F22 */ {NONE},
    /* F23 */ {NONE},
    /* F24 */ {NONE},
    /* EXECUTE */ {NONE},
    /* HELP */ {NONE},
    /* MENU */ {NONE},
    /* SELECT */ {NONE},
    /* STOP */ {NONE},
    /* AGAIN */ {NONE},
    /* UNDO */ {NONE},
    /* CUT */ {NONE},
    /* COPY */ {NONE},
    /* PASTE */ {NONE},
    /* FIND */ {NONE},
    /* MUTE */ {NONE},
    /* VOLUMEUP */ {NONE},
    /* VOLUMEDOWN */ {NONE},
    {}, {}, {},
    /* KP_COMMA */ {NONE},
    /* KP_EQUALSAS400 */ {NONE},
    /* INTERNATIONAL1 */ {NONE},
    /* INTERNATIONAL2 */ {NONE},
    /* INTERNATIONAL3 */ {NONE},
    /* INTERNATIONAL4 */ {NONE},
    /* INTERNATIONAL5 */ {NONE},
    /* INTERNATIONAL6 */ {NONE},
    /* INTERNATIONAL7 */ {NONE},
    /* INTERNATIONAL8 */ {NONE},
    /* INTERNATIONAL9 */ {NONE},
    /* LANG1 */ {NONE},
    /* LANG2 */ {NONE},
    /* LANG3 */ {NONE},
    /* LANG4 */ {NONE},
    /* LANG5 */ {NONE},
    /* LANG6 */ {NONE},
    /* LANG7 */ {NONE},
    /* LANG8 */ {NONE},
    /* LANG9 */ {NONE},
    /* ALTERASE */ {NONE},
    /* SYSREQ */ {NONE},
    /* CANCEL */ {NONE},
    /* CLEAR */ {NONE},
    /* PRIOR */ {NONE},
    /* RETURN2 */ {NONE},
    /* SEPARATOR */ {NONE},
    /* OUT */ {NONE},
    /* OPER */ {NONE},
    /* CLEARAGAIN */ {NONE},
    /* CRSEL */ {NONE},
    /* EXSEL */ {NONE},
    {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /* KP_00 */ {NONE},
    /* KP_000 */ {NONE},
    /* THOUSANDSSEPARATOR */ {NONE},
    /* DECIMALSEPARATOR */ {NONE},
    /* CURRENCYUNIT */ {NONE},
    /* CURRENCYSUBUNIT */ {NONE},
    /* KP_LEFTPAREN */ {NONE},
    /* KP_RIGHTPAREN */ {NONE},
    /* KP_LEFTBRACE */ {NONE},
    /* KP_RIGHTBRACE */ {NONE},
    /* KP_TAB */ {NONE},
    /* KP_BACKSPACE */ {NONE},
    /* KP_A */ {NONE},
    /* KP_B */ {NONE},
    /* KP_C */ {NONE},
    /* KP_D */ {NONE},
    /* KP_E */ {NONE},
    /* KP_F */ {NONE},
    /* KP_XOR */ {NONE},
    /* KP_POWER */ {NONE},
    /* KP_PERCENT */ {NONE},
    /* KP_LESS */ {NONE},
    /* KP_GREATER */ {NONE},
    /* KP_AMPERSAND */ {NONE},
    /* KP_DBLAMPERSAND */ {NONE},
    /* KP_VERTICALBAR */ {NONE},
    /* KP_DBLVERTICALBAR */ {NONE},
    /* KP_COLON */ {NONE},
    /* KP_HASH */ {NONE},
    /* KP_SPACE */ {NONE},
    /* KP_AT */ {NONE},
    /* KP_EXCLAM */ {NONE},
    /* KP_MEMSTORE */ {NONE},
    /* KP_MEMRECALL */ {NONE},
    /* KP_MEMCLEAR */ {NONE},
    /* KP_MEMADD */ {NONE},
    /* KP_MEMSUBTRACT */ {NONE},
    /* KP_MEMMULTIPLY */ {NONE},
    /* KP_MEMDIVIDE */ {NONE},
    /* KP_PLUSMINUS */ {NONE},
    /* KP_CLEAR */ {NONE},
    /* KP_CLEARENTRY */ {NONE},
    /* KP_BINARY */ {NONE},
    /* KP_OCTAL */ {NONE},
    /* KP_DECIMAL */ {NONE},
    /* KP_HEXADECIMAL */ {NONE},
    {}, {},
    /* LCTRL */ {BUTTON, Input::Ctrl},
    /* LSHIFT */ {BUTTON, Input::A},
    /* LALT */ {BUTTON, Input::Alt},
    /* LGUI */ {NONE},
    /* RCTRL */ {BUTTON, Input::Ctrl},
    /* RSHIFT */ {BUTTON, Input::Shift},
    /* RALT */ {BUTTON, Input::Alt},
    /* RGUI */ {NONE},
    {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    /* MODE */ {NONE},
    /* AUDIONEXT */ {NONE},
    /* AUDIOPREV */ {NONE},
    /* AUDIOSTOP */ {NONE},
    /* AUDIOPLAY */ {NONE},
    /* AUDIOMUTE */ {NONE},
    /* MEDIASELECT */ {NONE},
    /* WWW */ {NONE},
    /* MAIL */ {NONE},
    /* CALCULATOR */ {NONE},
    /* COMPUTER */ {NONE},
    /* AC_SEARCH */ {NONE},
    /* AC_HOME */ {NONE},
    /* AC_BACK */ {NONE},
    /* AC_FORWARD */ {NONE},
    /* AC_STOP */ {NONE},
    /* AC_REFRESH */ {NONE},
    /* AC_BOOKMARKS */ {NONE},
    /* BRIGHTNESSDOWN */ {NONE},
    /* BRIGHTNESSUP */ {NONE},
    /* DISPLAYSWITCH */ {NONE},
    /* KBDILLUMTOGGLE */ {NONE},
    /* KBDILLUMDOWN */ {NONE},
    /* KBDILLUMUP */ {NONE},
    /* EJECT */ {NONE},
    /* SLEEP */ {NONE},
    /* APP1 */ {NONE},
    /* APP2 */ {NONE},
    /* AUDIOREWIND */ {NONE},
    /* AUDIOFASTFORWARD */ {NONE},
    /* SOFTLEFT */ {NONE},
    /* SOFTRIGHT */ {NONE},
    /* CALL */ {NONE},
    /* ENDCALL */ {NONE},
};

struct mkxp_input_retro_binding mkxpScancodeMapping[NUM_SCANCODES];

const struct mkxp_input_retro_binding mkxpDefaultControllerMapping[NUM_CONTROLLER_BUTTONS] = {
    /* A */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_A},
    /* B */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_B},
    /* X */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_X},
    /* Y */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_Y},
    /* BACK */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_SELECT},
    /* GUIDE */ {NONE},
    /* START */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_START},
    /* LEFTSTICK */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_L3},
    /* RIGHTSTICK */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_R3},
    /* LEFTSHOULDER */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_L},
    /* RIGHTSHOULDER */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_R},
    /* DPAD_UP */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_UP},
    /* DPAD_DOWN */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_DOWN},
    /* DPAD_LEFT */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_LEFT},
    /* DPAD_RIGHT */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_RIGHT},
    /* MISC1 */ {NONE},
    /* PADDLE1 */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_L2},
    /* PADDLE2 */ {JOYPAD, RETRO_DEVICE_ID_JOYPAD_R2},
    /* PADDLE3 */ {NONE},
    /* PADDLE4 */ {NONE},
    /* TOUCHPAD */ {NONE},
};

struct mkxp_input_retro_binding mkxpControllerMapping[NUM_CONTROLLER_BUTTONS];

/* This is a lookup table for handling directional input in `updateDir4()` and
 * `updateDir8()`. For a direction `dir` which can be either `Input::Down`,
 * `Input::Left`, `Input::Right` or `Input::Up`, the values of
 * `otherDirs[dir/2-1][0]`, `otherDirs[dir/2-1][1]` and `otherDirs[dir/2-1][2]`
 * are the three directions other than `dir`. */
static const uint8_t otherDirs[4][3] = {
    { Input::Left, Input::Right, Input::Up    }, /* Down  */
    { Input::Down, Input::Up,    Input::Right }, /* Left  */
    { Input::Down, Input::Up,    Input::Left  }, /* Right */
    { Input::Left, Input::Right, Input::Down  }, /* Up    */
};

static const enum retro_key scancodeToRetrok[NUM_SCANCODES] = {
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_a,
    RETROK_b,
    RETROK_c,
    RETROK_d,
    RETROK_e,
    RETROK_f,
    RETROK_g,
    RETROK_h,
    RETROK_i,
    RETROK_j,
    RETROK_k,
    RETROK_l,
    RETROK_m,
    RETROK_n,
    RETROK_o,
    RETROK_p,
    RETROK_q,
    RETROK_r,
    RETROK_s,
    RETROK_t,
    RETROK_u,
    RETROK_v,
    RETROK_w,
    RETROK_x,
    RETROK_y,
    RETROK_z,
    RETROK_1,
    RETROK_2,
    RETROK_3,
    RETROK_4,
    RETROK_5,
    RETROK_6,
    RETROK_7,
    RETROK_8,
    RETROK_9,
    RETROK_0,
    RETROK_RETURN,
    RETROK_ESCAPE,
    RETROK_BACKSPACE,
    RETROK_TAB,
    RETROK_SPACE,
    RETROK_MINUS,
    RETROK_EQUALS,
    RETROK_LEFTBRACKET,
    RETROK_RIGHTBRACKET,
    RETROK_BACKSLASH,
    RETROK_BACKSLASH,
    RETROK_SEMICOLON,
    RETROK_QUOTE,
    RETROK_BACKQUOTE,
    RETROK_COMMA,
    RETROK_PERIOD,
    RETROK_SLASH,
    RETROK_CAPSLOCK,
    RETROK_F1,
    RETROK_F2,
    RETROK_F3,
    RETROK_F4,
    RETROK_F5,
    RETROK_F6,
    RETROK_F7,
    RETROK_F8,
    RETROK_F9,
    RETROK_F10,
    RETROK_F11,
    RETROK_F12,
    RETROK_UNKNOWN,
    RETROK_SCROLLOCK,
    RETROK_PAUSE,
    RETROK_INSERT,
    RETROK_HOME,
    RETROK_PAGEUP,
    RETROK_DELETE,
    RETROK_END,
    RETROK_PAGEDOWN,
    RETROK_RIGHT,
    RETROK_LEFT,
    RETROK_DOWN,
    RETROK_UP,
    RETROK_NUMLOCK,
    RETROK_KP_DIVIDE,
    RETROK_KP_MULTIPLY,
    RETROK_KP_MINUS,
    RETROK_KP_PLUS,
    RETROK_KP_ENTER,
    RETROK_KP1,
    RETROK_KP2,
    RETROK_KP3,
    RETROK_KP4,
    RETROK_KP5,
    RETROK_KP6,
    RETROK_KP7,
    RETROK_KP8,
    RETROK_KP9,
    RETROK_KP0,
    RETROK_KP_PERIOD,
    RETROK_BACKSLASH,
    RETROK_COMPOSE,
    RETROK_POWER,
    RETROK_KP_EQUALS,
    RETROK_F13,
    RETROK_F14,
    RETROK_F15,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_HELP,
    RETROK_MENU,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNDO,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_VOLUME_MUTE,
    RETROK_VOLUME_UP,
    RETROK_VOLUME_DOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_SYSREQ,
    RETROK_UNKNOWN,
    RETROK_CLEAR,
    RETROK_PAGEUP,
    RETROK_UNKNOWN,
    RETROK_KP_PERIOD,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_KP_PERIOD,
    RETROK_KP_PERIOD,
    RETROK_KP_PERIOD,
    RETROK_KP_PERIOD,
    RETROK_LEFTPAREN,
    RETROK_RIGHTPAREN,
    RETROK_LEFTBRACE,
    RETROK_RIGHTBRACE,
    RETROK_TAB,
    RETROK_BACKSPACE,
    RETROK_a,
    RETROK_b,
    RETROK_c,
    RETROK_d,
    RETROK_e,
    RETROK_f,
    RETROK_UNKNOWN,
    RETROK_CARET,
    RETROK_UNKNOWN,
    RETROK_LESS,
    RETROK_GREATER,
    RETROK_AMPERSAND,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_COLON,
    RETROK_HASH,
    RETROK_SPACE,
    RETROK_AT,
    RETROK_EXCLAIM,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_LCTRL,
    RETROK_LSHIFT,
    RETROK_LALT,
    RETROK_LSUPER,
    RETROK_RCTRL,
    RETROK_RSHIFT,
    RETROK_RALT,
    RETROK_RSUPER,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_MEDIA_NEXT,
    RETROK_MEDIA_PREV,
    RETROK_MEDIA_STOP,
    RETROK_MEDIA_PLAY_PAUSE,
    RETROK_VOLUME_MUTE,
    RETROK_LAUNCH_MEDIA,
    RETROK_UNKNOWN,
    RETROK_LAUNCH_MAIL,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_BROWSER_SEARCH,
    RETROK_BROWSER_HOME,
    RETROK_BROWSER_BACK,
    RETROK_BROWSER_FORWARD,
    RETROK_BROWSER_STOP,
    RETROK_BROWSER_REFRESH,
    RETROK_BROWSER_FAVORITES,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_LAUNCH_APP1,
    RETROK_LAUNCH_APP2,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
};

static const enum retro_key vkeyToRetrok[] = {
    RETROK_UNKNOWN,
    RETROK_UNKNOWN, // left mouse button; specially handled
    RETROK_UNKNOWN, // right mouse button; specially handled
    RETROK_UNKNOWN,
    RETROK_UNKNOWN, // middle mouse button; specially handled
    RETROK_UNKNOWN, // mouse button 4; specially handled
    RETROK_UNKNOWN, // mouse button 5; specially handled
    RETROK_UNKNOWN,
    RETROK_BACKSPACE,
    RETROK_TAB,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_CLEAR,
    RETROK_RETURN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN, // shift; specially handled to map to both RETROK_LSHIFT and RETROK_RSHIFT
    RETROK_UNKNOWN, // ctrl; specially handled to map to both RETROK_LCTRL and RETROK_RCTRL
    RETROK_UNKNOWN, // alt; specially handled to map to both RETROK_LALT and RETROK_RALT
    RETROK_PAUSE,
    RETROK_CAPSLOCK,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_ESCAPE,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_SPACE,
    RETROK_PAGEUP,
    RETROK_PAGEDOWN,
    RETROK_END,
    RETROK_HOME,
    RETROK_LEFT,
    RETROK_UP,
    RETROK_RIGHT,
    RETROK_DOWN,
    RETROK_UNKNOWN,
    RETROK_PRINT,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_INSERT,
    RETROK_DELETE,
    RETROK_HELP,
    RETROK_0,
    RETROK_1,
    RETROK_2,
    RETROK_3,
    RETROK_4,
    RETROK_5,
    RETROK_6,
    RETROK_7,
    RETROK_8,
    RETROK_9,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_a,
    RETROK_b,
    RETROK_c,
    RETROK_d,
    RETROK_e,
    RETROK_f,
    RETROK_g,
    RETROK_h,
    RETROK_i,
    RETROK_j,
    RETROK_k,
    RETROK_l,
    RETROK_m,
    RETROK_n,
    RETROK_o,
    RETROK_p,
    RETROK_q,
    RETROK_r,
    RETROK_s,
    RETROK_t,
    RETROK_u,
    RETROK_v,
    RETROK_w,
    RETROK_x,
    RETROK_y,
    RETROK_z,
    RETROK_LSUPER,
    RETROK_RSUPER,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_KP0,
    RETROK_KP1,
    RETROK_KP2,
    RETROK_KP3,
    RETROK_KP4,
    RETROK_KP5,
    RETROK_KP6,
    RETROK_KP7,
    RETROK_KP8,
    RETROK_KP9,
    RETROK_KP_MULTIPLY,
    RETROK_KP_PLUS,
    RETROK_KP_PERIOD,
    RETROK_KP_MINUS,
    RETROK_KP_PERIOD,
    RETROK_KP_DIVIDE,
    RETROK_F1,
    RETROK_F2,
    RETROK_F3,
    RETROK_F4,
    RETROK_F5,
    RETROK_F6,
    RETROK_F7,
    RETROK_F8,
    RETROK_F9,
    RETROK_F10,
    RETROK_F11,
    RETROK_F12,
    RETROK_F13,
    RETROK_F14,
    RETROK_F15,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_NUMLOCK,
    RETROK_SCROLLOCK,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_LSHIFT,
    RETROK_RSHIFT,
    RETROK_LCTRL,
    RETROK_RCTRL,
    RETROK_LALT,
    RETROK_RALT,
    RETROK_BROWSER_BACK,
    RETROK_BROWSER_FORWARD,
    RETROK_BROWSER_REFRESH,
    RETROK_BROWSER_STOP,
    RETROK_BROWSER_SEARCH,
    RETROK_BROWSER_FAVORITES,
    RETROK_BROWSER_HOME,
    RETROK_VOLUME_MUTE,
    RETROK_VOLUME_DOWN,
    RETROK_VOLUME_UP,
    RETROK_MEDIA_NEXT,
    RETROK_MEDIA_PREV,
    RETROK_MEDIA_STOP,
    RETROK_MEDIA_PLAY_PAUSE,
    RETROK_LAUNCH_MAIL,
    RETROK_LAUNCH_MEDIA,
    RETROK_LAUNCH_APP1,
    RETROK_LAUNCH_APP2,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_SEMICOLON,
    RETROK_PLUS,
    RETROK_COMMA,
    RETROK_MINUS,
    RETROK_PERIOD,
    RETROK_SLASH,
    RETROK_BACKQUOTE,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_LEFTBRACKET,
    RETROK_BACKSLASH,
    RETROK_RIGHTBRACKET,
    RETROK_QUOTE,
    RETROK_EXCLAIM,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_OEM_102,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_COMPOSE,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
    RETROK_UNKNOWN,
};

// Compiled from the vKeyToScancode from input.cpp
static const uint16_t vKeyToScancode[] = {0, 0, 0, 155, 0, 0, 0, 0, 42, 43, 0, 0, 156, 40, 0, 0, 0, 0, 0, 72, 57, 0, 0, 0, 0, 0, 0, 41, 0, 0, 0, 0, 44, 75, 78, 77, 74, 80, 82, 79, 81, 119, 0, 116, 70, 73, 76, 117, 39, 30, 31, 32, 33, 34, 35, 36, 37, 38, 0, 0, 0, 0, 0, 0, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 227, 231, 101, 0, 282, 98, 89, 90, 91, 92, 93, 94, 95, 96, 97, 85, 87, 40, 86, 220, 84, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 0, 0, 0, 0, 0, 0, 0, 0, 83, 71, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 225, 229, 224, 228, 226, 230, 270, 271, 273, 272, 268, 0, 269, 262, 129, 128, 258, 259, 260, 261, 265, 263, 0, 0, 0, 0, 51, 46, 54, 45, 55, 56, 53, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 47, 49, 48, 52, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 163, 164, 0, 261, 0, 0, 0, 156};

#define NUM_VKEYS (sizeof vKeyToScancode / sizeof *vKeyToScancode)

struct InputPrivate
{
    uint16_t joypadStates[NUM_INPUT_PORTS];
    uint32_t lightgunStates[NUM_INPUT_PORTS];
    uint16_t mouseStates[NUM_INPUT_PORTS];
    int16_t mouseX;
    int16_t mouseY;
    uint32_t scrollV;
    bool mouseInWindow;
    bool keyboardStates[RETROK_LAST];

    uint8_t buttonStates[NUM_BUTTONCODES];
    uint8_t buttonStatesOld[NUM_BUTTONCODES];

    uint8_t dir4;
    uint8_t dir4Old;
    uint8_t dir8;

    uint8_t rawKeyStates[NUM_SCANCODES];
    uint8_t rawKeyStatesOld[NUM_SCANCODES];
    uint8_t rawButtonStates[NUM_CONTROLLER_BUTTONS];
    uint8_t rawButtonStatesOld[NUM_CONTROLLER_BUTTONS];

    uint8_t repeating;
    uint32_t repeatCount;
    double repeatTime;

    uint16_t rawRepeating;
    uint32_t rawRepeatCount;
    double rawRepeatTime;

    uint8_t buttonRepeating;
    uint32_t buttonRepeatCount;
    double buttonRepeatTime;

    double last_update;

    bool joypadMaskSupported;

    InputPrivate() :
        joypadStates {},
        lightgunStates {},
        mouseStates {},
        mouseX(0),
        mouseY(0),
        scrollV(0),
        mouseInWindow(false),
        keyboardStates {},
        buttonStates {},
        buttonStatesOld {},
        dir4(0),
        dir4Old(0),
        dir8(0),
        rawKeyStates {},
        rawKeyStatesOld {},
        rawButtonStates {},
        rawButtonStatesOld {},
        repeating(Input::None),
        repeatCount(0),
        repeatTime(0),
        rawRepeating(-1),
        rawRepeatCount(0),
        rawRepeatTime(0),
        buttonRepeating(-1),
        buttonRepeatCount(0),
        buttonRepeatTime(0),
        last_update(0),
        joypadMaskSupported(mkxp_retro::environment(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, nullptr))
    {
    }

    void updateJoypad()
    {
        for (uint8_t port = 0; port < NUM_INPUT_PORTS; ++port)
        {
            if (joypadMaskSupported) {
                joypadStates[port] = (uint16_t)mkxp_retro::input_state(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
            } else {
                joypadStates[port] = 0;
                for (uint8_t i = 0; i < JOYPAD_BUTTON_MAX; ++i) {
                    if (mkxp_retro::input_state(port, RETRO_DEVICE_JOYPAD, 0, i)) {
                        joypadStates[port] |= (1 << i);
                    }
                }
            }
        }
    }

    void updateLightgun()
    {
        for (uint8_t port = 0; port < NUM_INPUT_PORTS; ++port)
        {
            lightgunStates[port] = mkxp_retro::input_state(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_TRIGGER) ? 1 << RETRO_DEVICE_ID_LIGHTGUN_TRIGGER : 0;
            lightgunStates[port] = mkxp_retro::input_state(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_AUX_A) ? 1 << RETRO_DEVICE_ID_LIGHTGUN_AUX_A : 0;
            lightgunStates[port] = mkxp_retro::input_state(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_AUX_B) ? 1 << RETRO_DEVICE_ID_LIGHTGUN_AUX_B : 0;
            lightgunStates[port] = mkxp_retro::input_state(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_START) ? 1 << RETRO_DEVICE_ID_LIGHTGUN_START : 0;
            lightgunStates[port] = mkxp_retro::input_state(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_SELECT) ? 1 << RETRO_DEVICE_ID_LIGHTGUN_SELECT : 0;
            lightgunStates[port] = mkxp_retro::input_state(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_AUX_C) ? 1 << RETRO_DEVICE_ID_LIGHTGUN_AUX_C : 0;
            lightgunStates[port] = mkxp_retro::input_state(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP) ? 1 << RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP : 0;
            lightgunStates[port] = mkxp_retro::input_state(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN) ? 1 << RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN : 0;
            lightgunStates[port] = mkxp_retro::input_state(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT) ? 1 << RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT : 0;
            lightgunStates[port] = mkxp_retro::input_state(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT) ? 1 << RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT : 0;
            lightgunStates[port] = mkxp_retro::input_state(port, RETRO_DEVICE_LIGHTGUN, 0, RETRO_DEVICE_ID_LIGHTGUN_RELOAD) ? 1 << RETRO_DEVICE_ID_LIGHTGUN_RELOAD : 0;
        }
    }

    void updateMouse()
    {
        for (uint8_t port = 0; port < 3; ++port)
        {
            mouseStates[port] = mkxp_retro::input_state(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT) ? 1 << RETRO_DEVICE_ID_MOUSE_LEFT : 0;
            mouseStates[port] |= mkxp_retro::input_state(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT) ? 1 << RETRO_DEVICE_ID_MOUSE_RIGHT : 0;
            mouseStates[port] |= mkxp_retro::input_state(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE) ? 1 << RETRO_DEVICE_ID_MOUSE_MIDDLE : 0;
            mouseStates[port] |= mkxp_retro::input_state(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_BUTTON_4) ? 1 << RETRO_DEVICE_ID_MOUSE_BUTTON_4 : 0;
            mouseStates[port] |= mkxp_retro::input_state(port, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_BUTTON_5) ? 1 << RETRO_DEVICE_ID_MOUSE_BUTTON_5 : 0;
        }

        mouseX = (int16_t)mkxp_retro::input_state(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
        mouseY = (int16_t)mkxp_retro::input_state(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);
        mouseInWindow = !mkxp_retro::input_state(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN);
        scrollV += mkxp_retro::input_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN);
        scrollV -= mkxp_retro::input_state(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELUP);
    }

    void updateKeyboard()
    {
        std::memcpy(keyboardStates, mkxp_retro::keyboard_state, sizeof keyboardStates);
    }

    template <bool isScancode, bool isControllerButton> bool getMappedState(uint16_t buttonOrScancodeOrControllerButton)
    {
        if (buttonOrScancodeOrControllerButton >= (isScancode ? NUM_SCANCODES : isControllerButton ? NUM_CONTROLLER_BUTTONS : NUM_BUTTONCODES))
            return false;

        if (!isScancode && !isControllerButton && buttonOrScancodeOrControllerButton == Input::None)
            return false;

        const struct mkxp_input_retro_binding *const defaultMapping = isScancode ? mkxpDefaultScancodeMapping : isControllerButton ? mkxpDefaultControllerMapping : mkxpDefaultButtonMapping;
        const struct mkxp_input_retro_binding *mapping = isScancode ? mkxpScancodeMapping : isControllerButton ? mkxpControllerMapping : mkxpButtonMapping;

        for (;;)
            switch (mapping[buttonOrScancodeOrControllerButton].type)
            {
                case DEFAULT:
                    if (mapping == defaultMapping)
                        return false;
                    mapping = defaultMapping;
                    break;

                case NONE:
                    return false;

                case BUTTON:
                    return isScancode || isControllerButton ? getMappedState<false, false>(mapping[buttonOrScancodeOrControllerButton].id) : false;

                case JOYPAD:
                    if (mapping[buttonOrScancodeOrControllerButton].port >= NUM_INPUT_PORTS)
                        return false;
                    return joypadStates[mapping[buttonOrScancodeOrControllerButton].port] & (1 << mapping[buttonOrScancodeOrControllerButton].id);

                case LIGHTGUN:
                    if (mapping[buttonOrScancodeOrControllerButton].port >= NUM_INPUT_PORTS)
                        return false;
                    return lightgunStates[mapping[buttonOrScancodeOrControllerButton].port] & (1 << mapping[buttonOrScancodeOrControllerButton].id);

                case MOUSE:
                    if (mapping[buttonOrScancodeOrControllerButton].port >= NUM_INPUT_PORTS)
                        return false;
                    return mouseStates[mapping[buttonOrScancodeOrControllerButton].port] & (1 << mapping[buttonOrScancodeOrControllerButton].id);
            }
    }

    void updateButtonStates()
    {
        std::memcpy(buttonStatesOld, buttonStates, sizeof buttonStates);
        for (uint8_t i = 0; i < NUM_BUTTONCODES; ++i)
            buttonStates[i] = getMappedState<false, false>(i);
    }

    void updateDir4()
    {
        /* Check for dead keys */
        if ((buttonStates[Input::Down] && buttonStates[Input::Up]) || (buttonStates[Input::Left] && buttonStates[Input::Right]))
        {
            dir4 = Input::None;
            return;
        }

        if (dir4Old != Input::None)
        {
            /* Check if prev still pressed */
            if (buttonStates[dir4Old])
            {
                for (size_t i = 0; i < 3; ++i)
                {
                    uint8_t other = otherDirs[BUTTONCODE_TO_DIRINDEX(dir4Old)][i];

                    if (!buttonStates[other])
                        continue;

                    dir4 = other;
                    return;
                }
            }
        }

        for (size_t i = 0; i < 4; ++i)
        {
            if (!buttonStates[DIRINDEX_TO_BUTTONCODE(i)])
                continue;

            dir4 = dir4Old = DIRINDEX_TO_BUTTONCODE(i);
            return;
        }

        dir4 = dir4Old = Input::None;
    }

    void updateDir8()
    {
        static const uint8_t combos[4][4] =
        {
            { 2, 1, 3, 0 },
            { 1, 4, 0, 7 },
            { 3, 0, 6, 9 },
            { 0, 7, 9, 8 },
        };

        dir8 = 0;

        for (size_t i = 0; i < 4; ++i)
        {
            if (!buttonStates[DIRINDEX_TO_BUTTONCODE(i)])
                continue;

            for (size_t j = 0; j < 3; ++j)
            {
                uint8_t other = otherDirs[i][j];

                if (!buttonStates[other])
                    continue;

                dir8 = combos[i][BUTTONCODE_TO_DIRINDEX(other)];
                return;
            }

            dir8 = combos[i][i];
            return;
        }
    }

    void updateRawKeyStates()
    {
        std::memcpy(rawKeyStatesOld, rawKeyStates, sizeof rawKeyStates);
        for (uint16_t i = 0; i < NUM_SCANCODES; ++i)
            rawKeyStates[i] = keyboardStates[scancodeToRetrok[i]] || getMappedState<true, false>(i);
    }

    void updateRawButtonStates()
    {
        std::memcpy(rawButtonStatesOld, rawButtonStates, sizeof rawButtonStates);
        for (uint8_t i = 0; i < NUM_CONTROLLER_BUTTONS; ++i)
            rawButtonStates[i] = getMappedState<false, true>(i);
    }

    void updateRepeat()
    {
        for (uint8_t i = 0; i < NUM_BUTTONCODES; ++i)
        {
            /* Check for new repeating button */
            if (i != Input::None && buttonStates[i] && !buttonStatesOld[i])
            {
                repeating = i;
                repeatCount = 0;
                repeatTime = shState->runTime();
                return;
            }
        }

        /* Check if repeating button still pressed */
        if (repeating != Input::None && buttonStates[repeating])
        {
            ++repeatCount;
            return;
        }

        repeating = Input::None;
    }

    void updateRawRepeat()
    {
        for (uint16_t i = 0; i < NUM_SCANCODES; ++i)
        {
            /* Check for new repeating key */
            if (rawKeyStates[i] && !rawKeyStatesOld[i])
            {
                rawRepeating = i;
                rawRepeatCount = 0;
                rawRepeatTime = shState->runTime();
                return;
            }
        }

        /* Check if repeating key still pressed */
        if (rawRepeating != (uint16_t)-1 && rawKeyStates[rawRepeating])
        {
            ++rawRepeatCount;
            return;
        }

        rawRepeating = -1;
    }

    void updateButtonRepeat()
    {
        for (uint8_t i = 0; i < NUM_CONTROLLER_BUTTONS; ++i)
        {
            /* Check for new repeating controller button */
            if (rawButtonStates[i] && !rawButtonStatesOld[i])
            {
                buttonRepeating = i;
                buttonRepeatCount = 0;
                buttonRepeatTime = shState->runTime();
                return;
            }
        }

        /* Check if repeating controller button still pressed */
        if (buttonRepeating != (uint8_t)-1 && rawButtonStates[buttonRepeating])
        {
            ++buttonRepeatCount;
            return;
        }

        buttonRepeating = -1;
    }

    bool getState(int button)
    {
        if (button < 0 || button >= NUM_BUTTONCODES)
            return false;
        return buttonStates[button];
    }

    bool getStateOld(int button)
    {
        if (button < 0 || button >= NUM_BUTTONCODES)
            return false;
        return buttonStatesOld[button];
    }

    bool isRepeat(int button)
    {
        return getState(button) && repeating == button;
    }

    bool getRawState(int button, bool isVKey)
    {
        if (button < 0 || button >= (isVKey ? NUM_VKEYS : NUM_SCANCODES))
            return false;
        if (!isVKey)
            return rawKeyStates[button];
        switch (button)
        {
            case 0x10:
                return getState(Input::Shift);
            case 0x11:
                return getState(Input::Ctrl);
            case 0x12:
                return getState(Input::Alt);
            case 0x1:
                return getState(Input::MouseLeft);
            case 0x2:
                return getState(Input::MouseRight);
            case 0x4:
                return getState(Input::MouseMiddle);
            default:
                return rawKeyStates[vKeyToScancode[button]];
        }
    }

    bool getRawStateOld(int button, bool isVKey)
    {
        if (button < 0 || button >= (isVKey ? NUM_VKEYS : NUM_SCANCODES))
            return false;
        if (!isVKey)
            return rawKeyStatesOld[button];
        switch (button)
        {
            case 0x10:
                return getStateOld(Input::Shift);
            case 0x11:
                return getStateOld(Input::Ctrl);
            case 0x12:
                return getStateOld(Input::Alt);
            case 0x1:
                return getStateOld(Input::MouseLeft);
            case 0x2:
                return getStateOld(Input::MouseRight);
            case 0x4:
                return getStateOld(Input::MouseMiddle);
            default:
                return rawKeyStatesOld[vKeyToScancode[button]];
        }
    }

    bool isRawRepeat(int button, bool isVKey)
    {
        if (!getRawState(button, isVKey))
            return false;
        if (!isVKey)
            return rawRepeating == button;
        switch (button)
        {
            case 0x10:
                return isRepeat(Input::Shift);
            case 0x11:
                return isRepeat(Input::Ctrl);
            case 0x12:
                return isRepeat(Input::Alt);
            case 0x1:
                return isRepeat(Input::MouseLeft);
            case 0x2:
                return isRepeat(Input::MouseRight);
            case 0x4:
                return isRepeat(Input::MouseMiddle);
            default:
                return rawRepeating == vKeyToScancode[button];
        }
    }

    bool getControllerState(int button)
    {
        if (button < 0 || button >= NUM_CONTROLLER_BUTTONS)
            return false;
        return rawButtonStates[button];
    }

    bool getControllerStateOld(int button)
    {
        if (button < 0 || button >= NUM_CONTROLLER_BUTTONS)
            return false;
        return rawButtonStatesOld[button];
    }

    bool isControllerRepeat(int button)
    {
        return getControllerState(button) && buttonRepeating == button;
    }
};

Input::Input()
{
    p = new InputPrivate();
}

Input::~Input()
{
    delete p;
}

void Input::recalcRepeat(unsigned int fps)
{

}

double Input::getDelta()
{
    return shState->runTime() - p->last_update;
}

void Input::update()
{
    mkxp_retro::input_poll();
    mkxp_retro::input_polled = true;
    p->updateJoypad();
    p->updateLightgun();
    p->updateMouse();
    p->updateKeyboard();
    p->updateButtonStates();
    p->updateDir4();
    p->updateDir8();
    p->updateRawKeyStates();
    p->updateRawButtonStates();
    p->updateRepeat();
    p->updateRawRepeat();
    p->updateButtonRepeat();
    p->last_update = shState->runTime();
}

bool Input::isPressed(int button)
{
    return p->getState(button);
}

bool Input::isTriggered(int button)
{
    return p->getState(button) && !p->getStateOld(button);
}

bool Input::isReleased(int button)
{
    return p->getStateOld(button) && !p->getState(button);
}

bool Input::isRepeated(int button)
{
    int frame_rate = shState->graphics().getFrameRate();
    return p->isRepeat(button) && (p->repeatCount == 0 || (p->repeatCount >= (size_t)std::ceil(REPEAT_START * frame_rate) && (p->repeatCount + 1) % (size_t)std::ceil(REPEAT_DELAY * frame_rate) == 0));
}

unsigned int Input::count(int button)
{
    return p->isRepeat(button) ? p->repeatCount : 0;
}

double Input::repeatTime(int button)
{
    return p->isRepeat(button) ? shState->runTime() - p->repeatTime : 0;
}

bool Input::isPressedEx(int button, bool isVKey)
{
    return p->getRawState(button, isVKey);
}

bool Input::isTriggeredEx(int button, bool isVKey)
{
    return p->getRawState(button, isVKey) && !p->getRawStateOld(button, isVKey);
}

bool Input::isReleasedEx(int button, bool isVKey)
{
    return p->getRawStateOld(button, isVKey) && !p->getRawState(button, isVKey);
}

bool Input::isRepeatedEx(int button, bool isVKey)
{
    int frame_rate = shState->graphics().getFrameRate();
    return p->isRawRepeat(button, isVKey) && (p->rawRepeatCount == 0 || (p->rawRepeatCount >= (size_t)std::ceil(REPEAT_START * frame_rate) && (p->rawRepeatCount + 1) % (size_t)std::ceil(REPEAT_DELAY * frame_rate) == 0));
}

unsigned int Input::repeatcount(int button, bool isVKey)
{
    return p->isRawRepeat(button, isVKey) ? p->rawRepeatCount : 0;
}

double Input::repeatTimeEx(int button, bool isVKey)
{
    return p->isRawRepeat(button, isVKey) ? shState->runTime() - p->rawRepeatTime : 0;
}

bool Input::controllerIsPressedEx(int button)
{
    return p->getControllerState(button);
}

bool Input::controllerIsTriggeredEx(int button)
{
    return p->getControllerState(button) && !p->getControllerStateOld(button);
}

bool Input::controllerIsReleasedEx(int button)
{
    return p->getControllerStateOld(button) && !p->getControllerState(button);
}

bool Input::controllerIsRepeatedEx(int button)
{
    int frame_rate = shState->graphics().getFrameRate();
    return p->isControllerRepeat(button) && (p->buttonRepeatCount == 0 || (p->buttonRepeatCount >= (size_t)std::ceil(REPEAT_START * frame_rate) && (p->buttonRepeatCount + 1) % (size_t)std::ceil(REPEAT_DELAY * frame_rate) == 0));
}

unsigned int Input::controllerRepeatcount(int button)
{
    return p->isControllerRepeat(button) ? p->buttonRepeatCount : 0;
}

double Input::controllerRepeatTimeEx(int button)
{
    return p->isControllerRepeat(button) ? shState->runTime() - p->buttonRepeatTime : 0;
}

uint8_t *Input::rawKeyStates()
{
    return p->rawKeyStates;
}

unsigned int Input::rawKeyStatesLength()
{
    return NUM_SCANCODES;
}

uint8_t *Input::rawButtonStates()
{
    return p->rawButtonStates;
}

unsigned int Input::rawButtonStatesLength()
{
    return NUM_CONTROLLER_BUTTONS;
}

int16_t *Input::rawAxes()
{
    return nullptr; // TODO
}

unsigned int Input::rawAxesLength()
{
    return 0; // TODO
}

short Input::getControllerAxisValue(SDL_GameControllerAxis axis)
{
    return 0; // TODO
}

int Input::dir4Value()
{
    return p->dir4;
}

int Input::dir8Value()
{
    return p->dir8;
}

int Input::mouseX()
{
    uint16_t x = clamp(p->mouseX, (int16_t)-32767, (int16_t)32767);
    x += 32767;
    return (int)std::lround(((float)x / 65534.0f) * shState->graphics().width());
}

int Input::mouseY()
{
    uint16_t y = clamp(p->mouseY, (int16_t)-32767, (int16_t)32767);
    y += 32767;
    return (int)std::lround(((float)y / 65534.0f) * shState->graphics().height());
}

int Input::scrollV()
{
    return (int)p->scrollV;
}

bool Input::mouseInWindow()
{
    return p->mouseInWindow;
}

bool Input::getControllerConnected()
{
    return true;
}

const char *Input::getControllerName()
{
    return "RetroPad";
}

int Input::getControllerPowerLevel()
{
    return (int)SDL_JOYSTICK_POWER_UNKNOWN;
}

bool Input::getTextInputMode()
{
    return false;
}

void Input::setTextInputMode(bool mode)
{

}

const char *Input::getText()
{
    return "";
}

void Input::clearText()
{

}

std::string Input::getClipboardText()
{
    return {};
}

void Input::setClipboardText(const char *text)
{

}
