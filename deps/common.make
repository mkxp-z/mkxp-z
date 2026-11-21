SYSROOT := $(shell xcrun --sdk $(SDK) --show-sdk-path)
SDK_VERSION := $(shell xcrun --sdk $(SDK) --show-sdk-version)
TARGETFLAGS := -isysroot $(SYSROOT) $(TARGET_FLAG) -arch $(ARCH)

# `ld -platform_version <platform> <min> <sdk>` — needed for ld -r
# linking on modern Apple toolchains. Platform name differs between
# device (`ios`) and simulator (`ios-simulator`); we infer from $(SDK).
ifeq ($(SDK),iphonesimulator)
LD_PLATFORM := ios-simulator
else ifeq ($(SDK),macosx)
LD_PLATFORM := macos
else
LD_PLATFORM := ios
endif
LD_PLATFORM_VERSION := -platform_version $(LD_PLATFORM) $(MINIMUM_REQUIRED) $(SDK_VERSION)
BUILD_PREFIX := ${PWD}/build-$(SDK)-$(ARCH)
LIBDIR := $(BUILD_PREFIX)/lib
INCLUDEDIR := $(BUILD_PREFIX)/include
# Per-SDK tag for isolated autotools/cmake build dirs. Never share
# configure output between iphoneos and iphonesimulator — see
# BUILD_PIPELINE_ISSUES.md issues 3 and 7.
SDK_TAG := $(SDK)-$(ARCH)

# Every source tree below is shared by both SDKs, and configuring one
# distcleans it, so only one SDK's objects can live in a tree at a
# time. A configure step therefore deletes every `.configured-*` stamp
# in that tree, across all SDK and arch combinations. The next build
# for any other combination reconfigures instead of archiving the
# objects this one left behind. Without it, building both SDKs in one
# session produced an iphoneos libruby made of simulator objects, and
# the merged 3.1 binding failed to link (2026-08-09).
#
# Keep the `=` assignment: `$@` must expand when the recipe runs.
MARK_SDK_CONFIGURED = rm -f $(dir $@).configured-* && touch $@
# `make distclean` can fail in silence, or rerun the last SDK's
# configure through `config.status --recheck` and keep its objects. The
# 2026-09-28 release then shipped an iphoneos SDL_ttf.o in the simulator
# libSDL2_ttf.a. Delete every object after it, so none can carry over.
CLEAN_SDK_TREE = { $(MAKE) distclean >/dev/null 2>&1 || true; } && \
	find . \( -name '*.o' -o -name '*.lo' -o -name '*.la' -o -name '*.a' \) -type f -delete
CMAKE_BUILDDIR := cmakebuild-$(SDK_TAG)
DOWNLOADS := ${PWD}/downloads/$(HOST)
SOURCES := ${PWD}/sources
PATCHES := ${PWD}
ENGINE := ${PWD}/..
NPROC := $(shell sysctl -n hw.ncpu)
CFLAGS := -I$(INCLUDEDIR) -I$(INCLUDEDIR)/freetype2 $(TARGETFLAGS) -O3
CXXFLAGS := $(CFLAGS)
LDFLAGS := -L$(LIBDIR) $(TARGETFLAGS)
CLANG   := $(shell xcrun --sdk $(SDK) -f clang) -arch $(ARCH)
CLANGXX := $(shell xcrun --sdk $(SDK) -f clang++) -arch $(ARCH)
# Empty when the machine has no ccache.
CCACHE  := $(shell command -v ccache)
CC      := $(CCACHE) $(CLANG)
CXX     := $(CCACHE) $(CLANGXX)
AR      := $(shell xcrun --sdk $(SDK) -f ar)
RANLIB  := $(shell xcrun --sdk $(SDK) -f ranlib)
PKG_CONFIG_LIBDIR := $(BUILD_PREFIX)/lib/pkgconfig
GIT := git
CLONE := $(GIT) clone -q
GITHUB := https://github.com

# Host build triple for cross-compilation (Apple system Ruby returns non-standard string)
RBUILD := aarch64-apple-darwin

CPPFLAGS := -isysroot $(SYSROOT) $(CFLAGS)

CONFIGURE_ENV := \
	PKG_CONFIG_LIBDIR=$(PKG_CONFIG_LIBDIR) \
	CC="$(CC)" CXX="$(CXX)" AR="$(AR)" RANLIB="$(RANLIB)" \
	CFLAGS="$(CFLAGS)" CXXFLAGS="$(CXXFLAGS)" CPPFLAGS="$(CPPFLAGS)" LDFLAGS="$(LDFLAGS)"

CONFIGURE_ARGS := \
	--prefix="$(BUILD_PREFIX)" \
	--host=$(HOST)

CMAKE_ARGS := \
	-DCMAKE_INSTALL_PREFIX="$(BUILD_PREFIX)" \
	-DCMAKE_PREFIX_PATH="$(BUILD_PREFIX)" \
	-DCMAKE_OSX_ARCHITECTURES=$(ARCH) \
	-DCMAKE_OSX_SYSROOT=$(SYSROOT) \
	-DCMAKE_C_FLAGS="$(CFLAGS)" \
	-DCMAKE_CXX_FLAGS="$(CXXFLAGS)" \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_SYSTEM_NAME=$(if $(filter macosx,$(SDK)),Darwin,iOS) \
	-DCMAKE_OSX_DEPLOYMENT_TARGET=$(MINIMUM_REQUIRED) \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_FIND_ROOT_PATH="$(BUILD_PREFIX)" \
	-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH \
	-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH \
	-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH

# Ruby: static only for iOS, no JIT, no fiddle, no shared
RUBY_CONFIGURE_ARGS := \
	--disable-shared \
	--enable-install-static-library \
	--with-static-linked-ext \
	--with-out-ext=fiddle,gdbm,win32ole,win32,pty,syslog,readline,bigdecimal \
	--disable-rubygems \
	--disable-install-doc \
	--disable-jit-support \
	--build=$(RBUILD) \
	${EXTRA_RUBY_CONFIG_ARGS}

CONFIGURE := $(CONFIGURE_ENV) ./configure $(CONFIGURE_ARGS)
AUTOGEN   := $(CONFIGURE_ENV) ./autogen.sh $(CONFIGURE_ARGS)
# CMake takes the first word of CC as the compiler, so ccache goes in
# as its launcher instead.
CMAKE     := $(CONFIGURE_ENV) CC="$(CLANG)" CXX="$(CLANGXX)" cmake .. $(CMAKE_ARGS) \
	$(if $(CCACHE),-DCMAKE_C_COMPILER_LAUNCHER=$(CCACHE) -DCMAKE_CXX_COMPILER_LAUNCHER=$(CCACHE) \
	-DCMAKE_OBJC_COMPILER_LAUNCHER=$(CCACHE))

default: all

# Everything a host links, for one SDK: the dependencies, the three
# Rubies, the engine core, the three binding objects and the Ruby
# stdlib subsets.
all: angle deps-core ruby ruby18 ruby19 mkxp-merged mkxp-core ruby-stdlib

# ANGLE (prebuilt). The engine draws with GLES through ANGLE, which
# draws with Metal. The core needs the headers only. A host links the
# static libraries of the same release.
ANGLE_VERSION := angle-2026-11-19
ANGLE_SHA256 := 89077b8af161d026da2400f91459d16aac4789db5b69bf5b515104dc4c765460
ANGLE_STAMP := ${PWD}/ANGLE/.$(ANGLE_VERSION)

angle: $(ANGLE_STAMP)

$(ANGLE_STAMP):
	@mkdir -p $(DOWNLOADS)
	curl -fL --retry 3 -o $(DOWNLOADS)/angle-apple-prebuilt.tar.gz \
		$(GITHUB)/mateo-m/empo-deps/releases/download/$(ANGLE_VERSION)/angle-apple-prebuilt.tar.gz
	echo "$(ANGLE_SHA256)  $(DOWNLOADS)/angle-apple-prebuilt.tar.gz" | shasum -a 256 -c
	rm -rf ${PWD}/ANGLE
	mkdir -p ${PWD}/ANGLE
	tar -xzf $(DOWNLOADS)/angle-apple-prebuilt.tar.gz -C ${PWD}/ANGLE
	rm $(DOWNLOADS)/angle-apple-prebuilt.tar.gz
	touch $@

# Theora
libtheora: init_dirs libvorbis libogg $(LIBDIR)/libtheora.a

$(LIBDIR)/libtheora.a: $(LIBDIR)/libogg.a $(DOWNLOADS)/theora/Makefile
	cd $(DOWNLOADS)/theora; \
	make -j$(NPROC); make install

$(DOWNLOADS)/theora/.configured-$(SDK_TAG): $(DOWNLOADS)/theora/configure
	cd $(DOWNLOADS)/theora; $(CLEAN_SDK_TREE)
	cd $(DOWNLOADS)/theora; \
	$(CONFIGURE) --with-ogg=$(BUILD_PREFIX) --enable-shared=false --enable-static=true --disable-examples
	$(MARK_SDK_CONFIGURED)

$(DOWNLOADS)/theora/Makefile: $(DOWNLOADS)/theora/.configured-$(SDK_TAG)

# Theora's autogen.sh runs `autoreconf -isf`, which links the helper
# scripts into the Homebrew automake folder. A Homebrew automake update
# breaks those links, so copy them.
$(DOWNLOADS)/theora/configure: $(DOWNLOADS)/theora/autogen.sh
	cd $(DOWNLOADS)/theora; \
	autoreconf -if

