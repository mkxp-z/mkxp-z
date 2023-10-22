//
// Created by fcors on 10/21/2023.
//

#pragma once

#include "IInput.h"

#include <memory>

/*
#define INPUT_MANAGER InputManager::getInstance()
#define shState->input() InputManager::getInstance().getInput()
*/

class InputManager {
    InputManager();

    ~InputManager();

public:
    static InputManager &getInstance();

    bool init();

    IInput &getInput() const;

private:
    std::unique_ptr<IInput> m_input;
};
