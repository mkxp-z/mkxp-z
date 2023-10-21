//
// Created by fcors on 10/20/2023.
//
#include "minimal.h"

void etcBindingInit();
void fontBindingInit();

MKXPZ_EXPORT void initBindings() {
    etcBindingInit();
    fontBindingInit();
}