DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

MKXPZ_PREFIX="$ARCH"
if [ "$MKXPZ_PREFIX" = "" ]
then
  MKXPZ_PREFIX=$(uname -m)
fi
export LDFLAGS="-L$DIR/build-${MKXPZ_PREFIX}/lib"
export CFLAGS="-I$DIR/build-${MKXPZ_PREFIX}/include"
MKXPZ_OLD_PC=$(pkg-config --variable pc_path pkg-config)
if [ "$ARCH_CONFIGURE" != "" ]
then
  MKXPZ_OLD_PC="/usr/lib/$ARCH_CONFIGURE/pkgconfig"
fi

# Try to load the stuff we built first
export PKG_CONFIG_LIBDIR="$DIR/build-${MKXPZ_PREFIX}/lib/pkgconfig:$DIR/build-${MKXPZ_PREFIX}/lib64/pkgconfig:${MKXPZ_OLD_PC}"
export PATH="$DIR/build-${MKXPZ_PREFIX}/bin:$PATH"
export LD_LIBRARY_PATH="$DIR/build-${MKXPZ_PREFIX}/lib:${LD_LIBRARY_PATH}"
export MKXPZ_PREFIX="$DIR/build-${MKXPZ_PREFIX}"

# Horrifying Ruby cross-compile bug workaround
MKXPZ_RUBY_PREFIX="$MKXPZ_PREFIX/$MKXPZ_PREFIX"
cp -a "$MKXPZ_RUBY_PREFIX"/* "$MKXPZ_PREFIX/"
