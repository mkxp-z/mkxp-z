This is just some random observations to the changes in the build procedure:

1. You need the UCRT version of MSYS2 using the following install command:
```shell
pacman -S git ruby vim base-devel \
          mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-meson \
          mingw-w64-ucrt-x86_64-autotools \
          mingw-w64-ucrt-x86_64-gcc
```

```shell
# Export the variables necessary to find the stuff we built
source vars.sh

# Configure the build
cd ..; meson build

# Build the thing
cd build && ninja

# Copy dependent libraries to the same folder
# for fun and profit
cp ${MKXPZ_PREFIX}/bin/x64-ucrt-ruby310.dll $PWD
cp /mingw64/bin/zlib1.dll $PWD

# (Remember you also need Steam's DLL if you configured with that)

# Strip it to save space
strip mkxp-z.exe

# Feast your eyes upon your work
start .
```

```shell
# Export the variables necessary to find the stuff we built
source vars.sh

# Configure the build
cd ..; meson build -Dbuild_gem=true

# Build the thing
cd build && ninja

# Strip it to save space
strip mkxpz.so

# Cory over to the lib folder
cp $PWD/mkxpz.so $PWD/lib/mkxp-z

# Feast your eyes upon your work
start .
```