/*
 ** input-binding.cpp
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

#include <SDL_joystick.h>
#include <string>


#include "eventthread.h"

#include "binding-util.h"
#include "util/exception.h"
#include "input/input.h"
#include "sharedstate.h"
#include "src/util/util.h"

#if RAPI_FULL > 187
DEF_TYPE(Input);
#else
DEF_ALLOCFUNC(Input);
#endif

RB_METHOD(inputInitialize) {
    RB_UNUSED_PARAM;
    setPrivateData(self, new Input(shState->rtData()));
    return self;
}

static Input &getInput(VALUE self) {
    return rb_typeddata_is_kind_of(self, &InputType) ? *getPrivateDataNoRaise<Input>(self) : shState->input();
}

RB_METHOD(inputDelta) {
    RB_UNUSED_PARAM;
    
    return rb_float_new(getInput(self).getDelta());
}

RB_METHOD_GUARD(inputUpdate) {
    RB_UNUSED_PARAM;
    
    getInput(self).update();
    
    return Qnil;
}
RB_METHOD_GUARD_END

static int getButtonArg(VALUE *argv) {
    int num;
    
    if (FIXNUM_P(*argv)) {
        num = FIX2INT(*argv);
    } else if (SYMBOL_P(*argv) && rgssVer >= 3) {
        VALUE symHash = getRbData()->buttoncodeHash;
#if RAPI_FULL > 187
        num = FIX2INT(rb_hash_lookup2(symHash, *argv, INT2FIX(Input::None)));
#else
        VALUE res = rb_hash_aref(symHash, *argv);
        if (!NIL_P(res))
            num = FIX2INT(res);
        else
            num = Input::None;
#endif
    } else {
        // FIXME: RMXP allows only few more types that
        // don't make sense (symbols in pre 3, floats)
        num = 0;
    }
    
    return num;
}

static int getScancodeArg(VALUE *argv) {
    const char *scancode = rb_id2name(SYM2ID(*argv));
    int code{};
    try {
        code = strToScancode[scancode];
    } catch (...) {
        throw Exception(Exception::RuntimeError, "%s is not a valid name of an SDL scancode.", scancode);
    }
    
    return code;
}

static int getControllerButtonArg(VALUE *argv) {
    const char *button = rb_id2name(SYM2ID(*argv));
    int btn{};
    try {
        btn = strToGCButton[button];
    } catch (...) {
        throw Exception(Exception::RuntimeError, "%s is not a valid name of an SDL Controller button.", button);
    }
    
    return btn;
}

RB_METHOD(inputPress) {
    RB_UNUSED_PARAM;
    
    rb_check_argc(argc, 1);
    
    VALUE button;
    rb_scan_args(argc, argv, "1", &button);
    
    int num = getButtonArg(&button);
    
    return rb_bool_new(getInput(self).isPressed(num));
}

RB_METHOD(inputTrigger) {
    RB_UNUSED_PARAM;
    
    rb_check_argc(argc, 1);
    
    VALUE button;
    rb_scan_args(argc, argv, "1", &button);
    
    int num = getButtonArg(&button);
    
    return rb_bool_new(getInput(self).isTriggered(num));
}

RB_METHOD(inputRepeat) {
    RB_UNUSED_PARAM;
    
    rb_check_argc(argc, 1);
    
    VALUE button;
    rb_scan_args(argc, argv, "1", &button);
    
    int num = getButtonArg(&button);
    
    return rb_bool_new(getInput(self).isRepeated(num));
}

RB_METHOD(inputRelease) {
    RB_UNUSED_PARAM;
    
    rb_check_argc(argc, 1);
    
    VALUE button;
    rb_scan_args(argc, argv, "1", &button);
    
    int num = getButtonArg(&button);
    
    return rb_bool_new(getInput(self).isReleased(num));
}

RB_METHOD(inputCount) {
    RB_UNUSED_PARAM;
    
    rb_check_argc(argc, 1);
    
    VALUE button;
    rb_scan_args(argc, argv, "1", &button);
    
    int num = getButtonArg(&button);
    
    return UINT2NUM(getInput(self).count(num));
}

RB_METHOD(inputRepeatTime) {
    RB_UNUSED_PARAM;
    
    rb_check_argc(argc, 1);
    
    VALUE button;
    rb_scan_args(argc, argv, "1", &button);
    
    int num = getButtonArg(&button);
    
    return rb_float_new(getInput(self).repeatTime(num));
}

RB_METHOD_GUARD(inputPressEx) {
    RB_UNUSED_PARAM;
    
    VALUE button;
    rb_scan_args(argc, argv, "1", &button);
    
    if (SYMBOL_P(button)) {
        int num = getScancodeArg(&button);
        return rb_bool_new(getInput(self).isPressedEx(num, 0));
    }
    
    return rb_bool_new(getInput(self).isPressedEx(NUM2INT(button), 1));
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(inputTriggerEx) {
    RB_UNUSED_PARAM;
    
    VALUE button;
    rb_scan_args(argc, argv, "1", &button);
    
    if (SYMBOL_P(button)) {
        int num = getScancodeArg(&button);
        return rb_bool_new(getInput(self).isTriggeredEx(num, 0));
    }
    
    return rb_bool_new(getInput(self).isTriggeredEx(NUM2INT(button), 1));
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(inputRepeatEx) {
    RB_UNUSED_PARAM;
    
    VALUE button;
    rb_scan_args(argc, argv, "1", &button);
    
    if (SYMBOL_P(button)) {
        int num = getScancodeArg(&button);
        return rb_bool_new(getInput(self).isRepeatedEx(num, 0));
    }
    
    return rb_bool_new(getInput(self).isRepeatedEx(NUM2INT(button), 1));
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(inputReleaseEx) {
    RB_UNUSED_PARAM;
    
    VALUE button;
    rb_scan_args(argc, argv, "1", &button);
    
    if (SYMBOL_P(button)) {
        int num = getScancodeArg(&button);
        return rb_bool_new(getInput(self).isReleasedEx(num, 0));
    }
    
    return rb_bool_new(getInput(self).isReleasedEx(NUM2INT(button), 1));
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(inputCountEx) {
    RB_UNUSED_PARAM;
    
    VALUE button;
    rb_scan_args(argc, argv, "1", &button);
    
    if (SYMBOL_P(button)) {
        int num = getScancodeArg(&button);
        return UINT2NUM(getInput(self).repeatcount(num, 0));
    }
    
    return UINT2NUM(getInput(self).repeatcount(NUM2INT(button), 1));
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(inputRepeatTimeEx) {
    RB_UNUSED_PARAM;
    
    VALUE button;
    rb_scan_args(argc, argv, "1", &button);
    
    if (SYMBOL_P(button)) {
        int num = getScancodeArg(&button);
        return rb_float_new(getInput(self).repeatTimeEx(num, 0));
    }
    
    return rb_float_new(getInput(self).repeatTimeEx(NUM2INT(button), 1));
}
RB_METHOD_GUARD_END

RB_METHOD(inputDir4) {
    RB_UNUSED_PARAM;
    
    return rb_fix_new(getInput(self).dir4Value());
}

RB_METHOD(inputDir8) {
    RB_UNUSED_PARAM;
    
    return rb_fix_new(getInput(self).dir8Value());
}

/* Non-standard extensions */
RB_METHOD(inputMouseX) {
    RB_UNUSED_PARAM;
    
    return rb_fix_new(getInput(self).mouseX());
}

