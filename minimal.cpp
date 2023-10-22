//
// Created by fcors on 10/20/2023.
//
#include "minimal.h"

void tableBindingInit();

void etcBindingInit();

void fontBindingInit();

void bitmapBindingInit();

void spriteBindingInit();

void viewportBindingInit();

void planeBindingInit();

void windowBindingInit();

void tilemapBindingInit();

void windowVXBindingInit();

void tilemapVXBindingInit();

void inputBindingInit();

void audioBindingInit();

void graphicsBindingInit();

void fileIntBindingInit();

void httpBindingInit();

MKXPZ_EXPORT void initBindings() {
    tableBindingInit();
    etcBindingInit();
    fontBindingInit();
    fileIntBindingInit();
    bitmapBindingInit();
    spriteBindingInit();
    viewportBindingInit();
    windowBindingInit();
    tilemapBindingInit();
    planeBindingInit();
    audioBindingInit();
    inputBindingInit();
    //httpBindingInit();
    graphicsBindingInit();
}