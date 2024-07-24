#!/usr/bin/env bash

# Deps instructions:
# sudo dpkg --add-architecture riscv64
# (Target versions of xorg-dev; removes native versions of: libxfont-dev xorg-dev xserver-xorg-dev) sudo apt install libdmx-dev:riscv64 libfontenc-dev:riscv64 libfs-dev:riscv64 libice-dev:riscv64 libsm-dev:riscv64 libx11-dev:riscv64 libxau-dev:riscv64 libxaw7-dev:riscv64 libxcomposite-dev:riscv64 libxcursor-dev:riscv64 libxdamage-dev:riscv64 libxdmcp-dev:riscv64 libxext-dev:riscv64 libxfixes-dev:riscv64 libxfont-dev:riscv64 libxft-dev:riscv64 libxi-dev:riscv64 libxinerama-dev:riscv64 libxkbfile-dev:riscv64 libxmu-dev:riscv64 libxmuu-dev:riscv64 libxpm-dev:riscv64 libxrandr-dev:riscv64 libxrender-dev:riscv64 libxres-dev:riscv64 libxss-dev:riscv64 libxt-dev:riscv64 libxtst-dev:riscv64 libxv-dev:riscv64 libxvmc-dev:riscv64 libxxf86dga-dev:riscv64 libxxf86vm-dev:riscv64 x11proto-dev:riscv64 xserver-xorg-dev:riscv64 xtrans-dev:riscv64
# sudo apt install gcc-riscv64-linux-gnu g++-riscv64-linux-gnu zlib1g-dev:riscv64 libbz2-dev:riscv64 libgl1-mesa-dev:riscv64 libasound2-dev:riscv64 libpulse-dev:riscv64

# Build instructions
# cd linux
# source target-whatever.sh
# make
# source vars.sh
# cd ../
# meson setup --cross-file ./linux/$ARCH_MESON_TOOLCHAIN build
# cd build
# ninja
# meson configure --bindir=. --prefix=$PWD/local
# ninja install
# cp "$MKXPZ_PREFIX/lib/$("$ARCH_CONFIGURE-objdump" -p local/mkxp-z* | grep -i NEEDED | grep -Eo 'libruby.so.[0-9\.]+')" local/lib64/
# cp "/usr/lib/$ARCH_CONFIGURE/$("$ARCH_CONFIGURE-objdump" -p local/lib64/libruby.so* | grep -i NEEDED | grep -Eo 'libcrypt.so.[0-9\.]+')" local/lib64/

export ARCH=riscv64
export ARCH_OPENSSL=riscv64
export ARCH_CFLAGS=""
export ARCH_CONFIGURE=riscv64-linux-gnu
export CC="$ARCH_CONFIGURE-gcc"
export ARCH_CMAKE_TOOLCHAIN=toolchain-riscv64.cmake
export ARCH_MESON_TOOLCHAIN=meson-riscv64.txt
