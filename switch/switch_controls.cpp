#include "switch_controls.h"

#include "util/json5pp.hpp"
#include "switch-trace.h"

#include <SDL_keyboard.h>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace json = json5pp;

namespace SwitchControls {

namespace {

struct InputInfo {
    const char *id;
    const char *label;
};

const InputInfo INPUTS[InputCount] = {
    {"A", "A"}, {"B", "B"}, {"X", "X"}, {"Y", "Y"},
    {"L", "L"}, {"R", "R"}, {"ZL", "ZL"}, {"ZR", "ZR"},
    {"Plus", "+"}, {"Minus", "-"}, {"LStick", "L3 (left stick press)"},
    {"RStick", "R3 (right stick press)"},
    {"DUp", "D-pad up"}, {"DDown", "D-pad down"},
    {"DLeft", "D-pad left"}, {"DRight", "D-pad right"},
    {"LStickUp", "Left stick up"}, {"LStickDown", "Left stick down"},
    {"LStickLeft", "Left stick left"}, {"LStickRight", "Left stick right"},
    {"RStickUp", "Right stick up"}, {"RStickDown", "Right stick down"},
    {"RStickLeft", "Right stick left"}, {"RStickRight", "Right stick right"},
};

// Same threshold the engine uses for stick and trigger bindings
// (JAXIS_THRESHOLD in src/input/keybindings.h)
const int AXIS_THRESHOLD = 0x4000;

Profile makeDefault() {
    Profile p;
    p.name = DEFAULT_NAME;
    // Keys from mkxp-z's standard keyboard bindings, so each reaches the RGSS
    // button the engine's own controller binding does
    p.keys[In_A] = {SDL_SCANCODE_RETURN};
    p.keys[In_B] = {SDL_SCANCODE_X};
    p.keys[In_X] = {SDL_SCANCODE_LSHIFT, SDL_SCANCODE_D};
    p.keys[In_Y] = {SDL_SCANCODE_ESCAPE, SDL_SCANCODE_F6};
    p.keys[In_Minus] = p.keys[In_X];
    p.keys[In_Plus] = p.keys[In_Y];
    p.keys[In_L] = {SDL_SCANCODE_Q};
    p.keys[In_R] = {SDL_SCANCODE_D};
    p.keys[In_ZL] = {SDL_SCANCODE_F5};
    p.keys[In_ZR] = {SDL_SCANCODE_W};
    p.keys[In_DUp] = p.keys[In_LSUp] = {SDL_SCANCODE_UP};
    p.keys[In_DDown] = p.keys[In_LSDown] = {SDL_SCANCODE_DOWN};
    p.keys[In_DLeft] = p.keys[In_LSLeft] = {SDL_SCANCODE_LEFT};
    p.keys[In_DRight] = p.keys[In_LSRight] = {SDL_SCANCODE_RIGHT};
    p.keys[In_RSUp] = {SDL_SCANCODE_A};
    p.keys[In_RSDown] = {SDL_SCANCODE_S};
    return p;
}

std::vector<SDL_Scancode> range(SDL_Scancode first, SDL_Scancode last) {
    std::vector<SDL_Scancode> v;
    for (int k = first; k <= last; ++k)
        v.push_back((SDL_Scancode)k);
    return v;
}

std::vector<KeyGroup> makeKeyGroups() {
    return {
        {"Common", {SDL_SCANCODE_RETURN, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_SPACE,
                    SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_TAB, SDL_SCANCODE_LSHIFT,
                    SDL_SCANCODE_RSHIFT, SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL,
                    SDL_SCANCODE_LALT, SDL_SCANCODE_RALT, SDL_SCANCODE_CAPSLOCK}},
        {"Letters", range(SDL_SCANCODE_A, SDL_SCANCODE_Z)},
        {"Numbers", range(SDL_SCANCODE_1, SDL_SCANCODE_0)},
        {"Arrows and navigation", {SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT,
                                   SDL_SCANCODE_RIGHT, SDL_SCANCODE_HOME, SDL_SCANCODE_END,
                                   SDL_SCANCODE_PAGEUP, SDL_SCANCODE_PAGEDOWN,
                                   SDL_SCANCODE_INSERT, SDL_SCANCODE_DELETE}},
        {"Function keys", range(SDL_SCANCODE_F1, SDL_SCANCODE_F12)},
        {"Symbols", {SDL_SCANCODE_MINUS, SDL_SCANCODE_EQUALS, SDL_SCANCODE_LEFTBRACKET,
                     SDL_SCANCODE_RIGHTBRACKET, SDL_SCANCODE_BACKSLASH,
                     SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_APOSTROPHE, SDL_SCANCODE_GRAVE,
                     SDL_SCANCODE_COMMA, SDL_SCANCODE_PERIOD, SDL_SCANCODE_SLASH}},
        {"Keypad", range(SDL_SCANCODE_KP_DIVIDE, SDL_SCANCODE_KP_PERIOD)},
    };
}

json::value profileToJson(const Profile &p) {
    json::value bindings = json::object({});
    for (int i = 0; i < InputCount; ++i) {
        json::value keys = json::array({});
        for (SDL_Scancode k : p.keys[i])
            keys.as_array().push_back(json::value(keyName(k)));
        bindings.as_object()[INPUTS[i].id] = keys;
    }
    return json::object({{"name", json::value(p.name)}, {"bindings", bindings}});
}

bool profileFromJson(const json::value &v, Profile &p) {
    if (!v.is_object())
        return false;
    const json::value::object_type &o = v.as_object();
    auto name = o.find("name");
    auto bindings = o.find("bindings");
    if (name == o.end() || !name->second.is_string() || name->second.as_string().empty())
        return false;
    p.name = name->second.as_string();
    if (bindings == o.end() || !bindings->second.is_object())
        return true;
    const json::value::object_type &b = bindings->second.as_object();
    for (int i = 0; i < InputCount; ++i) {
        auto it = b.find(INPUTS[i].id);
        if (it == b.end() || !it->second.is_array())
            continue;
        for (const json::value &k : it->second.as_array()) {
            if (!k.is_string())
                continue;
            SDL_Scancode sc = SDL_GetScancodeFromName(k.as_string().c_str());
            if (sc != SDL_SCANCODE_UNKNOWN)
                p.keys[i].push_back(sc);
        }
    }
    return true;
}

} // namespace

const char *inputLabel(int input) { return INPUTS[input].label; }
const char *inputId(int input) { return INPUTS[input].id; }

const Profile &defaultProfile() {
    static const Profile p = makeDefault();
    return p;
}

Store parseStore(const std::string &text) {
    Store s;
    try {
        json::value v = json::parse5(text);
        if (!v.is_object())
            return s;
        const json::value::object_type &o = v.as_object();
        auto active = o.find("active");
        auto profiles = o.find("profiles");
        if (active != o.end() && active->second.is_string())
            s.active = active->second.as_string();
        if (profiles != o.end() && profiles->second.is_array()) {
            for (const json::value &pv : profiles->second.as_array()) {
                Profile p;
                if (profileFromJson(pv, p) && p.name != DEFAULT_NAME &&
                    !findProfile(s, p.name))
                    s.profiles.push_back(p);
            }
        }
    } catch (const std::exception &) {
        return Store();
    }
    return s;
}

std::string serializeStore(const Store &store) {
    json::value profiles = json::array({});
    for (const Profile &p : store.profiles)
        profiles.as_array().push_back(profileToJson(p));
    json::value v = json::object({{"active", json::value(store.active)},
                                  {"profiles", profiles}});
    std::ostringstream out;
    out << json::rule::space_indent<2>() << v << "\n";
    return out.str();
}

Store loadStore(const char *path) {
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return Store();
    std::stringstream ss;
    ss << f.rdbuf();
    return parseStore(ss.str());
}

bool saveStore(const char *path, const Store &store) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f)
        return false;
    f << serializeStore(store);
    return (bool)f;
}

