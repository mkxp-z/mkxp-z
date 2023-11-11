DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

MKXPZ_HOST=$(gcc -dumpmachine)
MKXPZ_PREFIX=$(ruby -e "printf ('${MKXPZ_HOST}'[/i686/].nil?) ? 'mingw64' : 'mingw'")
export LDFLAGS="-L$DIR/build-${MKXPZ_PREFIX}/lib -L$DIR/build-${MKXPZ_PREFIX}/bin"
export CFLAGS="-I$DIR/build-${MKXPZ_PREFIX}/include"
export PATH="$DIR/build-${MKXPZ_PREFIX}/bin:$PATH"
MKXPZ_OLD_PC=$(pkg-config --variable pc_path pkg-config)

# Try to load the stuff we built first
export PKG_CONFIG_PATH="$DIR/build-${MKXPZ_PREFIX}/lib/pkgconfig"
export MKXPZ_PREFIX="$DIR/build-${MKXPZ_PREFIX}"

# Optional Ruby Variables to Build with your System's Interpreter
RUBY_EXE=$(cygpath "$(where ruby | head -1)")
RUBY_BIN_DIR=$(dirname "${RUBY_EXE}")
RUBY_ROOT=$(dirname "${RUBY_BIN_DIR}")
export RUBY_LIB_DIR="${RUBY_ROOT}/lib"
export RUBY_LIB_NAME=$(find ${RUBY_LIB_DIR}/lib*ruby*.dll.a -type f | head -1 | perl -nle 'm/lib(\w+-\w+-ruby\d+)\.dll\.a/; print $1')
RUBY_INCLUDE_ROOT="${RUBY_ROOT}/include"
RUBY_INCLUDE_DIR=$(find ${RUBY_INCLUDE_ROOT}/ruby-* -maxdepth 0 -type d | head -1)
RUBY_CONFIG_HEADER=$(find ${RUBY_INCLUDE_DIR}/**/ruby/config.h -type f | head -1)
RUBY_CONFIG_INCLUDE_DIR=$(dirname "$(dirname "${RUBY_CONFIG_HEADER}")")
export RUBY_INCLUDE_DIRS="${RUBY_INCLUDE_DIR}:${RUBY_CONFIG_INCLUDE_DIR}"

