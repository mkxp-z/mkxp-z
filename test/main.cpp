//
// Created by fcors on 10/19/2023.
//

//#include "binding-mri.h"
#include <ruby.h>

#include <windows.h>
#include <iostream>

int main(int argc, char **argv) {
    ruby_sysinit(&argc, &argv);
    RUBY_INIT_STACK
    ruby_init();

    auto lib = LoadLibrary("libmkxp_z.dll");
    auto err = GetLastError();
    if (err != 0)
        std::cout << "Error Code: " << err;

    //initBindings();
    return 0;
}