Profile *findProfile(Store &store, const std::string &name) {
    for (Profile &p : store.profiles)
        if (p.name == name)
            return &p;
    return nullptr;
}

const Profile *findProfile(const Store &store, const std::string &name) {
    return findProfile(const_cast<Store &>(store), name);
}

std::string uniqueName(const Store &store, const std::string &name) {
    auto taken = [&](const std::string &n) {
        return n == DEFAULT_NAME || findProfile(store, n) != nullptr;
    };
    if (!taken(name))
        return name;
    for (int i = 2;; ++i) {
        std::string candidate = name + " (" + std::to_string(i) + ")";
        if (!taken(candidate))
            return candidate;
    }
}

std::string keyName(SDL_Scancode key) { return SDL_GetScancodeName(key); }

std::string keysLabel(const std::vector<SDL_Scancode> &keys) {
    if (keys.empty())
        return "nothing";
    std::string s;
    for (SDL_Scancode k : keys)
        s += (s.empty() ? "" : " + ") + keyName(k);
    return s;
}

const std::vector<KeyGroup> &keyGroups() {
    static const std::vector<KeyGroup> groups = makeKeyGroups();
    return groups;
}

bool inputPressed(int input, const bool *buttons, const int *axes) {
    switch (input) {
    case In_A: return buttons[SDL_CONTROLLER_BUTTON_B];
    case In_B: return buttons[SDL_CONTROLLER_BUTTON_A];
    case In_X: return buttons[SDL_CONTROLLER_BUTTON_Y];
    case In_Y: return buttons[SDL_CONTROLLER_BUTTON_X];
    case In_L: return buttons[SDL_CONTROLLER_BUTTON_LEFTSHOULDER];
    case In_R: return buttons[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER];
    case In_ZL: return axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT] > AXIS_THRESHOLD;
    case In_ZR: return axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] > AXIS_THRESHOLD;
    case In_Plus: return buttons[SDL_CONTROLLER_BUTTON_START];
    case In_Minus: return buttons[SDL_CONTROLLER_BUTTON_BACK];
    case In_LStick: return buttons[SDL_CONTROLLER_BUTTON_LEFTSTICK];
    case In_RStick: return buttons[SDL_CONTROLLER_BUTTON_RIGHTSTICK];
    case In_DUp: return buttons[SDL_CONTROLLER_BUTTON_DPAD_UP];
    case In_DDown: return buttons[SDL_CONTROLLER_BUTTON_DPAD_DOWN];
    case In_DLeft: return buttons[SDL_CONTROLLER_BUTTON_DPAD_LEFT];
    case In_DRight: return buttons[SDL_CONTROLLER_BUTTON_DPAD_RIGHT];
    case In_LSUp: return axes[SDL_CONTROLLER_AXIS_LEFTY] < -AXIS_THRESHOLD;
    case In_LSDown: return axes[SDL_CONTROLLER_AXIS_LEFTY] > AXIS_THRESHOLD;
    case In_LSLeft: return axes[SDL_CONTROLLER_AXIS_LEFTX] < -AXIS_THRESHOLD;
    case In_LSRight: return axes[SDL_CONTROLLER_AXIS_LEFTX] > AXIS_THRESHOLD;
    case In_RSUp: return axes[SDL_CONTROLLER_AXIS_RIGHTY] < -AXIS_THRESHOLD;
    case In_RSDown: return axes[SDL_CONTROLLER_AXIS_RIGHTY] > AXIS_THRESHOLD;
    case In_RSLeft: return axes[SDL_CONTROLLER_AXIS_RIGHTX] < -AXIS_THRESHOLD;
    case In_RSRight: return axes[SDL_CONTROLLER_AXIS_RIGHTX] > AXIS_THRESHOLD;
    }
    return false;
}

