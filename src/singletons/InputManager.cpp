//
// Created by fcors on 10/21/2023.
//

#include "InputManager.h"
#include "ThreadManager.h"
#include "input.h"

InputManager::InputManager() {

}

InputManager::~InputManager() {

}

InputManager &InputManager::getInstance() {
    static InputManager inputManager;
    return inputManager;
}

bool InputManager::init() {
    if (!THREAD_MANAGER.isInitialized())
        return false;

    m_input = std::unique_ptr<Input>(new Input(*shState->rtData()));
    return true;
}

IInput &InputManager::getInput() const {
    return *m_input;
}
