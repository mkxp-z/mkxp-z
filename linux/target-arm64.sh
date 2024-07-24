#!/usr/bin/env bash

# Deps instructions:
# sudo dpkg --add-architecture arm64
# (Target versions of xorg-dev; removes native versions of: libxfont-dev xorg-dev xserver-xorg-dev) sudo apt install libdmx-dev:arm64 libfontenc-dev:arm64 libfs-dev:arm64 libice-dev:arm64 libsm-dev:arm64 libx11-dev:arm64 libxau-dev:arm64 libxaw7-dev:arm64 libxcomposite-dev:arm64 libxcursor-dev:arm64 libxdamage-dev:arm64 libxdmcp-dev:arm64 libxext-dev:arm64 libxfixes-dev:arm64 libxfont-dev:arm64 libxft-dev:arm64 libxi-dev:arm64 libxinerama-dev:arm64 libxkbfile-dev:arm64 libxmu-dev:arm64 libxmuu-dev:arm64 libxpm-dev:arm64 libxrandr-dev:arm64 libxrender-dev:arm64 libxres-dev:arm64 libxss-dev:arm64 libxt-dev:arm64 libxtst-dev:arm64 libxv-dev:arm64 libxvmc-dev:arm64 libxxf86dga-dev:arm64 libxxf86vm-dev:arm64 x11proto-dev:arm64 xserver-xorg-dev:arm64 xtrans-dev:arm64
# sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu zlib1g-dev:arm64 libbz2-dev:arm64 libgl1-mesa-dev:arm64 libasound2-dev:arm64 libpulse-dev:arm64

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

export ARCH=arm64
export ARCH_OPENSSL=aarch64
export ARCH_CFLAGS=""
export ARCH_CONFIGURE=aarch64-linux-gnu
export CC="$ARCH_CONFIGURE-gcc"
export ARCH_CMAKE_TOOLCHAIN=toolchain-arm64.cmake
export ARCH_MESON_TOOLCHAIN=meson-arm64.txt
