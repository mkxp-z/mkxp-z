#!/usr/bin/env bash

# Deps instructions:
# sudo dpkg --add-architecture ppc64el
# (Target versions of xorg-dev; removes native versions of: libxfont-dev xorg-dev xserver-xorg-dev) sudo apt install libdmx-dev:ppc64el libfontenc-dev:ppc64el libfs-dev:ppc64el libice-dev:ppc64el libsm-dev:ppc64el libx11-dev:ppc64el libxau-dev:ppc64el libxaw7-dev:ppc64el libxcomposite-dev:ppc64el libxcursor-dev:ppc64el libxdamage-dev:ppc64el libxdmcp-dev:ppc64el libxext-dev:ppc64el libxfixes-dev:ppc64el libxfont-dev:ppc64el libxft-dev:ppc64el libxi-dev:ppc64el libxinerama-dev:ppc64el libxkbfile-dev:ppc64el libxmu-dev:ppc64el libxmuu-dev:ppc64el libxpm-dev:ppc64el libxrandr-dev:ppc64el libxrender-dev:ppc64el libxres-dev:ppc64el libxss-dev:ppc64el libxt-dev:ppc64el libxtst-dev:ppc64el libxv-dev:ppc64el libxvmc-dev:ppc64el libxxf86dga-dev:ppc64el libxxf86vm-dev:ppc64el x11proto-dev:ppc64el xserver-xorg-dev:ppc64el xtrans-dev:ppc64el
# sudo apt install gcc-powerpc64le-linux-gnu g++-powerpc64le-linux-gnu zlib1g-dev:ppc64el libbz2-dev:ppc64el libgl1-mesa-dev:ppc64el libasound2-dev:ppc64el libpulse-dev:ppc64el

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

export ARCH=power9le
export ARCH_OPENSSL=ppc64le
export ARCH_CFLAGS="-mcpu=power9 -mtune=power9"
export ARCH_CONFIGURE=powerpc64le-linux-gnu
export CC="$ARCH_CONFIGURE-gcc"
export ARCH_CMAKE_TOOLCHAIN=toolchain-ppc64le.cmake
export ARCH_MESON_TOOLCHAIN=meson-power9le.txt
