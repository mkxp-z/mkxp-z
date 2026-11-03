#pragma once

#include <string>
#include <vector>

struct PickerGame {
    std::string dir;  // full path to the game folder
    std::string name; // label shown in the picker
};

enum PickerResult { PICKER_PICKED, PICKER_QUIT, PICKER_UNAVAILABLE };

// Home-screen style picker drawn to a libnx framebuffer. sel is the initial
// and the chosen index. Returns PICKER_UNAVAILABLE, before touching the
// display, if its font or canvas cannot be set up.
PickerResult pickerUiRun(const std::vector<PickerGame> &games, int &sel);
