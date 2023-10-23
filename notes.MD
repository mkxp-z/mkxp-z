This is just some random observations to the changes in the build procedure:

1. You need the UCRT version of MSYS2 using the following install command:
```shell
pacman -S git ruby vim base-devel \
          mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-meson \
          mingw-w64-ucrt-x86_64-autotools \
          mingw-w64-ucrt-x86_64-gcc
```
wh