ARCH := arm64
HOST := aarch64-apple-darwin
SDK := iphonesimulator
MINIMUM_REQUIRED := 17.0
TARGET_FLAG := -mios-simulator-version-min=$(MINIMUM_REQUIRED) -target arm64-apple-ios$(MINIMUM_REQUIRED)-simulator

include common.make
