// Host-side tests for switch/switch_controls.cpp (no libnx needed).
// Run: meson test -C build controls (switch/tests/controls/meson.build)

#include "switch_controls.h"

#include <SDL_gamecontroller.h>

#include <cstdio>
#include <cstring>
#include <string>

using namespace SwitchControls;

static int failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

static std::vector<SDL_Scancode> keys(std::initializer_list<SDL_Scancode> k) { return k; }

static void test_default_profile_matches_spec() {
    const Profile &d = defaultProfile();
    CHECK(d.name == "Default");
    CHECK(d.keys[In_A] == keys({SDL_SCANCODE_RETURN}));
    CHECK(d.keys[In_B] == keys({SDL_SCANCODE_X}));
    CHECK(d.keys[In_X] == keys({SDL_SCANCODE_LSHIFT, SDL_SCANCODE_D}));
    CHECK(d.keys[In_Y] == keys({SDL_SCANCODE_ESCAPE, SDL_SCANCODE_F6}));
    CHECK(d.keys[In_Minus] == keys({SDL_SCANCODE_LSHIFT, SDL_SCANCODE_D}));
    CHECK(d.keys[In_Plus] == keys({SDL_SCANCODE_ESCAPE, SDL_SCANCODE_F6}));
    CHECK(d.keys[In_L] == keys({SDL_SCANCODE_Q}));
    CHECK(d.keys[In_R] == keys({SDL_SCANCODE_D}));
    CHECK(d.keys[In_ZL] == keys({SDL_SCANCODE_F5}));
    CHECK(d.keys[In_ZR] == keys({SDL_SCANCODE_W}));
    CHECK(d.keys[In_DUp] == keys({SDL_SCANCODE_UP}));
    CHECK(d.keys[In_LSLeft] == keys({SDL_SCANCODE_LEFT}));
    CHECK(d.keys[In_RSUp] == keys({SDL_SCANCODE_A}));
    CHECK(d.keys[In_RSDown] == keys({SDL_SCANCODE_S}));
    CHECK(d.keys[In_RSLeft].empty());
    CHECK(d.keys[In_LStick].empty());
    CHECK(d.keys[In_RStick].empty());
}

static void test_store_round_trip() {
    Store s;
    Profile p = defaultProfile();
    p.name = "Lefty";
    p.keys[In_A] = keys({SDL_SCANCODE_Z});
    p.keys[In_ZL].clear();
    s.profiles.push_back(p);
    s.active = "Lefty";

    Store r = parseStore(serializeStore(s));
    CHECK(r.active == "Lefty");
    CHECK(r.profiles.size() == 1);
    const Profile *q = findProfile(r, "Lefty");
    CHECK(q != nullptr);
    if (q) {
        CHECK(q->keys[In_A] == keys({SDL_SCANCODE_Z}));
        CHECK(q->keys[In_ZL].empty());
        CHECK(q->keys[In_Y] == keys({SDL_SCANCODE_ESCAPE, SDL_SCANCODE_F6}));
    }
}

static void test_bad_json_means_default() {
    Store s = parseStore("this is not json {");
    CHECK(s.profiles.empty());
    CHECK(s.active.empty());
    CHECK(findProfile(s, s.active) == nullptr);

    Store t = parseStore("[1, 2, 3]");
    CHECK(t.profiles.empty());
}

static void test_unknown_keys_and_inputs_are_skipped() {
    Store s = parseStore(R"({"active":"P","profiles":[{"name":"P","bindings":)"
                         R"({"A":["Return","NotAKey"],"Banana":["X"],"B":"Escape"}}]})");
    const Profile *p = findProfile(s, "P");
    CHECK(p != nullptr);
    if (p) {
        CHECK(p->keys[In_A] == keys({SDL_SCANCODE_RETURN}));
        CHECK(p->keys[In_B].empty()); // not an array
    }
}

static void test_active_name_without_profile_is_default() {
    Store s = parseStore(R"({"active":"Gone","profiles":[]})");
    CHECK(findProfile(s, s.active) == nullptr);
}