$(DOWNLOADS)/theora/autogen.sh:
	$(CLONE) $(GITHUB)/xiph/theora $(DOWNLOADS)/theora

# Vorbis
libvorbis: init_dirs libogg $(LIBDIR)/libvorbis.a

$(LIBDIR)/libvorbis.a: $(LIBDIR)/libogg.a $(DOWNLOADS)/vorbis/$(CMAKE_BUILDDIR)/Makefile
	cd $(DOWNLOADS)/vorbis/$(CMAKE_BUILDDIR); \
	make -j$(NPROC); make install

$(DOWNLOADS)/vorbis/$(CMAKE_BUILDDIR)/Makefile: $(DOWNLOADS)/vorbis/CMakeLists.txt
	cd $(DOWNLOADS)/vorbis; \
	mkdir -p $(CMAKE_BUILDDIR); cd $(CMAKE_BUILDDIR); \
	$(CMAKE) -DBUILD_SHARED_LIBS=no

$(DOWNLOADS)/vorbis/CMakeLists.txt:
	$(CLONE) $(GITHUB)/xiph/vorbis -b v1.3.7 $(DOWNLOADS)/vorbis


# Ogg
libogg: init_dirs $(LIBDIR)/libogg.a

$(LIBDIR)/libogg.a: $(DOWNLOADS)/ogg/Makefile
	cd $(DOWNLOADS)/ogg; \
	make -j$(NPROC); make install

$(DOWNLOADS)/ogg/Makefile: $(DOWNLOADS)/ogg/.configured-$(SDK_TAG)

$(DOWNLOADS)/ogg/.configured-$(SDK_TAG): $(DOWNLOADS)/ogg/configure
	cd $(DOWNLOADS)/ogg; $(CLEAN_SDK_TREE)
	cd $(DOWNLOADS)/ogg; \
	$(CONFIGURE) --enable-static=true --enable-shared=false
	$(MARK_SDK_CONFIGURED)

$(DOWNLOADS)/ogg/configure: $(DOWNLOADS)/ogg/autogen.sh
	cd $(DOWNLOADS)/ogg; ./autogen.sh

$(DOWNLOADS)/ogg/autogen.sh:
	$(CLONE) $(GITHUB)/xiph/ogg -b v1.3.6 $(DOWNLOADS)/ogg

# uchardet
uchardet: init_dirs $(LIBDIR)/libuchardet.a

$(LIBDIR)/libuchardet.a: $(DOWNLOADS)/uchardet/$(CMAKE_BUILDDIR)/Makefile
	cd $(DOWNLOADS)/uchardet/$(CMAKE_BUILDDIR); \
	make -j$(NPROC); make install

$(DOWNLOADS)/uchardet/$(CMAKE_BUILDDIR)/Makefile: $(DOWNLOADS)/uchardet/CMakeLists.txt
	cd $(DOWNLOADS)/uchardet; \
	mkdir -p $(CMAKE_BUILDDIR); cd $(CMAKE_BUILDDIR); \
	$(CMAKE) -DBUILD_SHARED_LIBS=no -DBUILD_BINARY=OFF

$(DOWNLOADS)/uchardet/CMakeLists.txt:
	$(CLONE) https://gitlab.freedesktop.org/uchardet/uchardet -b v0.0.8 $(DOWNLOADS)/uchardet


# Pixman
pixman: init_dirs libpng $(LIBDIR)/libpixman-1.a

$(LIBDIR)/libpixman-1.a: $(DOWNLOADS)/pixman/Makefile
	cd $(DOWNLOADS)/pixman
	make -C $(DOWNLOADS)/pixman -j$(NPROC)
	make -C $(DOWNLOADS)/pixman install

$(DOWNLOADS)/pixman/.configured-$(SDK_TAG): $(DOWNLOADS)/pixman/autogen.sh
	cd $(DOWNLOADS)/pixman; $(CLEAN_SDK_TREE)
	cd $(DOWNLOADS)/pixman; \
	$(AUTOGEN) --enable-static=yes --enable-shared=no \
	--disable-arm-a64-neon
	$(MARK_SDK_CONFIGURED)

$(DOWNLOADS)/pixman/Makefile: $(DOWNLOADS)/pixman/.configured-$(SDK_TAG)

$(DOWNLOADS)/pixman/autogen.sh:
	$(CLONE) https://gitlab.freedesktop.org/pixman/pixman -b pixman-0.42.2 $(DOWNLOADS)/pixman


# PhysFS
physfs: init_dirs $(LIBDIR)/libphysfs.a

$(LIBDIR)/libphysfs.a: $(DOWNLOADS)/physfs/$(CMAKE_BUILDDIR)/Makefile
	cd $(DOWNLOADS)/physfs/$(CMAKE_BUILDDIR); \
	make -j$(NPROC); make install

$(DOWNLOADS)/physfs/$(CMAKE_BUILDDIR)/Makefile: $(DOWNLOADS)/physfs/CMakeLists.txt
	cd $(DOWNLOADS)/physfs; \
	mkdir -p $(CMAKE_BUILDDIR); cd $(CMAKE_BUILDDIR); \
	$(CMAKE) -DPHYSFS_BUILD_STATIC=true -DPHYSFS_BUILD_SHARED=false -DPHYSFS_BUILD_TEST=false

$(DOWNLOADS)/physfs/CMakeLists.txt:
	$(CLONE) $(GITHUB)/icculus/physfs -b release-3.2.0 $(DOWNLOADS)/physfs

# libpng
libpng: init_dirs $(LIBDIR)/libpng.a

$(LIBDIR)/libpng.a: $(DOWNLOADS)/libpng/Makefile
	cd $(DOWNLOADS)/libpng; \
	make -j$(NPROC); make install

$(DOWNLOADS)/libpng/.configured-$(SDK_TAG): $(DOWNLOADS)/libpng/configure
	cd $(DOWNLOADS)/libpng; $(CLEAN_SDK_TREE)
	cd $(DOWNLOADS)/libpng; \
	$(CONFIGURE) \
	--enable-shared=no --enable-static=yes
	$(MARK_SDK_CONFIGURED)

$(DOWNLOADS)/libpng/Makefile: $(DOWNLOADS)/libpng/.configured-$(SDK_TAG)

$(DOWNLOADS)/libpng/configure:
	$(CLONE) $(GITHUB)/pnggroup/libpng -b v1.6.50 $(DOWNLOADS)/libpng

# SDL2 (submodule: sources/sdl2)
sdl2: init_dirs $(LIBDIR)/libSDL2.a

$(LIBDIR)/libSDL2.a: $(SOURCES)/sdl2/$(CMAKE_BUILDDIR)/Makefile
	cd $(SOURCES)/sdl2/$(CMAKE_BUILDDIR); \
	make -j$(NPROC); make install

$(SOURCES)/sdl2/.patched-$(SDK_TAG): $(PATCHES)/sdl2.patches.lst $(PATCHES)/sdl2/ios.patch $(PATCHES)/sdl2/macos-no-opengl.patch $(PATCHES)/sdl2/macos.patch
	cd $(SOURCES)/sdl2; \
	git checkout -- . 2>/dev/null; \
	$(PATCHES)/apply-sdl-patches.sh $(SOURCES)/sdl2 --patches-root $(PATCHES); \
	touch $@

# SDL_OPENGLES stays OFF. mkxp-z drives ANGLE through EGL itself and
# creates its window without SDL_WINDOW_OPENGL, so it never asks SDL for
# a GL context. With it ON, SDL_uikitopenglview still linked in and
# dragged EAGLContext, CAEAGLLayer and four OES entry points behind it,
# which forced -weak_framework OpenGLES on the MkxpCore link. It was also
# a hazard: SDL_GL_DeleteContext on an ANGLE context reaches SDL's EAGL
# backend, which reads it as an Objective-C view and crashes. See the
# comment at mkxp-z-apple-mobile/src/main.cpp EngineHost::shutdown.
$(SOURCES)/sdl2/$(CMAKE_BUILDDIR)/Makefile: $(SOURCES)/sdl2/CMakeLists.txt $(SOURCES)/sdl2/.patched-$(SDK_TAG)
	cd $(SOURCES)/sdl2; \
	mkdir -p $(CMAKE_BUILDDIR); cd $(CMAKE_BUILDDIR); \
	$(CMAKE) -DBUILD_SHARED_LIBS=no \
	-DSDL_OPENGL=OFF \
	-DSDL_OPENGLES=OFF \
	-DSDL_METAL=ON \
	-DSDL_RENDER_METAL=ON

# SDL_image (submodule: sources/sdl2_image)
sdl2image: init_dirs sdl2 $(LIBDIR)/libSDL2_image.a

$(LIBDIR)/libSDL2_image.a: $(SOURCES)/sdl2_image/$(CMAKE_BUILDDIR)/Makefile
	cd $(SOURCES)/sdl2_image/$(CMAKE_BUILDDIR); \
	make -j$(NPROC); make install

