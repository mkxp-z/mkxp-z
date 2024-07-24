#!/usr/bin/env bash

# Deps instructions:
# sudo dpkg --add-architecture armhf
# (Target versions of xorg-dev; removes native versions of: libxfont-dev xorg-dev xserver-xorg-dev) sudo apt install libdmx-dev:armhf libfontenc-dev:armhf libfs-dev:armhf libice-dev:armhf libsm-dev:armhf libx11-dev:armhf libxau-dev:armhf libxaw7-dev:armhf libxcomposite-dev:armhf libxcursor-dev:armhf libxdamage-dev:armhf libxdmcp-dev:armhf libxext-dev:armhf libxfixes-dev:armhf libxfont-dev:armhf libxft-dev:armhf libxi-dev:armhf libxinerama-dev:armhf libxkbfile-dev:armhf libxmu-dev:armhf libxmuu-dev:armhf libxpm-dev:armhf libxrandr-dev:armhf libxrender-dev:armhf libxres-dev:armhf libxss-dev:armhf libxt-dev:armhf libxtst-dev:armhf libxv-dev:armhf libxvmc-dev:armhf libxxf86dga-dev:armhf libxxf86vm-dev:armhf x11proto-dev:armhf xserver-xorg-dev:armhf xtrans-dev:armhf
# sudo apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf zlib1g-dev:armhf libbz2-dev:armhf libgl1-mesa-dev:armhf libasound2-dev:armhf libpulse-dev:armhf

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

export ARCH=armv7
export ARCH_OPENSSL=armv4
export ARCH_CFLAGS="-mcpu=generic-armv7-a+fp -mtune=generic-armv7-a+fp"
export ARCH_CONFIGURE=arm-linux-gnueabihf
export CC="$ARCH_CONFIGURE-gcc"
export ARCH_CMAKE_TOOLCHAIN=toolchain-arm32.cmake
export ARCH_MESON_TOOLCHAIN=meson-armv7.txt
