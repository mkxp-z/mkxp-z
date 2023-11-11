DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

MKXPZ_PREFIX=$(uname -m)
export LDFLAGS="-L$DIR/build-${MKXPZ_PREFIX}/lib"
export CFLAGS="-I$DIR/build-${MKXPZ_PREFIX}/include"
MKXPZ_OLD_PC=$(pkg-config --variable pc_path pkg-config)

# Try to load the stuff we built first
export PKG_CONFIG_LIBDIR="$DIR/build-${MKXPZ_PREFIX}/lib/pkgconfig:$DIR/build-${MKXPZ_PREFIX}/lib64/pkgconfig:${MKXPZ_OLD_PC}"
export PATH="$DIR/build-${MKXPZ_PREFIX}/bin:$PATH"
export LD_LIBRARY_PATH="$DIR/build-${MKXPZ_PREFIX}/lib:${LD_LIBRARY_PATH}"
export MKXPZ_PREFIX="$DIR/build-${MKXPZ_PREFIX}"

# Optional Ruby Variables to Build with your System's Interpreter
RUBY_EXE=$(where ruby | head -1)
RUBY_BIN_DIR=$(dirname "${RUBY_EXE}")
RUBY_ROOT=$(dirname "${RUBY_BIN_DIR}")
export RUBY_LIB_DIR="${RUBY_ROOT}/lib"
export RUBY_LIB_NAME="ruby"
RUBY_INCLUDE_ROOT="${RUBY_ROOT}/include"
RUBY_INCLUDE_DIR=$(find ${RUBY_INCLUDE_ROOT}/ruby-* -maxdepth 0 -type d | head -1)
RUBY_CONFIG_HEADER=$(find ${RUBY_INCLUDE_DIR}/**/ruby/config.h -type f | head -1)
RUBY_CONFIG_INCLUDE_DIR=$(dirname "$(dirname "${RUBY_CONFIG_HEADER}")")
export RUBY_INCLUDE_DIRS="${RUBY_INCLUDE_DIR}:${RUBY_CONFIG_INCLUDE_DIR}"