# SDL_image pins 8 image libraries as submodules. With these flags it
# builds only jpeg, libpng and the zlib that libpng needs. libjxl's own
# submodules live on googlesource.com, which answers GitHub's runners
# with 503, so a recursive checkout must not reach them.
$(SOURCES)/sdl2_image/$(CMAKE_BUILDDIR)/Makefile: $(SOURCES)/sdl2_image/CMakeLists.txt
	cd $(SOURCES)/sdl2_image; \
	git submodule update --init --depth 1 external/jpeg external/libpng external/zlib
	cd $(SOURCES)/sdl2_image; mkdir -p $(CMAKE_BUILDDIR); cd $(CMAKE_BUILDDIR); \
	$(CMAKE) \
	-DBUILD_SHARED_LIBS=no \
	-DSDL2IMAGE_SAMPLES=no \
	-DSDL2IMAGE_JPG_SAVE=yes \
	-DSDL2IMAGE_PNG_SAVE=yes \
	-DSDL2IMAGE_PNG_SHARED=no \
	-DSDL2IMAGE_JPG_SHARED=no \
	-DSDL2IMAGE_JXL=no \
	-DSDL2IMAGE_BACKEND_IMAGEIO=no \
	-DSDL2IMAGE_VENDORED=yes


# SDL_sound (submodule: sources/sdl_sound)
sdlsound: init_dirs sdl2 libogg libvorbis $(LIBDIR)/libSDL2_sound.a

$(LIBDIR)/libSDL2_sound.a: $(SOURCES)/sdl_sound/$(CMAKE_BUILDDIR)/Makefile $(SOURCES)/sdl_sound/.patched-$(SDK_TAG)
	cd $(SOURCES)/sdl_sound/$(CMAKE_BUILDDIR); \
	make -j$(NPROC); make install

$(SOURCES)/sdl_sound/.patched-$(SDK_TAG): $(PATCHES)/sdl_sound/wav-24bit.patch
	cd $(SOURCES)/sdl_sound; \
	git checkout -- . 2>/dev/null; \
	git apply $(PATCHES)/sdl_sound/wav-24bit.patch && \
	touch $@

$(SOURCES)/sdl_sound/$(CMAKE_BUILDDIR)/Makefile: $(SOURCES)/sdl_sound/CMakeLists.txt $(SOURCES)/sdl_sound/.patched-$(SDK_TAG)
	cd $(SOURCES)/sdl_sound; mkdir -p $(CMAKE_BUILDDIR); cd $(CMAKE_BUILDDIR); \
	$(CMAKE) \
	-DSDLSOUND_BUILD_SHARED=false \
	-DSDLSOUND_BUILD_TEST=false \
	-DSDLSOUND_DECODER_COREAUDIO=false


# SDL2_ttf (submodule: sources/sdl2_ttf)
sdl2ttf: init_dirs sdl2 freetype $(LIBDIR)/libSDL2_ttf.a

# ACLOCAL=:/AUTOMAKE=:/... no-op the automake refresh rules: on fresh
# clones (CI runners) file mtimes are checkout-ordered roulette, and a
# fired refresh rule wants the exact automake version the checked-in
# files were generated with (`missing automake-1.16`). We always build
# from the checked-in generated files, so the refresh must never run.
$(LIBDIR)/libSDL2_ttf.a: $(SOURCES)/sdl2_ttf/.configured-$(SDK_TAG)
	cd $(SOURCES)/sdl2_ttf && \
	make -j$(NPROC) ACLOCAL=: AUTOCONF=: AUTOMAKE=: AUTOHEADER=: libSDL2_ttf.la && \
	make ACLOCAL=: AUTOCONF=: AUTOMAKE=: AUTOHEADER=: install-libLTLIBRARIES install-libSDL2_ttfincludeHEADERS install-pkgconfigDATA

$(SOURCES)/sdl2_ttf/.configured-$(SDK_TAG): $(SOURCES)/sdl2_ttf/configure
	cd $(SOURCES)/sdl2_ttf; $(CLEAN_SDK_TREE)
	cd $(SOURCES)/sdl2_ttf; \
	$(CONFIGURE) --enable-static=true --enable-shared=false
	$(MARK_SDK_CONFIGURED)

$(SOURCES)/sdl2_ttf/configure: $(SOURCES)/sdl2_ttf/autogen.sh
	cd $(SOURCES)/sdl2_ttf; ./autogen.sh

# OpenAL-Soft (submodule: sources/openal-soft)
#
# Replaces Apple's deprecated `-framework OpenAL` (frozen at the
# circa-2005 fork point, known buffer-management quirks on iOS that
# manifest as the Pokemon Infinite Fusion BGM-loop bug). OpenAL-Soft
# is the de-facto reference OpenAL 1.1 implementation, owns its
# own mixer ring (so `alSourceStop` actually drops samples instead
# of letting CoreAudio drain them), and is what JoiPlay ships on
# Android without exhibiting the loop.
#
# Static-lib build: every other dep here ships static, App Store
# accepts it, dyld doesn't pay an extra load. CoreAudio backend
# is the only one we need on iOS; everything else is forced off
# so the lib stays small. ALSOFT_REQUIRE_COREAUDIO=ON makes the
# CMake configure fail loudly if the backend isn't detected,
# rather than silently producing a no-output lib.
openal: init_dirs $(LIBDIR)/libopenal.a

$(LIBDIR)/libopenal.a: $(SOURCES)/openal-soft/$(CMAKE_BUILDDIR)/Makefile
	cd $(SOURCES)/openal-soft/$(CMAKE_BUILDDIR); \
	make -j$(NPROC); make install

$(SOURCES)/openal-soft/$(CMAKE_BUILDDIR)/Makefile: $(SOURCES)/openal-soft/CMakeLists.txt
	cd $(SOURCES)/openal-soft; \
	mkdir -p $(CMAKE_BUILDDIR); cd $(CMAKE_BUILDDIR); \
	$(CMAKE) \
	-DLIBTYPE=STATIC \
	-DALSOFT_UTILS=OFF \
	-DALSOFT_EXAMPLES=OFF \
	-DALSOFT_TESTS=OFF \
	-DALSOFT_INSTALL_EXAMPLES=OFF \
	-DALSOFT_INSTALL_UTILS=OFF \
	-DALSOFT_INSTALL_AMBDEC_PRESETS=OFF \
	-DALSOFT_INSTALL_HRTF_DATA=OFF \
	-DALSOFT_BACKEND_COREAUDIO=ON \
	-DALSOFT_REQUIRE_COREAUDIO=ON \
	-DALSOFT_BACKEND_PIPEWIRE=OFF \
	-DALSOFT_BACKEND_PULSEAUDIO=OFF \
	-DALSOFT_BACKEND_ALSA=OFF \
	-DALSOFT_BACKEND_OSS=OFF \
	-DALSOFT_BACKEND_SOLARIS=OFF \
	-DALSOFT_BACKEND_SNDIO=OFF \
	-DALSOFT_BACKEND_PORTAUDIO=OFF \
	-DALSOFT_BACKEND_JACK=OFF \
	-DALSOFT_BACKEND_OPENSL=OFF \
	-DALSOFT_BACKEND_WAVE=OFF


# FluidSynth (submodule: sources/fluidsynth)
#
# The engine renders MIDI with the synth only, so every audio and MIDI
# driver, file output, the shell and the network server stay off. The
# build has no option to skip the fluidsynth program, so make builds
# the library target and installs only its development component.
# osal=cpp11 removes glib. Native DLS needs C++17, which the cpp11
# layer turns on. The gentables need the gcem submodule, and CI checks
# out deps/sources without nested submodules.
fluidsynth: init_dirs $(LIBDIR)/libfluidsynth.a

$(LIBDIR)/libfluidsynth.a: $(SOURCES)/fluidsynth/$(CMAKE_BUILDDIR)/Makefile
	cd $(SOURCES)/fluidsynth/$(CMAKE_BUILDDIR) && \
	make -j$(NPROC) libfluidsynth && \
	cmake --install . --component fluidsynth_development

$(SOURCES)/fluidsynth/$(CMAKE_BUILDDIR)/Makefile: $(SOURCES)/fluidsynth/CMakeLists.txt
	cd $(SOURCES)/fluidsynth; \
	git submodule update --init --depth 1 gcem
	cd $(SOURCES)/fluidsynth; mkdir -p $(CMAKE_BUILDDIR); cd $(CMAKE_BUILDDIR); \
	$(CMAKE) \
	-DBUILD_SHARED_LIBS=OFF \
	-DCMAKE_MACOSX_BUNDLE=OFF \
	-Dosal=cpp11 \
	-Denable-native-dls=ON \
	-Denable-threads=ON \
	-Denable-openmp=OFF \
	-Denable-signalsmith=OFF \
	-Denable-libsndfile=OFF \
	-Denable-aufile=OFF \
	-Denable-readline=OFF \
	-Denable-network=OFF \
	-Denable-ipv6=OFF \
	-Denable-ladspa=OFF \
	-Denable-dbus=OFF \
	-Denable-systemd=OFF \
	-Denable-pulseaudio=OFF \
	-Denable-pipewire=OFF \
	-Denable-jack=OFF \
	-Denable-alsa=OFF \
	-Denable-oss=OFF \
	-Denable-portaudio=OFF \
	-Denable-sdl3=OFF \
	-Denable-midishare=OFF \
	-Denable-coreaudio=OFF \
	-Denable-coremidi=OFF \
	-Denable-framework=OFF


