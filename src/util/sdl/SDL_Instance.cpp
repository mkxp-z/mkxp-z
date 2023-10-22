//
// Created by fcors on 10/17/2023.
//
#include "SDL_Instance.h"

SDL_Instance::SDL_Instance() : m_core(new SDL_Core()) {
}

std::shared_ptr<SDL_Instance> SDL_Instance::create(Uint32 flags) {
    if (SDL_Init(flags) >= 0)
        return std::shared_ptr<SDL_Instance>(new SDL_Instance());
    else
        return nullptr;
}

bool SDL_Instance::initIMG(int flags) {
    if (IMG_Init(flags) == flags) {
        m_img = std::unique_ptr<IMG_Instance>(new IMG_Instance());
        return true;
    }

    return false;
}

bool SDL_Instance::initTTF() {
    if (TTF_Init() >= 0) {
        m_ttf = std::unique_ptr<TTF_Instance>(new TTF_Instance());
        return true;
    }

    return false;
}

bool SDL_Instance::initSound() {
    if (Sound_Init() != 0) {
        m_sound = std::unique_ptr<Sound_Instance>(new Sound_Instance());
        return true;
    }
    return false;
}
