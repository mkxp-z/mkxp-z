ARCH := arm64
HOST := aarch64-apple-darwin
SDK := macosx
MINIMUM_REQUIRED := 14.0
TARGET_FLAG := -mmacosx-version-min=$(MINIMUM_REQUIRED) -target arm64-apple-macos$(MINIMUM_REQUIRED)

# Configure scripts see a native build and compile their build tools
# with the raw toolchain clang, which finds no SDK without this.
export SDKROOT := $(shell xcrun --sdk $(SDK) --show-sdk-path)

include common.make
