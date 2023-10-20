//
// Created by fcors on 10/19/2023.
//

#include "binding-mri.h"

int main(int argc, char **argv) {
    ruby_sysinit(&argc, &argv);
    RUBY_INIT_STACK
    ruby_init();

    initBindings();
    return 0;
}