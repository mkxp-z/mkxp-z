//
// Created by fcors on 10/21/2023.
//

#pragma once

#include "IInput.h"

#include <memory>

#define GAME_INPUT InputManager::getInstance().getInput()

class InputManager {
    InputManager();

    ~InputManager();

public:
    static InputManager &getInstance();

    IInput &getInput() const;

private:
    std::unique_ptr<IInput> m_input;
};