# OpenSSL (static). Ruby's openssl ext is merged into mkxp31-merged.o,
# and the host links these archives for the engine's own HTTP client.
# 3.5 is the current OpenSSL LTS (supported until 2030); 1.1.1 went
# EOL in 2023. Ruby 3.1's bundled openssl gem (3.0.x) and the vendored
# cpp-httplib both speak 3.x.
OPENSSL_VERSION := 3.5.7
OPENSSL_DIR := $(DOWNLOADS)/openssl-$(OPENSSL_VERSION)
OPENSSL_CONFIGURE_TARGET := ios64-xcrun
OPENSSL_CONFIGURE_FLAGS := -miphoneos-version-min=$(MINIMUM_REQUIRED)
ifeq ($(SDK),iphonesimulator)
OPENSSL_CONFIGURE_TARGET := iossimulator-xcrun
OPENSSL_CONFIGURE_FLAGS := -mios-simulator-version-min=$(MINIMUM_REQUIRED)
endif
ifeq ($(SDK),macosx)
OPENSSL_CONFIGURE_TARGET := darwin64-arm64-cc
OPENSSL_CONFIGURE_FLAGS := -mmacosx-version-min=$(MINIMUM_REQUIRED)
endif
OPENSSL_CONFIGURED := $(OPENSSL_DIR)/.configured-$(SDK)-$(ARCH)

openssl: init_dirs $(LIBDIR)/libcrypto.a $(LIBDIR)/libssl.a

$(LIBDIR)/libcrypto.a $(LIBDIR)/libssl.a: $(BUILD_PREFIX)/.openssl-installed

$(BUILD_PREFIX)/.openssl-installed: $(OPENSSL_CONFIGURED)
	cd $(OPENSSL_DIR); \
	$(MAKE) -j$(NPROC); \
	$(MAKE) install_sw
	touch $@

# Extract a pristine tree for each SDK, instead of cleaning the last
# one. `make distclean` left simulator objects in apps/libapps.a, and
# the device link then failed with "built for 'iOS-simulator'". The
# step also hid its own errors behind `|| true`. A fresh tree cannot
# carry the other SDK's objects.
$(OPENSSL_CONFIGURED): $(DOWNLOADS)/openssl-$(OPENSSL_VERSION).tar.gz
	rm -rf $(OPENSSL_DIR)
	cd $(DOWNLOADS) && tar xzf openssl-$(OPENSSL_VERSION).tar.gz
	cd $(OPENSSL_DIR); \
	./Configure $(OPENSSL_CONFIGURE_TARGET) no-shared no-dso \
		--prefix="$(BUILD_PREFIX)" \
		--libdir=lib \
		--openssldir="$(BUILD_PREFIX)/ssl" \
		$(OPENSSL_CONFIGURE_FLAGS)
	$(MARK_SDK_CONFIGURED)

$(DOWNLOADS)/openssl-$(OPENSSL_VERSION).tar.gz:
	@mkdir -p $(DOWNLOADS)
	curl -L -o $@ https://github.com/openssl/openssl/releases/download/openssl-$(OPENSSL_VERSION)/openssl-$(OPENSSL_VERSION).tar.gz


# Freetype (submodule: sources/freetype)
freetype: init_dirs $(LIBDIR)/libfreetype.a

$(LIBDIR)/libfreetype.a: $(SOURCES)/freetype/.configured-$(SDK_TAG)
	cd $(SOURCES)/freetype; \
	make -j$(NPROC); make install

$(SOURCES)/freetype/.configured-$(SDK_TAG): $(SOURCES)/freetype/builds/unix/configure
	cd $(SOURCES)/freetype; $(CLEAN_SDK_TREE)
	cd $(SOURCES)/freetype; \
	$(CONFIGURE) --enable-static=true --enable-shared=false
	$(MARK_SDK_CONFIGURED)

$(SOURCES)/freetype/builds/unix/configure: $(SOURCES)/freetype/autogen.sh
	cd $(SOURCES)/freetype; ./autogen.sh


# Ruby 3.1 (submodule: sources/ruby)
ruby: init_dirs openssl $(LIBDIR)/libruby.3.1-static.a $(LIBDIR)/libruby.3.1-ext.a

$(LIBDIR)/libruby.3.1-static.a: $(SOURCES)/ruby/.configured-$(SDK_TAG)
	set -e; \
	cd $(SOURCES)/ruby; \
	$(CONFIGURE_ENV) make -j$(NPROC) libruby.3.1-static.a; \
	cp libruby.3.1-static.a $(LIBDIR)/; \
	mkdir -p $(INCLUDEDIR)/ruby31; \
	cp -R include/* $(INCLUDEDIR)/ruby31/; \
	cp .ext/include/*/ruby/config.h $(INCLUDEDIR)/ruby31/ruby/config.h 2>/dev/null || true
	@# Header isolation: 3.1 lives under $(INCLUDEDIR)/ruby31/,
	@# 1.9 under $(INCLUDEDIR)/ruby19/, 1.8 under
	@# $(INCLUDEDIR)/ruby18/. Consumers (project.yml's
	@# HEADER_SEARCH_PATHS, the per-version mkxp{N}-merged make
	@# targets) must point at the right subdir for the version
	@# they want. No global $(INCLUDEDIR)/ruby.h fallback so each
	@# build sees only its own headers.

# Build Ruby 3.1 extensions (zlib, stringio, strscan, digest, etc.) plus
# encoding libs into libruby.3.1-ext.a. Mirrors the Ruby 1.8 pattern (see
# RUBY18_EXTS above). ext/extinit.o and enc/encinit.o replace the dmyext.o
# and dmyenc.o stubs that live in libruby.3.1-static.a.
#
# `miniruby` is the host-side Ruby executable that runs the
# extconf scripts and generates the encoding bundles. Ruby's
# default targets don't build miniruby unless asked: a bare
# `make libruby.3.1-static.a` skips it, so a clean rebuild here
# silently produced an empty ext.a (zlib, stringio, etc. all
# missing) which then linked into mkxp31-merged.o, leaving
# scripts.rxdata decoding broken at runtime. Force-build miniruby
# before exts so cross-compile bundle linking can find it.
$(LIBDIR)/libruby.3.1-ext.a: $(LIBDIR)/libruby.3.1-static.a
	cd $(SOURCES)/ruby; \
	$(CONFIGURE_ENV) make -j1 miniruby exts.mk encs; \
	EXT_TARGETS=$$(awk '/^extensions =/,/[^\\]$$/' exts.mk | tr '\\' ' ' | grep -oE 'ext/[^ ]+' | sed 's|/\.$$|/static|'); \
	$(CONFIGURE_ENV) make -j1 -f exts.mk ext/extinit.o $$EXT_TARGETS; \
	$(CONFIGURE_ENV) make -j1 enc/encinit.o
	@TMPDIR=$$(mktemp -d); \
	cd $$TMPDIR; \
	for a in $$(find $(SOURCES)/ruby/ext -name "*.a" -not -path "*/test/*") \
	         $(SOURCES)/ruby/enc/libenc.a $(SOURCES)/ruby/enc/libtrans.a; do \
		[ -f "$$a" ] || continue; \
		sub=$$(basename $$a .a); \
		mkdir -p "$$sub"; \
		(cd "$$sub" && $(AR) x "$$a"); \
	done; \
	cp $(SOURCES)/ruby/ext/extinit.o .; \
	cp $(SOURCES)/ruby/enc/encinit.o .; \
	$(AR) rcs $(LIBDIR)/libruby.3.1-ext.a extinit.o encinit.o */*.o; \
	$(RANLIB) $(LIBDIR)/libruby.3.1-ext.a; \
	rm -rf $$TMPDIR
	@# Strip dmyext.o and dmyenc.o from the core static lib so the real
	@# Init_ext and Init_enc in libruby.3.1-ext.a win at link time.
	$(AR) d $(LIBDIR)/libruby.3.1-static.a dmyext.o dmyenc.o || true
	$(RANLIB) $(LIBDIR)/libruby.3.1-static.a
	@# Networking depends on socket + openssl being statically built.
	@# Their extconfs have historically failed silently under
	@# cross-compilation (netinet6/in6.h probe), so assert instead of
	@# shipping an ext archive that quietly lost them.
	@for sym in _Init_socket _Init_openssl; do \
	    nm $(LIBDIR)/libruby.3.1-ext.a 2>/dev/null | grep -q "T $$sym" || { \
	        echo "ERROR: $$sym missing from libruby.3.1-ext.a (extconf failure? check $(SOURCES)/ruby/ext/*/mkmf.log)"; \
	        rm -f $(LIBDIR)/libruby.3.1-ext.a; \
	        exit 1; \
	    }; \
	done

$(SOURCES)/ruby/.configured-$(SDK_TAG): $(SOURCES)/ruby/configure $(LIBDIR)/libcrypto.a
	cd $(SOURCES)/ruby; $(CLEAN_SDK_TREE)
	cd $(SOURCES)/ruby; \
	export $(CONFIGURE_ENV); \
	export CFLAGS="-std=gnu99 -DRUBY_FUNCTION_NAME_STRING=__func__ $$CFLAGS"; \
	export LDFLAGS="$$LDFLAGS"; \
	./configure $(CONFIGURE_ARGS) $(RUBY_CONFIGURE_ARGS) \
	--with-baseruby=/usr/bin/ruby \
	--with-openssl-dir="$(BUILD_PREFIX)" \
	ac_cv_func_setpgrp_void=yes \
	ac_cv_func_fork=no \
	ac_cv_func_dup3=no \
	ac_cv_func_pipe2=no \
	ac_cv_func_getentropy=no \
	ac_cv_func_posix_spawn=no \
	ac_cv_func_posix_spawnp=no \
	ac_cv_func_fdatasync=no \
	ac_cv_func_preadv=no \
	ac_cv_func_pwritev=no \
	ac_cv_func_copy_file_range=no \
	ac_cv_func_close_range=no \
	ac_cv_func_getattrlist=no \
	ac_cv_func_fgetattrlist=no \
	cross_compiling=yes; \
	sed -i '' 's|^ASFLAGS.*=.*|ASFLAGS = $$(ARCH_FLAG) $$(INCFLAGS) $(TARGETFLAGS)|' Makefile
	$(MARK_SDK_CONFIGURED)

