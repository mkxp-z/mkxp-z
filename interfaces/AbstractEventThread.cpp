//
// Created by fcors on 10/21/2023.
//
#include "AbstractEventThread.h"

uint8_t AbstractEventThread::keyStates[];
AbstractEventThread::ControllerState AbstractEventThread::controllerState;
AbstractEventThread::MouseState AbstractEventThread::mouseState;
AbstractEventThread::TouchState AbstractEventThread::touchState;
SDL_atomic_t AbstractEventThread::verticalScrollDistance;

AbstractEventThread::~AbstractEventThread() = default;