RB_METHOD(inputMouseY) {
    RB_UNUSED_PARAM;
    
    return rb_fix_new(getInput(self).mouseY());
}

RB_METHOD(inputScrollV) {
    RB_UNUSED_PARAM;
    
    return rb_fix_new(getInput(self).scrollV());
}

RB_METHOD(inputMouseInWindow) {
    RB_UNUSED_PARAM;
    
    return rb_bool_new(getInput(self).mouseInWindow());
}

RB_METHOD(inputRawKeyStates) {
    RB_UNUSED_PARAM;
    
    VALUE ret = rb_ary_new();

    uint8_t *states = getInput(self).rawKeyStates();
    
    for (unsigned int i = 0; i < getInput(self).rawKeyStatesLength(); i++)
        rb_ary_push(ret, rb_bool_new(states[i]));
    
    return ret;
}

#define M_SYMBOL(x) ID2SYM(rb_intern(x))
#define POWERCASE(v, c)                                                        \
case SDL_JOYSTICK_POWER_##c:                                                 \
v = M_SYMBOL(#c);                                                          \
break;

RB_METHOD(inputControllerConnected) {
    RB_UNUSED_PARAM;
    
    return rb_bool_new(getInput(self).getControllerConnected());
}

RB_METHOD(inputControllerName) {
    RB_UNUSED_PARAM;
    
    if (!getInput(self).getControllerConnected())
        return rb_utf8_str_new_cstr("");
    
    return rb_utf8_str_new_cstr(getInput(self).getControllerName());
}

RB_METHOD(inputControllerPowerLevel) {
    RB_UNUSED_PARAM;
    
    VALUE ret;
    
    if (!getInput(self).getControllerConnected())
        ret = M_SYMBOL("UNKNOWN");
    
    switch (getInput(self).getControllerPowerLevel()) {
            POWERCASE(ret, MAX);
            POWERCASE(ret, WIRED);
            POWERCASE(ret, FULL);
            POWERCASE(ret, MEDIUM);
            POWERCASE(ret, LOW);
            POWERCASE(ret, EMPTY);
            
        default:
            ret = M_SYMBOL("UNKNOWN");
            break;
    }
    
    return ret;
}

#define AXISFUNC(n, ax1, ax2) \
RB_METHOD(inputControllerGet##n##Axis) {\
RB_UNUSED_PARAM;\
VALUE ret = rb_ary_new(); \
if (!shState->eThread().getControllerConnected()) {\
rb_ary_push(ret, rb_float_new(0)); rb_ary_push(ret, rb_float_new(0)); \
}\
rb_ary_push(ret, rb_float_new(getInput(self).getControllerAxisValue(SDL_CONTROLLER_AXIS_##ax1) / 32767.0)); \
rb_ary_push(ret, rb_float_new(getInput(self).getControllerAxisValue(SDL_CONTROLLER_AXIS_##ax2) / 32767.0)); \
return ret; \
}

AXISFUNC(Left, LEFTX, LEFTY);
AXISFUNC(Right, RIGHTX, RIGHTY);
AXISFUNC(Trigger, TRIGGERLEFT, TRIGGERRIGHT);

#undef POWERCASE
#undef M_SYMBOL

RB_METHOD_GUARD(inputControllerPressEx) {
    RB_UNUSED_PARAM;
    
    VALUE button;
    rb_scan_args(argc, argv, "1", &button);
    
    if (SYMBOL_P(button)) {
        int num = getControllerButtonArg(&button);
        return rb_bool_new(getInput(self).controllerIsPressedEx(num));
    }
    
    return rb_bool_new(getInput(self).controllerIsPressedEx(NUM2INT(button)));
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(inputControllerTriggerEx) {
    RB_UNUSED_PARAM;
    
    VALUE button;
    rb_scan_args(argc, argv, "1", &button);
    
    if (SYMBOL_P(button)) {
        int num = getControllerButtonArg(&button);
        return rb_bool_new(getInput(self).controllerIsTriggeredEx(num));
    }
    
    return rb_bool_new(getInput(self).controllerIsTriggeredEx(NUM2INT(button)));
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(inputControllerRepeatEx) {
    RB_UNUSED_PARAM;
    
    VALUE button;
    rb_scan_args(argc, argv, "1", &button);
    
    if (SYMBOL_P(button)) {
        int num = getControllerButtonArg(&button);
        return rb_bool_new(getInput(self).controllerIsRepeatedEx(num));
    }
    
    return rb_bool_new(getInput(self).controllerIsRepeatedEx(NUM2INT(button)));
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(inputControllerReleaseEx) {
    RB_UNUSED_PARAM;
    
    VALUE button;
    rb_scan_args(argc, argv, "1", &button);
    
    if (SYMBOL_P(button)) {
        int num = getControllerButtonArg(&button);
        return rb_bool_new(getInput(self).controllerIsReleasedEx(num));
    }
    
    return rb_bool_new(getInput(self).controllerIsReleasedEx(NUM2INT(button)));
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(inputControllerCountEx) {
    RB_UNUSED_PARAM;
    
    VALUE button;
    rb_scan_args(argc, argv, "1", &button);
    
    if (SYMBOL_P(button)) {
        int num = getControllerButtonArg(&button);
        return rb_bool_new(getInput(self).controllerRepeatcount(num));
    }
    
    return rb_bool_new(getInput(self).controllerRepeatcount(NUM2INT(button)));
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(inputControllerRepeatTimeEx) {
    RB_UNUSED_PARAM;
    
    VALUE button;
    rb_scan_args(argc, argv, "1", &button);
    
    if (SYMBOL_P(button)) {
        int num = getControllerButtonArg(&button);
        return rb_float_new(getInput(self).controllerRepeatTimeEx(num));
    }
    
    return rb_float_new(getInput(self).controllerRepeatTimeEx(NUM2INT(button)));
}
RB_METHOD_GUARD_END

RB_METHOD(inputControllerRawButtonStates) {
    RB_UNUSED_PARAM;
    
    VALUE ret = rb_ary_new();
    uint8_t *states = getInput(self).rawButtonStates();
    
    for (unsigned int i = 0; i < getInput(self).rawButtonStatesLength(); i++)
        rb_ary_push(ret, rb_bool_new(states[i]));
    
    return ret;
}

RB_METHOD(inputControllerRawAxes) {
    RB_UNUSED_PARAM;
    
    VALUE ret = rb_ary_new();
    int16_t *states = getInput(self).rawAxes();
    
    for (unsigned int i = 0; i < getInput(self).rawAxesLength(); i++)
        rb_ary_push(ret, rb_float_new(states[i] / 32767.0));
    
    return ret;
}

RB_METHOD(inputGetMode) {
    RB_UNUSED_PARAM;
    
    return rb_bool_new(getInput(self).getTextInputMode());
}

RB_METHOD(inputSetMode) {
    RB_UNUSED_PARAM;
    
    bool mode;
    rb_get_args(argc, argv, "b", &mode RB_ARG_END);
    
    getInput(self).setTextInputMode(mode);
    
    return mode;
}

RB_METHOD(inputGets) {
    RB_UNUSED_PARAM;
    shState->eThread().lockText(true);
    VALUE ret = rb_utf8_str_new_cstr(getInput(self).getText());
    getInput(self).clearText();
    shState->eThread().lockText(false);
    return ret;
}

RB_METHOD_GUARD(inputGetClipboard) {
    RB_UNUSED_PARAM;
    return rb_utf8_str_new_cstr(getInput(self).getClipboardText());
}
RB_METHOD_GUARD_END

RB_METHOD_GUARD(inputSetClipboard) {
    RB_UNUSED_PARAM;
    
    VALUE str;
    rb_scan_args(argc, argv, "1", &str);
    
    SafeStringValue(str);
    
    getInput(self).setClipboardText(RSTRING_PTR(str));
    
    return str;
}
RB_METHOD_GUARD_END

struct {
    const char *str;
    Input::ButtonCode val;
} static buttonCodes[] = {{"DOWN", Input::Down},
    {"LEFT", Input::Left},
    {"RIGHT", Input::Right},
    {"UP", Input::Up},
    {"C", Input::C},
    {"Z", Input::Z},
    {"A", Input::A},
    {"B", Input::B},
    {"X", Input::X},
    {"Y", Input::Y},
    {"L", Input::L},
    {"R", Input::R},
    {"SHIFT", Input::Shift},
    {"CTRL", Input::Ctrl},
    {"ALT", Input::Alt},
    {"F5", Input::F5},
    {"F6", Input::F6},
    {"F7", Input::F7},
    {"F8", Input::F8},
    {"F9", Input::F9},
    
    {"MOUSELEFT", Input::MouseLeft},
    {"MOUSEMIDDLE", Input::MouseMiddle},
    {"MOUSERIGHT", Input::MouseRight},
    {"MOUSEX1", Input::MouseX1},
    {"MOUSEX2", Input::MouseX2}
};

static elementsN(buttonCodes);

#define DEFINE_INPUT_BINDING(name, function) do { \
    _rb_define_module_function(module, #name, function); \
    _rb_define_method(klass, #name, function); \
} while (0)

#define DEFINE_CONTROLLER_BINDING(name, function) do { \
    _rb_define_module_function(submod, #name, function); \
    _rb_define_method(klass, "controller_" #name, function); \
} while (0)

void inputBindingInit() {
    VALUE module = rb_define_module("Input");
    VALUE klass = rb_define_class("InputInstance", rb_cObject);
#if RAPI_FULL > 187
    rb_define_alloc_func(klass, classAllocate<&InputType>);
#else
    rb_define_alloc_func(klass, InputAllocate);
#endif
    _rb_define_method(klass, "initialize", inputInitialize);

    DEFINE_INPUT_BINDING(delta, inputDelta);
    DEFINE_INPUT_BINDING(update, inputUpdate);
    DEFINE_INPUT_BINDING(press?, inputPress);
    DEFINE_INPUT_BINDING(trigger?, inputTrigger);
    DEFINE_INPUT_BINDING(repeat?, inputRepeat);
    DEFINE_INPUT_BINDING(release?, inputRelease);
    DEFINE_INPUT_BINDING(count, inputCount);
    DEFINE_INPUT_BINDING(time?, inputRepeatTime);
    DEFINE_INPUT_BINDING(pressex?, inputPressEx);
    DEFINE_INPUT_BINDING(triggerex?, inputTriggerEx);
    DEFINE_INPUT_BINDING(repeatex?, inputRepeatEx);
    DEFINE_INPUT_BINDING(releaseex?, inputReleaseEx);
    DEFINE_INPUT_BINDING(repeatcount, inputCountEx);
    DEFINE_INPUT_BINDING(timeex?, inputRepeatTimeEx);
    DEFINE_INPUT_BINDING(dir4, inputDir4);
    DEFINE_INPUT_BINDING(dir8, inputDir8);

    DEFINE_INPUT_BINDING(mouse_x, inputMouseX);
    DEFINE_INPUT_BINDING(mouse_y, inputMouseY);
    DEFINE_INPUT_BINDING(scroll_v, inputScrollV);
    DEFINE_INPUT_BINDING(mouse_in_window, inputMouseInWindow);
    DEFINE_INPUT_BINDING(mouse_in_window?, inputMouseInWindow);

    DEFINE_INPUT_BINDING(raw_key_states, inputRawKeyStates);

    VALUE submod = rb_define_module_under(module, "Controller");
    DEFINE_CONTROLLER_BINDING(connected?, inputControllerConnected);
    DEFINE_CONTROLLER_BINDING(name, inputControllerName);
    DEFINE_CONTROLLER_BINDING(power_level, inputControllerPowerLevel);
    DEFINE_CONTROLLER_BINDING(axes_left, inputControllerGetLeftAxis);
    DEFINE_CONTROLLER_BINDING(axes_right, inputControllerGetRightAxis);
    DEFINE_CONTROLLER_BINDING(axes_trigger, inputControllerGetTriggerAxis);
    DEFINE_CONTROLLER_BINDING(raw_button_states, inputControllerRawButtonStates);
    DEFINE_CONTROLLER_BINDING(raw_axes, inputControllerRawAxes);
    DEFINE_CONTROLLER_BINDING(pressex?, inputControllerPressEx);
    DEFINE_CONTROLLER_BINDING(triggerex?, inputControllerTriggerEx);
    DEFINE_CONTROLLER_BINDING(repeatex?, inputControllerRepeatEx);
    DEFINE_CONTROLLER_BINDING(releaseex?, inputControllerReleaseEx);
    DEFINE_CONTROLLER_BINDING(repeatcount, inputControllerCountEx);
    DEFINE_CONTROLLER_BINDING(timeex?, inputControllerRepeatTimeEx);

    DEFINE_INPUT_BINDING(text_input, inputGetMode);
    DEFINE_INPUT_BINDING(text_input=, inputSetMode);
    DEFINE_INPUT_BINDING(gets, inputGets);

    DEFINE_INPUT_BINDING(clipboard, inputGetClipboard);
    DEFINE_INPUT_BINDING(clipboard=, inputSetClipboard);

    if (rgssVer >= 3) {
        VALUE symHash = rb_hash_new();
        
        for (size_t i = 0; i < buttonCodesN; ++i) {
            ID sym = rb_intern(buttonCodes[i].str);
            VALUE val = INT2FIX(buttonCodes[i].val);
            
            /* In RGSS3 all Input::XYZ constants are equal to :XYZ symbols,
             * to be compatible with the previous convention */
            rb_const_set(module, sym, ID2SYM(sym));
            rb_hash_aset(symHash, ID2SYM(sym), val);
        }
        
        rb_iv_set(module, "buttoncodes", symHash);
        getRbData()->buttoncodeHash = symHash;
    } else {
        for (size_t i = 0; i < buttonCodesN; ++i) {
            ID sym = rb_intern(buttonCodes[i].str);
            VALUE val = INT2FIX(buttonCodes[i].val);
            
            rb_const_set(module, sym, val);
        }
    }
}