$(SOURCES)/ruby/configure: $(SOURCES)/ruby/configure.ac
	cd $(SOURCES)/ruby; \
	git checkout -- . 2>/dev/null; \
	$(PATCHES)/apply-ruby-patches.sh 31 $(SOURCES)/ruby \
		--patches-root $(PATCHES) --engine $(ENGINE); \
	autoreconf -i

# Per-Ruby-version mkxp-z binding compile + libruby merge.
#
# Ship several Ruby versions in one binary: compile the engine's
# binding code once per Ruby version (each against that Ruby's
# headers), merge each compile with its libruby into a single .o with
# `ld -r`, and demote every Ruby-defined symbol to local via
# `-unexported_symbols_list`. Hidden Ruby symbols cannot clash across
# versions, so each merged .o exposes one entry point to the host.
#
# The recipe lives in $(ENGINE)/tools/build-binding-ios.sh, and
# $(ENGINE)/tools/build-engine-ios.sh gives it the paths. See
# $(ENGINE)/docs/multi-ruby.md.
#
# Ruby 1.8, 1.9 and 3.1 are built. Native Ruby 3.0 was dropped,
# because the syntax-transform parser patches only exist in the 3.1
# source. 3.0 plus Legacy compatibility was a silent no-op that
# confused users on Pokemon Essentials forks. Auto-detect routes
# 3.0-bundling games to 3.1 plus Legacy.

mkxp31-merged: init_dirs ruby     $(LIBDIR)/mkxp31-merged.o
mkxp19-merged: init_dirs ruby19   $(LIBDIR)/mkxp19-merged.o
mkxp18-merged: init_dirs ruby18   $(LIBDIR)/mkxp18-merged.o

# The fingerprint stamp says every merged object matches the binding
# sources. Only a rule that waits for all three objects can say that.
# A run that dies on the third version leaves the old stamp, and a
# host that checks the stamp then refuses the build. Do not
# move the write into build-binding-ios.sh: it builds one version per
# run, and a partial build there stamped a set that was not there.
$(LIBDIR)/.mkxp-binding-fingerprint: $(LIBDIR)/mkxp18-merged.o $(LIBDIR)/mkxp19-merged.o $(LIBDIR)/mkxp31-merged.o
	$(ENGINE)/tools/binding-fingerprint.sh > $@
	@echo "mkxp-merged: stamped $@"

mkxp-merged: mkxp18-merged mkxp19-merged mkxp31-merged $(LIBDIR)/.mkxp-binding-fingerprint
mkxp-core: init_dirs $(LIBDIR)/libmkxpz-core.a

# ---- Engine core static library --------------------------------------
# Everything under $(ENGINE)/src compiled into libmkxpz-core.a. The
# recipe lives in tools/build-core-ios.sh. Compile-only:
# dependency headers come from $(INCLUDEDIR) + the ANGLE prebuilt.
# The script stamps $(LIBDIR)/.mkxp-core-fingerprint, a hash of the
# engine src tree, the same way as the binding fingerprint above.
#
# tools/build-engine-ios.sh holds the header search order for both
# parts, and reads ARCH and MINIMUM_REQUIRED from this SDK's make file.
BUILD_ENGINE = $(ENGINE)/tools/build-engine-ios.sh --sdk $(SDK)
BUILD_ENGINE_DEPS := $(ENGINE)/tools/build-engine-ios.sh $(ENGINE)/deps/$(SDK).make

MKXPZ_CORE_SRC_DEPS := \
    $(wildcard $(ENGINE)/src/*.cpp) \
    $(wildcard $(ENGINE)/src/*.mm) \
    $(wildcard $(ENGINE)/src/*.h) \
    $(wildcard $(ENGINE)/src/*/*.c) \
    $(wildcard $(ENGINE)/src/*/*.cpp) \
    $(wildcard $(ENGINE)/src/*/*.mm) \
    $(wildcard $(ENGINE)/src/*/*.h) \
    $(wildcard $(ENGINE)/src/*/*/*.c) \
    $(wildcard $(ENGINE)/src/*/*/*.cpp) \
    $(wildcard $(ENGINE)/src/*/*/*.h)

$(LIBDIR)/libmkxpz-core.a: $(MKXPZ_CORE_SRC_DEPS) $(ENGINE)/tools/build-core-ios.sh $(BUILD_ENGINE_DEPS) | $(ANGLE_STAMP)
	@$(BUILD_ENGINE) --part core

# Every source that compiles into the merged binding objects. Listing
# them as prerequisites means editing binding-mri.cpp (or any engine
# header the binding includes) makes `make mkxp-merged` rebuild the
# .o files instead of silently reusing stale ones. Engine headers are
# included because the Xcode-compiled engine half shares struct
# layouts with the binding half. Drifting apart is UB, not a link
# error. tools/binding-fingerprint.sh hashes the same set.
MKXPZ_BINDING_SRC_DEPS := \
    $(wildcard $(ENGINE)/binding/*.cpp) \
    $(wildcard $(ENGINE)/binding/*.h) \
    $(wildcard $(ENGINE)/hmode7/src/*.cpp) \
    $(wildcard $(ENGINE)/hmode7/src/*.h) \
    $(ENGINE)/multiruby/wrapper.cpp \
    $(wildcard $(ENGINE)/src/*.h) \
    $(wildcard $(ENGINE)/src/*/*.h) \
    $(ENGINE)/tools/build-binding-ios.sh

$(LIBDIR)/mkxp31-merged.o: $(LIBDIR)/libruby.3.1-static.a \
                          $(LIBDIR)/libruby.3.1-ext.a \
                          $(MKXPZ_BINDING_SRC_DEPS) $(BUILD_ENGINE_DEPS) | $(ANGLE_STAMP)
	@$(BUILD_ENGINE) --part 31

$(LIBDIR)/mkxp19-merged.o: $(LIBDIR)/libruby19-static.a \
                          $(LIBDIR)/libruby19-ext.a \
                          $(MKXPZ_BINDING_SRC_DEPS) $(BUILD_ENGINE_DEPS) | $(ANGLE_STAMP)
	@$(BUILD_ENGINE) --part 19

$(LIBDIR)/mkxp18-merged.o: $(LIBDIR)/libruby18-static.a \
                          $(LIBDIR)/libruby18-ext.a \
                          $(MKXPZ_BINDING_SRC_DEPS) $(BUILD_ENGINE_DEPS) | $(ANGLE_STAMP)
	@$(BUILD_ENGINE) --part 18

# Ruby 1.8 (submodule: sources/ruby18)
ruby18: init_dirs $(LIBDIR)/libruby18-static.a $(LIBDIR)/libruby18-ext.a

# Ruby 1.9 (submodule: sources/ruby19)
#
# 1.9.3-p551 builds for iOS arm64 with a small, surgical patch:
#   - aarch64-darwin-fake.rb: cross-compile fake config so the host
#     ruby (which we run as MINIRUBY in cross mode) reports as the
#     1.9.3 target instead of itself, otherwise tool/mkconfig.rb
#     refuses with "ruby lib version doesn't match executable
#     version". Same approach as Ruby 1.8.
#   - tool/config.{sub,guess}: replaced with modern savannah versions
#     so aarch64-apple-darwin canonicalizes correctly. The shipped
#     1.9 versions are too old to recognize aarch64.
#   - process.c: gate system() behind TARGET_OS_IPHONE (unavailable
#     on iOS, same as 3.0/3.1's ios.patch).
#
# The MINIRUBY override below uses --disable=gems so the host ruby
# doesn't try to load its own rbconfig before our fake.rb runs
# (rubygems' gem_prelude.rb requires rbconfig at startup, which then
# triggers the version mismatch check).
#
# We use --host=aarch64-unknown-darwin (3-tuple with explicit unknown
# vendor) instead of aarch64-apple-darwin because 1.9's autoconf
# 2.59-era target_cpu extraction empties the cpu field for the apple
# vendor case.
ruby19: init_dirs $(LIBDIR)/libruby19-static.a $(LIBDIR)/libruby19-ext.a

RUBY19_CFLAGS = $(TARGETFLAGS) -std=gnu89 -O2 \
	-Wno-implicit-function-declaration \
	-Wno-implicit-int \
	-Wno-incompatible-pointer-types \
	-Wno-int-conversion \
	-Wno-deprecated-non-prototype \
	-Wno-incompatible-function-pointer-types \
	-Wno-compound-token-split-by-macro

# Mirrors RUBY18_EXTS. Note: `thread` is NOT a separate ext in 1.9
# (folded into core); pathname is added because Pokemon Essentials
# uses it.
RUBY19_EXTS = zlib stringio strscan digest fcntl pathname

