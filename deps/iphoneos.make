ARCH := arm64
HOST := aarch64-apple-darwin
SDK := iphoneos
MINIMUM_REQUIRED := 17.0
TARGET_FLAG := -miphoneos-version-min=$(MINIMUM_REQUIRED) -target arm64-apple-ios$(MINIMUM_REQUIRED)

include common.make
