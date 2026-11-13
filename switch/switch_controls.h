#pragma once

// Controller profiles: named mappings from Switch controller inputs to
// keyboard keys, edited in the game picker's Settings and stored in
// sdmc:/switch/mkxp-z/controls.json. The built-in Default profile keeps the
// engine's own controller bindings (src/input/keybindings.cpp); a custom
// profile replaces them and presses keyboard keys instead.
//
// This part has no libnx dependency so it can be tested on the host
// (switch/tests/controls).

#include <SDL_scancode.h>
#include <SDL_gamecontroller.h>

#include <stdint.h>
#include <string>
#include <vector>

namespace SwitchControls {

// Switch inputs, by their Switch labels. Stick directions count as buttons.
enum Input {
    In_A, In_B, In_X, In_Y,
    In_L, In_R, In_ZL, In_ZR,
    In_Plus, In_Minus, In_LStick, In_RStick,
    In_DUp, In_DDown, In_DLeft, In_DRight,
    In_LSUp, In_LSDown, In_LSLeft, In_LSRight,
    In_RSUp, In_RSDown, In_RSLeft, In_RSRight,
    InputCount
};

// Shown in the editor, e.g. "Left stick up"
const char *inputLabel(int input);
// Key in controls.json, e.g. "LStickUp"
const char *inputId(int input);

struct Profile {
    std::string name;
    std::vector<SDL_Scancode> keys[InputCount];
};

// Today's controller layout written as keys. Shown read-only for Default and
// used as the starting point for copies.
const Profile &defaultProfile();
const char *const DEFAULT_NAME = "Default";

// Custom profiles (never Default) and the active profile's name; an empty or
// unknown name means Default.
struct Store {
    std::vector<Profile> profiles;
    std::string active;
};

// Never throws: anything unreadable gives an empty store (Default active).
Store parseStore(const std::string &json);
std::string serializeStore(const Store &store);
Store loadStore(const char *path);
bool saveStore(const char *path, const Store &store);

Profile *findProfile(Store &store, const std::string &name);
const Profile *findProfile(const Store &store, const std::string &name);
// name, or name with " (2)", " (3)", ... so it clashes with no profile and
// isn't "Default"
std::string uniqueName(const Store &store, const std::string &name);

// SDL scancode names ("Return", "Left Shift", "F6"), as stored in the file
std::string keyName(SDL_Scancode key);
// "Escape + F6", or "nothing"
std::string keysLabel(const std::vector<SDL_Scancode> &keys);

struct KeyGroup {
    const char *name;
    std::vector<SDL_Scancode> keys;
};
// Every key offered in the editor, grouped for jumping
const std::vector<KeyGroup> &keyGroups();

// Whether a Switch input is held, from SDL game controller state (SDL's
// Switch mapping is positional: its "B" is the Switch A button)
bool inputPressed(int input, const bool *buttons, const int *axes);

// Sets keyStates for every key the profile uses: pressed while any input
// bound to it is held. Keys the profile doesn't use are left alone.
void applyKeys(const Profile &profile, const bool *buttons, const int *axes,
               uint8_t *keyStates);

} // namespace SwitchControls

#define SWITCH_CONTROLS_FILE "sdmc:/switch/mkxp-z/controls.json"

// Engine hooks (src/main.cpp, src/input/keybindings.cpp, src/eventthread.cpp)
extern "C" {
// Loads controls.json and selects the active profile for this launch.
void switch_controls_load_active();
// True when a custom profile drives the controller (not Default)
bool switch_controls_custom_active();
// Updates keyStates from the controller after each controller event
void switch_controls_update(const bool *buttons, const int *axes, uint8_t *keyStates);
}