# socket is built outside the RUBY19_EXTS loop: it needs constdefs.c
# generated by mkconstants.rb, must exclude the getaddrinfo/getnameinfo
# emulation fallbacks (Darwin has the real functions), and can't run
# its extconf under cross-compilation - so the extconf probe results
# are pinned here as a curated Darwin/iOS define set. constdefs.c is
# NOT in the source list on purpose: constants.c #includes it.
SOCKET19_SRCS = init basicsocket ipsocket tcpsocket tcpserver \
	sockssocket udpsocket unixsocket unixserver option ancdata \
	raddrinfo constants socket

SOCKET19_DEFS = -DINET6 -DENABLE_IPV6 -DHAVE_PROTOTYPES \
	-DFD_PASSING_BY_MSG_CONTROL=1 \
	-DHAVE_ARPA_INET_H -DHAVE_ARPA_NAMESER_H -DHAVE_FCNTL -DHAVE_FCNTL_H \
	-DHAVE_FREEADDRINFO -DHAVE_GAI_STRERROR -DGAI_STRERROR_CONST \
	-DHAVE_GETADDRINFO -DHAVE_GETHOSTNAME -DHAVE_GETIFADDRS -DHAVE_GETNAMEINFO \
	-DHAVE_GETPEEREID -DHAVE_GETSERVBYPORT -DHAVE_HSTRERROR -DHAVE_IFADDRS_H \
	-DHAVE_IF_INDEXTONAME -DHAVE_INET_ATON -DHAVE_INET_NTOA -DHAVE_INET_NTOP -DHAVE_INET_PTON \
	-DHAVE_NETINET_IN_SYSTM_H -DHAVE_NETINET_TCP_H -DHAVE_NETINET_UDP_H -DHAVE_NET_IF_H \
	-DHAVE_RECVMSG -DHAVE_RESOLV_H -DHAVE_SA_LEN -DHAVE_SENDMSG -DHAVE_SIN_LEN \
	-DHAVE_SOCKADDR_STORAGE -DHAVE_SOCKETPAIR -DHAVE_ST_MSG_CONTROL \
	-DHAVE_STRUCT_IN_PKTINFO_IPI_SPEC_DST -DHAVE_TYPE_STRUCT_IN6_PKTINFO \
	-DHAVE_SYS_IOCTL_H -DHAVE_SYS_PARAM_H -DHAVE_SYS_SELECT_H -DHAVE_SYS_SOCKIO_H \
	-DHAVE_SYS_TIME_H -DHAVE_SYS_TYPES_H -DHAVE_SYS_UCRED_H -DHAVE_SYS_UIO_H -DHAVE_SYS_UN_H \
	-DHAVE_TYPE_STRUCT_ADDRINFO -DHAVE_TYPE_STRUCT_IPV6_MREQ -DHAVE_TYPE_STRUCT_IP_MREQ \
	-DHAVE_UNISTD_H

