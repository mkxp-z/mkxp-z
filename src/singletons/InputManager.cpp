//
// Created by fcors on 10/21/2023.
//

#include "InputManager.h"

InputManager::InputManager() {

}

InputManager::~InputManager() {

}

InputManager &InputManager::getInstance() {
    static InputManager inputManager;
    return inputManager;
}

IInput &InputManager::getInput() const {
    return *m_input;
}