void applyKeys(const Profile &profile, const bool *buttons, const int *axes,
               uint8_t *keyStates) {
    // Clear every key the profile uses, then press the held inputs' keys, so
    // a key shared by two inputs stays down while either is held
    for (int i = 0; i < InputCount; ++i)
        for (SDL_Scancode k : profile.keys[i])
            keyStates[k] = 0;
    for (int i = 0; i < InputCount; ++i)
        if (inputPressed(i, buttons, axes))
            for (SDL_Scancode k : profile.keys[i])
                keyStates[k] = 1;
}

} // namespace SwitchControls

// The profile chosen for this launch; null means Default
static SwitchControls::Profile *activeProfile;

extern "C" void switch_controls_load_active() {
    SwitchControls::Store store = SwitchControls::loadStore(SWITCH_CONTROLS_FILE);
    if (const SwitchControls::Profile *p = SwitchControls::findProfile(store, store.active))
        activeProfile = new SwitchControls::Profile(*p);
    SWITCH_TRACE("controls: using %s profile '%s'\n", activeProfile ? "custom" : "built-in",
                 activeProfile ? activeProfile->name.c_str() : SwitchControls::DEFAULT_NAME);
}

extern "C" bool switch_controls_custom_active() { return activeProfile != nullptr; }

extern "C" void switch_controls_update(const bool *buttons, const int *axes, uint8_t *keyStates) {
    if (activeProfile)
        SwitchControls::applyKeys(*activeProfile, buttons, axes, keyStates);
}