$(LIBDIR)/libruby19-static.a: $(SOURCES)/ruby19/.configured-$(SDK_TAG)
	set -e; \
	cd $(SOURCES)/ruby19; \
	$(CONFIGURE_ENV) make -j$(NPROC) libruby-static.a; \
	cp libruby-static.a $(LIBDIR)/libruby19-static.a; \
	mkdir -p $(INCLUDEDIR)/ruby19; \
	cp -R include/* $(INCLUDEDIR)/ruby19/; \
	cp .ext/include/aarch64-darwin/ruby/config.h $(INCLUDEDIR)/ruby19/ruby/config.h 2>/dev/null || true
	@# Compile our setjmp/longjmp shim and inject it into
	@# libruby19-static.a. The shim is currently a tail-call
	@# forwarder to libc _setjmp / _longjmp; we keep the
	@# indirection so we can swap the implementation per-arch
	@# without rebuilding the rest of Ruby. config.h is sed'd
	@# in the Makefile rule below to point RUBY_SETJMP /
	@# RUBY_LONGJMP at our symbols.
	$(CC) $(TARGETFLAGS) -c ${PWD}/ruby19/mkxp_setjmp_arm64.S \
		-o $(SOURCES)/ruby19/mkxp_setjmp_arm64.o
	$(AR) rcs $(LIBDIR)/libruby19-static.a $(SOURCES)/ruby19/mkxp_setjmp_arm64.o
	$(RANLIB) $(LIBDIR)/libruby19-static.a
	@# Build extensions (mirrors the Ruby 1.8 pattern; see
	@# libruby18-static.a recipe). Adds our hand-rolled extinit.c
	@# which provides the real Init_ext() calling each Init_X, and
	@# the encodings and transcoders that encinit.c starts.
	@EXTCFLAGS="$(RUBY19_CFLAGS) -I$(SOURCES)/ruby19 -I$(SOURCES)/ruby19/include -I$(SOURCES)/ruby19/.ext/include/aarch64-darwin -I$(INCLUDEDIR)/ruby19"; \
	OBJ_FILES=""; \
	for ext in $(RUBY19_EXTS); do \
		for src in $(SOURCES)/ruby19/ext/$$ext/*.c; do \
			obj=$${src%.c}.o; \
			$(CC) $$EXTCFLAGS -c $$src -o $$obj; \
			OBJ_FILES="$$OBJ_FILES $$obj"; \
		done; \
	done; \
	SOCKDIR=$(SOURCES)/ruby19/ext/socket; \
	/usr/bin/ruby --disable=gems $$SOCKDIR/mkconstants.rb -H $$SOCKDIR/constdefs.h -o $$SOCKDIR/constdefs.c; \
	for name in $(SOCKET19_SRCS); do \
		src=$$SOCKDIR/$$name.c; \
		obj=$${src%.c}.o; \
		$(CC) $$EXTCFLAGS $(SOCKET19_DEFS) -I$$SOCKDIR -c $$src -o $$obj; \
		OBJ_FILES="$$OBJ_FILES $$obj"; \
	done; \
	(cd $(SOURCES)/ruby19 && make encdb.h transdb.h) >/dev/null || exit 1; \
	for src in $(SOURCES)/ruby19/enc/*.c; do \
		case $${src##*/} in ascii.c|us_ascii.c|unicode.c|utf_8.c) continue;; esac; \
		obj=$${src%.c}.o; \
		$(CC) $$EXTCFLAGS -DONIG_ENC_REGISTER=rb_enc_register -c $$src -o $$obj || exit 1; \
		OBJ_FILES="$$OBJ_FILES $$obj"; \
	done; \
	for src in $(SOURCES)/ruby19/enc/trans/*.c; do \
		name=$$(basename $$src .c); \
		rename=-DInit_$$name=Init_trans_$$name; \
		[ $$name = transdb ] && rename=; \
		obj=$${src%.c}.o; \
		$(CC) $$EXTCFLAGS $$rename -c $$src -o $$obj || exit 1; \
		OBJ_FILES="$$OBJ_FILES $$obj"; \
	done; \
	for name in extinit encinit; do \
		$(CC) $$EXTCFLAGS \
			-c ${PWD}/ruby19/$$name.c \
			-o $(SOURCES)/ruby19/$$name.o || exit 1; \
		OBJ_FILES="$(SOURCES)/ruby19/$$name.o $$OBJ_FILES"; \
	done; \
	$(AR) rcs $(LIBDIR)/libruby19-ext.a $$OBJ_FILES; \
	$(RANLIB) $(LIBDIR)/libruby19-ext.a
	@# Strip dmyext.o from libruby19-static.a so libruby19-ext.a's
	@# Init_ext wins at link time.
	$(AR) d $(LIBDIR)/libruby19-static.a dmyext.o || true
	$(RANLIB) $(LIBDIR)/libruby19-static.a

$(SOURCES)/ruby19/.configured-$(SDK_TAG): $(SOURCES)/ruby19/configure
	cd $(SOURCES)/ruby19; $(CLEAN_SDK_TREE)
	@# Same TRUE-constant fix as ruby18 above (1.9's copy lives in
	@# tool/); removed in Ruby 3.2, so modern host rubies choke.
	sed -i '' 's/=>TRUE/=>true/g' $(SOURCES)/ruby19/tool/mkconfig.rb
	cd $(SOURCES)/ruby19; \
	export $(CONFIGURE_ENV); \
	export CFLAGS="$(RUBY19_CFLAGS) $$CFLAGS"; \
	export LDFLAGS="$$LDFLAGS"; \
	./configure \
		--host=aarch64-unknown-darwin \
		--build=aarch64-unknown-darwin \
		--target=aarch64-unknown-darwin \
		--prefix="$(BUILD_PREFIX)" \
		--disable-shared \
		--with-static-linked-ext \
		--disable-rubygems \
		--disable-install-doc \
		cross_compiling=yes \
		ac_cv_func_fork=no
	@# Pin the system ruby (2.6) as the host ruby, matching the 3.1
	@# tree's --with-baseruby: 1.9's tool/*.rb uses the legacy 3-arg
	@# ERB.new that Ruby 4 removed, so a modern PATH ruby (homebrew)
	@# fails id.h/known_errors.inc generation and the build ships a
	@# truncated tree.
	sed -i '' 's|^BASERUBY = ruby$$|BASERUBY = /usr/bin/ruby --disable=gems|' $(SOURCES)/ruby19/Makefile
	sed -i '' 's|^MINIRUBY = ruby |MINIRUBY = /usr/bin/ruby --disable=gems |' $(SOURCES)/ruby19/Makefile
	@# Override config.h's RUBY_SETJMP / RUBY_LONGJMP to point at
	@# our shim symbols (see mkxp_setjmp_arm64.S). The shim
	@# currently tail-calls libc _setjmp / _longjmp; the
	@# indirection lets us swap implementations per-arch
	@# without rebuilding the rest of Ruby. `returns_twice`
	@# tells the compiler the call may resume control flow at
	@# the call site so locals stay reload-safe.
	CONFIG_H=$(SOURCES)/ruby19/.ext/include/aarch64-darwin/ruby/config.h; \
	if [ -f $$CONFIG_H ]; then \
	    sed -i '' \
	        -e 's|^#define RUBY_SETJMP(env) _setjmp(env)$$|#define RUBY_SETJMP(env) mkxp_ruby19_setjmp(env)|' \
	        -e 's|^#define RUBY_LONGJMP(env,val) _longjmp(env,val)$$|#define RUBY_LONGJMP(env,val) mkxp_ruby19_longjmp(env,val)|' \
	        $$CONFIG_H; \
	    echo '' >> $$CONFIG_H; \
	    echo 'extern int  mkxp_ruby19_setjmp(void *env) __attribute__((returns_twice));' >> $$CONFIG_H; \
	    echo 'extern void mkxp_ruby19_longjmp(void *env, int val) __attribute__((noreturn));' >> $$CONFIG_H; \
	fi
	$(MARK_SDK_CONFIGURED)

$(SOURCES)/ruby19/configure: $(SOURCES)/ruby19/configure.in
	cd $(SOURCES)/ruby19; \
	git checkout -- . 2>/dev/null; \
	git clean -fdxq 2>/dev/null; \
	rm -f aarch64-darwin-fake.rb arm64-darwin-fake.rb; \
	$(PATCHES)/apply-ruby-patches.sh 19 $(SOURCES)/ruby19 \
		--patches-root $(PATCHES) --engine $(ENGINE); \
	autoconf

# socket for 1.8 is a single socket.c, special-cased like 1.9's
# (see SOCKET19_DEFS): no cross-capable extconf, so the Darwin/iOS
# probe results are pinned. getaddrinfo.c/getnameinfo.c emulation
# fallbacks are skipped - Darwin has the real functions.
SOCKET18_DEFS = -DINET6 \
	-DHAVE_ARPA_INET_H -DHAVE_FCNTL -DHAVE_FCNTL_H \
	-DHAVE_GETADDRINFO -DHAVE_GETHOSTNAME -DHAVE_HSTRERROR \
	-DHAVE_NETINET_IN_SYSTM_H -DHAVE_NETINET_TCP_H -DHAVE_NETINET_UDP_H \
	-DHAVE_RECVMSG -DHAVE_SA_LEN -DHAVE_SENDMSG \
	-DHAVE_SOCKADDR_STORAGE -DHAVE_SOCKETPAIR -DHAVE_ST_MSG_CONTROL \
	-DHAVE_SYS_SELECT_H -DHAVE_SYS_TIME_H -DHAVE_SYS_TYPES_H \
	-DHAVE_SYS_UIO_H -DHAVE_SYS_UN_H -DHAVE_UNISTD_H

RUBY18_CFLAGS = $(TARGETFLAGS) -std=gnu89 -O2 \
	-fno-stack-protector \
	-fno-strict-aliasing \
	-fwrapv \
	-Wno-implicit-function-declaration \
	-Wno-implicit-int \
	-Wno-incompatible-pointer-types \
	-Wno-int-conversion \
	-Wno-deprecated-non-prototype \
	-Wno-incompatible-function-pointer-types

# Ruby 1.8 stdlib extensions to bundle into mkxp18-merged.o.
#
# `thread` was previously here but caused EXC_BAD_ACCESS in
# rb_thread_s_new (NULL deref at 0x15) when our hand-rolled
# Init_ext() force-initialized it on top of Ruby 1.8's already-built-in
# threading core. Removed; the core Thread class still works without
# it.
RUBY18_EXTS = zlib stringio strscan digest fcntl

$(LIBDIR)/libruby18-static.a: $(SOURCES)/ruby18/.configured-$(SDK_TAG)
	set -e; \
	cd $(SOURCES)/ruby18; \
	$(CONFIGURE_ENV) CFLAGS="$(RUBY18_CFLAGS)" make -j$(NPROC) COMPILE_PRELUDE=true libruby-static.a; \
	cp libruby-static.a $(LIBDIR)/libruby18-static.a; \
	mkdir -p $(INCLUDEDIR)/ruby18; \
	cp *.h $(INCLUDEDIR)/ruby18/
	@# Compile our PAC-free arm64 setjmp/longjmp replacement and
	@# inject it into libruby18-static.a. Apple's _setjmp signs LR
	@# with PACIBSP using SP as the modifier; Ruby 1.8's green
	@# threading longjmps onto a different stack, so PAC verify
	@# fails. The asm replacement saves/restores LR raw - no PAC.
	@# config.h is patched (in ios.patch) to point RUBY_SETJMP /
	@# RUBY_LONGJMP at our symbols.
	$(CC) $(TARGETFLAGS) -c ${PWD}/ruby18/mkxp_setjmp_arm64.S \
		-o $(SOURCES)/ruby18/mkxp_setjmp_arm64.o
	$(AR) rcs $(LIBDIR)/libruby18-static.a $(SOURCES)/ruby18/mkxp_setjmp_arm64.o
	$(RANLIB) $(LIBDIR)/libruby18-static.a
	@# Build extensions (use Ruby 1.8 headers only, not $(INCLUDEDIR) which has Ruby 3.1)
	@# Builds per-ext .o files, plus our hand-rolled extinit.c (which
	@# replaces dmyext.o's empty Init_ext at link time so Init_zlib /
	@# Init_stringio / etc. fire at Ruby startup; iOS can't dlopen).
	@EXTCFLAGS="$(RUBY18_CFLAGS) -I$(SOURCES)/ruby18 -I$(SOURCES)/ruby18/include -I$(INCLUDEDIR)"; \
	OBJ_FILES=""; \
	for ext in $(RUBY18_EXTS); do \
		for src in $(SOURCES)/ruby18/ext/$$ext/*.c; do \
			obj=$${src%.c}.o; \
			$(CC) $$EXTCFLAGS -c $$src -o $$obj; \
			OBJ_FILES="$$OBJ_FILES $$obj"; \
		done; \
	done; \
	SOCKDIR=$(SOURCES)/ruby18/ext/socket; \
	$(CC) $$EXTCFLAGS $(SOCKET18_DEFS) -I$$SOCKDIR \
		-c $$SOCKDIR/socket.c -o $$SOCKDIR/socket.o; \
	OBJ_FILES="$$OBJ_FILES $$SOCKDIR/socket.o"; \
	$(CC) $$EXTCFLAGS \
		-c ${PWD}/ruby18/extinit.c \
		-o $(SOURCES)/ruby18/extinit.o; \
	OBJ_FILES="$(SOURCES)/ruby18/extinit.o $$OBJ_FILES"; \
	$(AR) rcs $(LIBDIR)/libruby18-ext.a $$OBJ_FILES; \
	$(RANLIB) $(LIBDIR)/libruby18-ext.a
	@# Strip dmyext.o from libruby18-static.a so the real Init_ext
	@# from libruby18-ext.a wins at link time (mirrors the 3.0/3.1
	@# recipe's `$(AR) d ... dmyext.o` trick).
	$(AR) d $(LIBDIR)/libruby18-static.a dmyext.o || true
	$(RANLIB) $(LIBDIR)/libruby18-static.a

$(SOURCES)/ruby18/.configured-$(SDK_TAG): $(SOURCES)/ruby18/configure
	cd $(SOURCES)/ruby18; $(CLEAN_SDK_TREE)
	@# 1.8's mkconfig.rb uses the TRUE constant, runs under the HOST
	@# ruby in cross builds, and TRUE was removed in Ruby 3.2 — so
	@# the build breaks with a modern host ruby. Normalize to `true`
	@# (valid in every ruby). Idempotent sed instead of a submodule
	@# patch because sources/ruby18 is third-party (joiplay/ruby).
	sed -i '' 's/=>TRUE/=>true/g' $(SOURCES)/ruby18/mkconfig.rb
	cd $(SOURCES)/ruby18; \
	$(CONFIGURE_ENV) CFLAGS="$(RUBY18_CFLAGS)" \
	./configure \
		--host=$(HOST) \
		--build=x86_64-apple-darwin \
		--prefix="$(BUILD_PREFIX)" \
		--disable-shared \
		--enable-static \
		--with-static-linked-ext; \
		sed -i '' 's|^MINIRUBY = ruby |MINIRUBY = /usr/bin/ruby --disable=gems |' $(SOURCES)/ruby18/Makefile; \
	cp $(PATCHES)/ruby18/prelude.c $(SOURCES)/ruby18/prelude.c
	@# Override config.h's RUBY_SETJMP / RUBY_LONGJMP to point at
	@# our PAC-free arm64 setjmp variant (see mkxp_setjmp_arm64.S).
	@# Configure picks `_setjmp` based on HAVE__SETJMP detection;
	@# Apple's `_setjmp` signs LR with PAC, breaking Ruby 1.8's
	@# stack-swapping green threading. Our replacement uses raw LR
	@# save/restore.
	sed -i '' \
		-e 's|^#define RUBY_SETJMP(env) _setjmp(env)$$|#define RUBY_SETJMP(env) mkxp_ruby18_setjmp(env)|' \
		-e 's|^#define RUBY_LONGJMP(env,val) _longjmp(env,val)$$|#define RUBY_LONGJMP(env,val) mkxp_ruby18_longjmp(env,val)|' \
		$(SOURCES)/ruby18/config.h
	@# Inject prototypes; eval.c relies on the system header for
	@# _setjmp prototypes which won't match our names.
	@# CRITICAL: returns_twice attribute tells the compiler this is
	@# a setjmp-like function. Without it, locals live across the
	@# call may stay in registers and not be reloaded after longjmp,
	@# producing corrupted state on the second return. Apple's
	@# `_setjmp` is special-cased by clang automatically; ours isn't.
	echo '' >> $(SOURCES)/ruby18/config.h
	echo 'extern int  mkxp_ruby18_setjmp(void *env) __attribute__((returns_twice));' >> $(SOURCES)/ruby18/config.h
	echo 'extern void mkxp_ruby18_longjmp(void *env, int val) __attribute__((noreturn));' >> $(SOURCES)/ruby18/config.h
	$(MARK_SDK_CONFIGURED)

$(SOURCES)/ruby18/configure: $(SOURCES)/ruby18/configure.in
	cd $(SOURCES)/ruby18; \
	git checkout -- . 2>/dev/null; \
	git clean -fdx 2>/dev/null; \
	$(PATCHES)/apply-ruby-patches.sh 18 $(SOURCES)/ruby18 \
		--patches-root $(PATCHES) --engine $(ENGINE); \
	autoconf

# ====
init_dirs:
	@mkdir -p $(LIBDIR) $(INCLUDEDIR)

clean: clean-compiled

powerwash: clean-compiled clean-downloads

clean-downloads:
	-rm -rf downloads/$(HOST)

clean-compiled:
	-rm -rf build-$(SDK)-$(ARCH)

# Clean build artifacts from submodule source trees (configure outputs, object files, etc.)
clean-sources:
	@for dir in sdl2 sdl2_image sdl2_ttf sdl_sound freetype ruby ruby18 ruby19 openal-soft fluidsynth; do \
		rm -rf $(SOURCES)/$$dir/$(CMAKE_BUILDDIR) 2>/dev/null; \
		rm -f $(SOURCES)/$$dir/.configured-* 2>/dev/null; \
	done
	@for dir in ogg vorbis theora pixman libpng uchardet physfs; do \
		rm -rf $(DOWNLOADS)/$$dir/$(CMAKE_BUILDDIR) 2>/dev/null; \
		rm -f $(DOWNLOADS)/$$dir/.configured-* 2>/dev/null; \
	done
	cd $(SOURCES)/sdl2_ttf && git checkout -- . 2>/dev/null || true
	cd $(SOURCES)/freetype && git checkout -- . 2>/dev/null || true
	cd $(SOURCES)/ruby && git checkout -- . 2>/dev/null || true
	cd $(SOURCES)/ruby18 && git checkout -- . 2>/dev/null || true
	cd $(SOURCES)/ruby19 && git checkout -- . 2>/dev/null || true

deps-core: libtheora libvorbis pixman libpng physfs uchardet sdl2 sdl2image sdlsound sdl2ttf freetype openal openssl fluidsynth

# Pure-Ruby stdlib subsets for the three VMs, installed into
# $(BUILD_PREFIX)/ruby-stdlib/{3.1.0,1.9.1,1.8}. A host ships them
# next to the engine, which pushes them onto each VM's $LOAD_PATH
# (binding-mri.cpp). Directory names follow
# each Ruby's own rubylibdir convention.
#
# Curated, not wholesale: networking (net/http + its transitive
# closure) plus the small helpers desktop game scripts lean on.
# 1.8/1.9 deliberately exclude era net/ + openssl - those VMs get the
# Net::HTTP facade over the native client (net_http_compat.rb preload)
# because their openssl C exts predate OpenSSL 1.1 and cannot build.
RUBY_STDLIB_DIR := $(BUILD_PREFIX)/ruby-stdlib

RUBY31_STDLIB_LIB := net uri.rb uri resolv.rb resolv-replace.rb \
	ipaddr.rb base64.rb timeout.rb securerandom.rb random \
	open-uri.rb cgi.rb cgi time.rb singleton.rb forwardable.rb \
	forwardable delegate.rb English.rb \
	pstore.rb shellwords.rb tempfile.rb tmpdir.rb fileutils.rb
# No time.rb/date on 1.9 or 1.8: 1.9 lacks the date_core C ext and
# the JoiPlay 1.8 branch pruned lib/rational.rb which date/format.rb
# needs - both VMs would LoadError at require time. thread.rb on
# 1.8/1.9 is the pure green-thread Mutex/Queue, safe unlike the
# removed ext/thread C version.
RUBY19_STDLIB_LIB := uri.rb uri resolv.rb resolv-replace.rb \
	ipaddr.rb base64.rb timeout.rb securerandom.rb English.rb \
	set.rb thread.rb tsort.rb
RUBY18_STDLIB_LIB := uri.rb uri resolv.rb resolv-replace.rb \
	ipaddr.rb base64.rb timeout.rb securerandom.rb English.rb \
	thread.rb

ruby-stdlib: init_dirs
	rm -rf $(RUBY_STDLIB_DIR)
	mkdir -p $(RUBY_STDLIB_DIR)/3.1.0 $(RUBY_STDLIB_DIR)/1.9.1 $(RUBY_STDLIB_DIR)/1.8
	@# Ruby 3.1: full net/http stack + real openssl/socket .rb halves.
	@for item in $(RUBY31_STDLIB_LIB); do \
		cp -R $(SOURCES)/ruby/lib/$$item $(RUBY_STDLIB_DIR)/3.1.0/ || exit 1; \
	done
	cp -R $(SOURCES)/ruby/ext/digest/lib/  $(RUBY_STDLIB_DIR)/3.1.0/
	cp -R $(SOURCES)/ruby/ext/openssl/lib/ $(RUBY_STDLIB_DIR)/3.1.0/
	cp -R $(SOURCES)/ruby/ext/socket/lib/  $(RUBY_STDLIB_DIR)/3.1.0/
	cp -R $(SOURCES)/ruby/ext/date/lib/    $(RUBY_STDLIB_DIR)/3.1.0/
	mkdir -p $(RUBY_STDLIB_DIR)/3.1.0/rubygems
	cp $(SOURCES)/ruby/lib/rubygems/version.rb   $(RUBY_STDLIB_DIR)/3.1.0/rubygems/
	cp $(SOURCES)/ruby/lib/rubygems/deprecate.rb $(RUBY_STDLIB_DIR)/3.1.0/rubygems/
	cp ${PWD}/ruby31/rubygems_standin.rb $(RUBY_STDLIB_DIR)/3.1.0/rubygems.rb
	@# Ruby 1.9 / 1.8: era helpers only (see header comment).
	@for item in $(RUBY19_STDLIB_LIB); do \
		cp -R $(SOURCES)/ruby19/lib/$$item $(RUBY_STDLIB_DIR)/1.9.1/ || exit 1; \
	done
	cp -R $(SOURCES)/ruby19/ext/digest/lib/ $(RUBY_STDLIB_DIR)/1.9.1/
	cp $(SOURCES)/ruby19/ext/socket/lib/socket.rb $(RUBY_STDLIB_DIR)/1.9.1/
	@for item in $(RUBY18_STDLIB_LIB); do \
		cp -R $(SOURCES)/ruby18/lib/$$item $(RUBY_STDLIB_DIR)/1.8/ || exit 1; \
	done
	cp -R $(SOURCES)/ruby18/ext/digest/lib/ $(RUBY_STDLIB_DIR)/1.8/
	@find $(RUBY_STDLIB_DIR) -name "*.gemspec" -delete
	@# Warning-only transitive-require audit: report requires that
	@# resolve neither inside the shipped tree nor to a statically
	@# linked ext. Conditional/os-specific requires make a hard fail
	@# too noisy; treat output as a shipping-gap checklist.
	@for ver in 3.1.0 1.9.1 1.8; do \
		tree=$(RUBY_STDLIB_DIR)/$$ver; \
		grep -rhoE "^[[:space:]]*require ['\"][a-z0-9_/.-]+['\"]" $$tree 2>/dev/null \
		  | sed -E "s/^[[:space:]]*require ['\"]([^'\"]+)['\"]/\1/" | sort -u \
		  | while read -r feat; do \
			base=$${feat%.rb}; \
			[ -f "$$tree/$$base.rb" ] && continue; \
			[ -d "$$tree/$$base" ] && continue; \
			case " zlib stringio strscan digest fcntl pathname socket openssl date_core etc json monitor io/console io/nonblock io/wait cgi/escape digest/md5 digest/rmd160 digest/sha1 digest/sha2 digest/bubblebabble socket.so openssl.so digest.so etc.so english jcode " in \
				*" $$base "*) continue ;; \
			esac; \
			echo "  [ruby-stdlib audit $$ver] unresolved require: $$feat"; \
		done; \
	done; true
	@echo "ruby-stdlib installed under $(RUBY_STDLIB_DIR)"
