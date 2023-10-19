//
// Created by fcors on 10/19/2023.
//

#include "binding-mri.h"

int main(int argc, char **argv) {
    ruby_sysinit(&argc, &argv);
    RUBY_INIT_STACK
    ruby_init();

    Init_mkxp_z();
    return 0;
}