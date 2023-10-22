//
// Created by fcors on 10/17/2023.
//
#pragma once

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_sound.h>

#include <memory>

#define QUIT_HELPER(name, function) class name { \
                                      private:   \
                                        name() = default; \
                                      public: \
                                        ~name() { \
                                            function(); \
                                        }        \
                                                 \
                                      private:   \
                                        friend class SDL_Instance; \
                                    }; \

QUIT_HELPER(SDL_Core, SDL_Quit)
QUIT_HELPER(IMG_Instance, IMG_Quit)
QUIT_HELPER(TTF_Instance, TTF_Quit)
QUIT_HELPER(Sound_Instance, Sound_Quit)

class SDL_Instance {
private:
    SDL_Instance();

public:
    static std::shared_ptr<SDL_Instance> create(Uint32 flags);

    bool initIMG(int flags);
    bool initTTF();
    bool initSound();

private:
    // This is used to ensure that SDL_Quit is always called last
    // as the destructors are called in reverse order of declaration
    std::unique_ptr<SDL_Core> m_core;
    std::unique_ptr<IMG_Instance> m_img;
    std::unique_ptr<TTF_Instance> m_ttf;
    std::unique_ptr<Sound_Instance> m_sound;
};
