//
// Created by fcors on 10/21/2023.
//

#pragma once

struct TEXFBO;

class ITexPool {
public:
    virtual ~ITexPool() = default;

    virtual TEXFBO request(int width, int height) = 0;

    virtual void release(TEXFBO &obj) = 0;

    virtual void disable() = 0;
};