static void test_inputs_use_switch_labels() {
    bool buttons[SDL_CONTROLLER_BUTTON_MAX] = {};
    int axes[SDL_CONTROLLER_AXIS_MAX] = {};
    // SDL's Switch mapping is positional: SDL "B" is the Switch A button
    buttons[SDL_CONTROLLER_BUTTON_B] = true;
    CHECK(inputPressed(In_A, buttons, axes));
    CHECK(!inputPressed(In_B, buttons, axes));
    buttons[SDL_CONTROLLER_BUTTON_B] = false;
    buttons[SDL_CONTROLLER_BUTTON_X] = true;
    CHECK(inputPressed(In_Y, buttons, axes));
    buttons[SDL_CONTROLLER_BUTTON_X] = false;
    buttons[SDL_CONTROLLER_BUTTON_START] = true;
    CHECK(inputPressed(In_Plus, buttons, axes));
}

static void test_axes_need_threshold() {
    bool buttons[SDL_CONTROLLER_BUTTON_MAX] = {};
    int axes[SDL_CONTROLLER_AXIS_MAX] = {};
    axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT] = 0x3000;
    CHECK(!inputPressed(In_ZL, buttons, axes));
    axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT] = 0x7fff;
    CHECK(inputPressed(In_ZL, buttons, axes));
    axes[SDL_CONTROLLER_AXIS_LEFTY] = -0x7000;
    CHECK(inputPressed(In_LSUp, buttons, axes));
    CHECK(!inputPressed(In_LSDown, buttons, axes));
    axes[SDL_CONTROLLER_AXIS_RIGHTX] = 0x7000;
    CHECK(inputPressed(In_RSRight, buttons, axes));
}

static void test_shared_key_held_until_all_released() {
    Profile p;
    p.keys[In_A] = keys({SDL_SCANCODE_RETURN});
    p.keys[In_X] = keys({SDL_SCANCODE_RETURN, SDL_SCANCODE_D});
    bool buttons[SDL_CONTROLLER_BUTTON_MAX] = {};
    int axes[SDL_CONTROLLER_AXIS_MAX] = {};
    uint8_t ks[SDL_NUM_SCANCODES] = {};
    ks[SDL_SCANCODE_K] = 1; // not used by the profile: must be left alone

    buttons[SDL_CONTROLLER_BUTTON_B] = true; // Switch A
    buttons[SDL_CONTROLLER_BUTTON_Y] = true; // Switch X
    applyKeys(p, buttons, axes, ks);
    CHECK(ks[SDL_SCANCODE_RETURN] && ks[SDL_SCANCODE_D]);

    buttons[SDL_CONTROLLER_BUTTON_Y] = false; // release X, A still held
    applyKeys(p, buttons, axes, ks);
    CHECK(ks[SDL_SCANCODE_RETURN]);
    CHECK(!ks[SDL_SCANCODE_D]);

    buttons[SDL_CONTROLLER_BUTTON_B] = false;
    applyKeys(p, buttons, axes, ks);
    CHECK(!ks[SDL_SCANCODE_RETURN]);
    CHECK(ks[SDL_SCANCODE_K]);
}

static void test_keys_label() {
    CHECK(keysLabel({}) == "nothing");
    CHECK(keysLabel(keys({SDL_SCANCODE_ESCAPE, SDL_SCANCODE_F6})) == "Escape + F6");
}

static void test_key_groups_cover_common_keys() {
    bool sawA = false, sawF12 = false, sawReturn = false;
    for (const KeyGroup &g : keyGroups())
        for (SDL_Scancode k : g.keys) {
            sawA |= k == SDL_SCANCODE_A;
            sawF12 |= k == SDL_SCANCODE_F12;
            sawReturn |= k == SDL_SCANCODE_RETURN;
        }
    CHECK(sawA && sawF12 && sawReturn);
}

static void test_unique_name() {
    Store s;
    Profile p;
    p.name = "Mine";
    s.profiles.push_back(p);
    CHECK(uniqueName(s, "Mine") == "Mine (2)");
    CHECK(uniqueName(s, "Default") == "Default (2)");
    CHECK(uniqueName(s, "Other") == "Other");
}

int main() {
    test_default_profile_matches_spec();
    test_store_round_trip();
    test_bad_json_means_default();
    test_unknown_keys_and_inputs_are_skipped();
    test_active_name_without_profile_is_default();
    test_inputs_use_switch_labels();
    test_axes_need_threshold();
    test_shared_key_held_until_all_released();
    test_keys_label();
    test_key_groups_cover_common_keys();
    test_unique_name();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all controls tests passed\n");
    return 0;
}
