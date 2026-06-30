# Configure flags are based on the ones from https://github.com/ruby/ruby.wasm

.PHONY: all default clean

RUBY_VERSION ?= 3.3.10
WABT_VERSION ?= 1.0.37
LIBYAML_VERSION ?= 0.2.5
ZLIB_VERSION ?= 1.3.1
PICOSHA2_VERSION ?= 161cb3fc4170fa7a3eca9e582cebd27cc4d1fe29
BOOST_VERSION ?= 1.90.0
WASI_TARGET ?= wasm32-wasip2
WASI_SDK ?= /opt/wasi-sdk
WASM_OPT ?= wasm-opt
CTAGS ?= ctags
RUBY ?= ruby
AUTORECONF ?= autoreconf
CURL ?= curl
GIT ?= git
ZIP ?= zip
SED ?= sed
GREP ?= grep
NATIVE_CC ?= cc
NATIVE_CXX ?= c++
NATIVE_LD ?= ld
NATIVE_AR ?= ar
NATIVE_RANLIB ?= ranlib
NATIVE_CFLAGS ?=
NATIVE_CXXFLAGS ?=
NATIVE_LDFLAGS ?=
WASI_CFLAGS ?= -O3 -DNDEBUG
WASI_CXXFLAGS ?= -O3 -DNDEBUG
WASI_LDFLAGS ?=
WASM_OPT_FLAGS ?= -O4

MKXPZ_RUBY_HAVE_LTO ?= 1
MKXPZ_RUBY_HAVE_SOCKETS ?= 1
MKXPZ_RUBY_HAVE_USER_THREADS ?= 1

MKXPZ_RUBY_NUM_OUTPUTS ?= 32

BUILD_PREFIX := $(PWD)/build
OUTDIR := $(BUILD_PREFIX)/libretro-stage1
LIBDIR := $(BUILD_PREFIX)/lib
DOWNLOADS := $(BUILD_PREFIX)/downloads
WASM2C := $(LIBDIR)/bin/wasm2c
INDEXER := $(LIBDIR)/bin/indexer
CLONE := $(GIT) clone -q --depth 1
GITHUB := https://github.com
WASI_CC := $(WASI_SDK)/bin/clang
WASI_CXX := $(WASI_SDK)/bin/clang++
WASI_LD := $(WASI_SDK)/bin/clang
WASI_AR := $(WASI_SDK)/bin/llvm-ar
WASI_RANLIB := $(WASI_SDK)/bin/llvm-ranlib
NATIVE_TOOLCHAIN := CC=$(NATIVE_CC) CXX=$(NATIVE_CXX) LD=$(NATIVE_LD) AR=$(NATIVE_AR) RANLIB=$(NATIVE_RANLIB)
WASI_TOOLCHAIN := CC=$(WASI_CC) CXX=$(WASI_CXX) LD=$(WASI_LD) AR=$(WASI_AR) RANLIB=$(WASI_RANLIB)

# We need a larger stack size for Ruby 3.2 due to the lack of the overhaul of the setjmp implementation in WASI builds of Ruby started in https://github.com/ruby/ruby/pull/8902
ifneq ($(shell $(RUBY) -e 'puts "1" if Gem::Version.new("$(RUBY_VERSION)") < Gem::Version.new("3.3")'),)
    RUBY_STACK_SIZE := 16777216
else
    RUBY_STACK_SIZE := 2097152
endif

# WASI Preview 1 doesn't have usable socket support
ifeq ($(WASI_TARGET),wasm32-wasip1)
    MKXPZ_RUBY_HAVE_SOCKETS := 0
else ifeq ($(WASI_TARGET),wasm32-wasip1-threads)
    MKXPZ_RUBY_HAVE_SOCKETS := 0
else ifeq ($(WASI_TARGET),wasm32-wasi)
    MKXPZ_RUBY_HAVE_SOCKETS := 0
else ifeq ($(WASI_TARGET),wasm32-wasi-threads)
    MKXPZ_RUBY_HAVE_SOCKETS := 0
endif

# These lists of native extensions are copied directly from ruby.wasm, but with the dbm, gdbm and openssl extensions removed
ifneq ($(shell $(RUBY) -e 'puts "1" if Gem::Version.new("$(RUBY_VERSION)") < Gem::Version.new("3.4")'),)
    RUBY_EXTS := bigdecimal,cgi/escape,continuation,coverage,date,digest/bubblebabble,digest,digest/md5,digest/rmd160,digest/sha1,digest/sha2,etc,fcntl,fiber,json,json/generator,json/parser,nkf,objspace,pathname,psych,racc/cparse,rbconfig/sizeof,ripper,stringio,strscan,monitor,zlib
else
    RUBY_EXTS := cgi/escape,continuation,coverage,date,digest/bubblebabble,digest,digest/md5,digest/rmd160,digest/sha1,digest/sha2,etc,fcntl,json,json/generator,json/parser,objspace,pathname,psych,rbconfig/sizeof,ripper,stringio,strscan,monitor,zlib
endif

RUBY_EXTS := $(RUBY_EXTS),io/wait
ifeq ($(MKXPZ_RUBY_HAVE_SOCKETS),1)
    RUBY_EXTS := $(RUBY_EXTS),socket
endif

WASI_SDK_VERSION_EXPECTED := 30.0
WASI_SDK_VERSION_ACTUAL := $(shell head -n 1 $(WASI_SDK)/VERSION)
ifneq ($(WASI_SDK_VERSION_EXPECTED),$(WASI_SDK_VERSION_ACTUAL))
    $(error incorrect WASI SDK version: expected `$(WASI_SDK_VERSION_EXPECTED)', got `$(WASI_SDK_VERSION_ACTUAL)')
endif

WASI_TARGET_FLAGS := --target=$(WASI_TARGET) -mbulk-memory
ifeq ($(MKXPZ_RUBY_HAVE_LTO),1)
    WASI_TARGET_FLAGS := $(WASI_TARGET_FLAGS) -flto
    WASI_LDFLAGS := $(WASI_LDFLAGS) -flto
endif
ifeq ($(MKXPZ_RUBY_HAVE_USER_THREADS),1)
    WASI_TARGET_FLAGS := $(WASI_TARGET_FLAGS) -DMKXPZ_RUBY_HAVE_USER_THREADS
endif
WASI_C_STANDARD := -std=gnu99
WASI_CXX_STANDARD := -std=gnu++11
WASI_CFLAGS := $(WASI_TARGET_FLAGS) $(WASI_C_STANDARD) $(WASI_CFLAGS)
WASI_CXXFLAGS := $(WASI_TARGET_FLAGS) $(WASI_CXX_STANDARD) $(WASI_CXXFLAGS)

RUBY_SRC := $(DOWNLOADS)/ruby

all default: ruby-dist ruby-bindings $(OUTDIR)/preload.zip $(OUTDIR)/boost_predef/include/boost/predef.h

ruby-dist: $(OUTDIR)/dist.zip $(OUTDIR)/ruby/mkxp-sandbox-ruby.h $(OUTDIR)/ruby/mkxp-sandbox-ruby-impl.h $(OUTDIR)/ruby/mkxp-sandbox-ruby-func-types.h $(OUTDIR)/ruby/mkxp-sandbox-ruby-indices.h $(OUTDIR)/ruby/mkxp-sandbox-ruby-instantiate-args.h $(foreach i,$(shell seq 0 $(shell expr $(MKXPZ_RUBY_NUM_OUTPUTS) - 1)),$(OUTDIR)/ruby/mkxp-sandbox-ruby_$(i).c) $(OUTDIR)/ruby/mkxp-sandbox-ruby-num-outputs.txt $(OUTDIR)/ruby/mkxp-sandbox-ruby-target.txt $(OUTDIR)/ruby/mkxp-sandbox-ruby-version.h

ruby-bindings: $(OUTDIR)/sandbox-bindgen/mkxp-sandbox-bindgen.cpp $(OUTDIR)/sandbox-bindgen/mkxp-sandbox-bindgen.h

clean: clean-ruby-dist clean-ruby-bindings
	rm -rf $(LIBDIR)/*
	rm -rf $(DOWNLOADS)/*
	rm -f $(OUTDIR)/preload.zip
	rm -rf $(OUTDIR)/boost_predef

clean-ruby-dist:
	rm -f $(OUTDIR)/dist.zip
	rm -rf $(OUTDIR)/ruby

clean-ruby-bindings:
	rm -rf $(OUTDIR)/sandbox-bindgen
	rm -f $(LIBDIR)/tags.c
	rm -f $(LIBDIR)/tags

$(OUTDIR)/preload.zip: ../scripts/preload/kgl2_wrap.rb ../scripts/preload/mkxp_wrap.rb ../scripts/preload/ruby_classic_wrap.rb ../scripts/preload/win32_wrap.rb
	rm -rf $(LIBDIR)/preload
	mkdir -p $(LIBDIR)/preload
	mkdir -p $(OUTDIR)
	cp $^ $(LIBDIR)/preload/
	cd $(LIBDIR)/preload && $(ZIP) -r $@ *
	rm -r $(LIBDIR)/preload

# Ruby (targets WASI)

$(OUTDIR)/ruby/mkxp-sandbox-ruby.h $(OUTDIR)/ruby/mkxp-sandbox-ruby-impl.h $(OUTDIR)/ruby/mkxp-sandbox-ruby-func-types.h $(foreach i,$(shell seq 0 $(shell expr $(MKXPZ_RUBY_NUM_OUTPUTS) - 1)),$(OUTDIR)/ruby/mkxp-sandbox-ruby_$(i).c) $(OUTDIR)/ruby/mkxp-sandbox-ruby-num-outputs.txt $(OUTDIR)/ruby/mkxp-sandbox-ruby-target.txt $(OUTDIR)/ruby/mkxp-sandbox-ruby-version.h &: $(WASM2C) $(LIBDIR)/Dist/bin/ruby
	mkdir -p $(OUTDIR)/ruby
	echo '$(MKXPZ_RUBY_NUM_OUTPUTS)' > $(OUTDIR)/ruby/mkxp-sandbox-ruby-num-outputs.txt
	echo '$(WASI_TARGET)' > $(OUTDIR)/ruby/mkxp-sandbox-ruby-target.txt
	echo '#ifndef RAPI_MAJOR' > $(OUTDIR)/ruby/mkxp-sandbox-ruby-version.h
	$(RUBY) -e 'puts "#define RAPI_MAJOR " + "$(RUBY_VERSION)".split(".")[0]' >> $(OUTDIR)/ruby/mkxp-sandbox-ruby-version.h
	echo '#endif /* RAPI_MAJOR */' >> $(OUTDIR)/ruby/mkxp-sandbox-ruby-version.h
	echo '#ifndef RAPI_MINOR' >> $(OUTDIR)/ruby/mkxp-sandbox-ruby-version.h
	$(RUBY) -e 'puts "#define RAPI_MINOR " + "$(RUBY_VERSION)".split(".")[1]' >> $(OUTDIR)/ruby/mkxp-sandbox-ruby-version.h
	echo '#endif /* RAPI_MINOR */' >> $(OUTDIR)/ruby/mkxp-sandbox-ruby-version.h
	echo '#ifndef RAPI_TEENY' >> $(OUTDIR)/ruby/mkxp-sandbox-ruby-version.h
	$(RUBY) -e 'puts "#define RAPI_TEENY " + "$(RUBY_VERSION)".split(".")[2]' >> $(OUTDIR)/ruby/mkxp-sandbox-ruby-version.h
	echo '#endif /* RAPI_TEENY */' >> $(OUTDIR)/ruby/mkxp-sandbox-ruby-version.h
	$(WASM2C) $(LIBDIR)/Dist/bin/ruby -n ruby --num-outputs=$(MKXPZ_RUBY_NUM_OUTPUTS) -o $(OUTDIR)/ruby/mkxp-sandbox-ruby.c
	# Find all the function types declared using `FUNC_TYPE_DECL_EXTERN_T` and redeclare them as macros to fix the "initializer element is not constant" compilation error that occurs when using GCC versions earlier than 8.1.0
	rm -f $(OUTDIR)/ruby/mkxp-sandbox-ruby-func-types.h
	for func_type in $$($(GREP) -r 'FUNC_TYPE_DECL_EXTERN_T *([^()]*)' $(OUTDIR)/ruby/mkxp-sandbox-ruby-impl.h | $(GREP) -v '# *define' | $(SED) -e 's/FUNC_TYPE_DECL_EXTERN_T *( *\(.*\) *).*/\1/'); do \
		echo "#define $$func_type \"$$(od -N 32 -x /dev/urandom | cut -b 9- | tr -d ' \n' | $(SED) -e 's/.\{2\}/\\x&/g')\"" >> $(OUTDIR)/ruby/mkxp-sandbox-ruby-func-types.h; \
	done
	$(SED) -i 's/ *# *define * FUNC_TYPE_DECL_EXTERN_T *([^()]).*/#define FUNC_TYPE_DECL_EXTERN_T(x)/g' $(OUTDIR)/ruby/mkxp-sandbox-ruby-impl.h
	$(SED) -i 's/ *# *define * FUNC_TYPE_EXTERN_T *([^()]).*/#define FUNC_TYPE_EXTERN_T(x) const char _mkxp_unused_##x[]/g' $(OUTDIR)/ruby/mkxp-sandbox-ruby-impl.h
	$(SED) -i 's/ *# *define * FUNC_TYPE_T *([^()]).*/#define FUNC_TYPE_T(x) static const char _mkxp_unused_##x[]/g' $(OUTDIR)/ruby/mkxp-sandbox-ruby-impl.h
	$(SED) -i 's/__has_builtin *([^()]*)/0/g' $(OUTDIR)/ruby/mkxp-sandbox-ruby-impl.h
	$(SED) -i 's/#error "Missing implementation of __builtin_add_overflow or _addcarry_u64"/return __builtin_add_overflow(a, b, resptr);/g' $(OUTDIR)/ruby/mkxp-sandbox-ruby-impl.h

$(OUTDIR)/ruby/mkxp-sandbox-ruby-indices.h: $(INDEXER) $(LIBDIR)/Dist/bin/ruby
	mkdir -p $(OUTDIR)/ruby
	$(INDEXER) $(LIBDIR)/Dist/bin/ruby > $(OUTDIR)/ruby/mkxp-sandbox-ruby-indices.h

$(OUTDIR)/ruby/mkxp-sandbox-ruby-instantiate-args.h: $(OUTDIR)/ruby/mkxp-sandbox-ruby.h
	echo '#ifndef SANDBOX_INSTANTIATE_ARGS' > $@
	echo "#define SANDBOX_INSTANTIATE_ARGS(x) $$($(GREP) '^void wasm2c_ruby_instantiate(' $< | head -n 1 | $(SED) -e 's/^[^,]*, *//' -e 's/)[^)]*$$/,/' -e 's/\([^,]*\), */,static_cast<\1>(x)/g' -e 's/^,//')" >> $@
	echo '#endif /* SANDBOX_INSTANTIATE_ARGS */' >> $@

$(OUTDIR)/dist.zip: $(LIBDIR)/Dist/bin/ruby $(RUBY_SRC)/BSDL $(RUBY_SRC)/COPYING $(RUBY_SRC)/COPYING.ja
	rm -rf $(LIBDIR)/_Dist
	cp -r $(LIBDIR)/Dist $(LIBDIR)/_Dist
	rm $(LIBDIR)/_Dist/bin/ruby
	rm $(LIBDIR)/_Dist/lib/libruby-static.a
	rm -r $(LIBDIR)/_Dist/include
	rm -r $(LIBDIR)/_Dist/share
	rm -rf $(LIBDIR)/_Dist/lib/ruby/gems
	rm -r $(LIBDIR)/_Dist/lib/ruby/$(shell $(RUBY) -e 'puts "$(RUBY_VERSION)".split(".")[..1].join(".")').0/bundler
	rm -r $(LIBDIR)/_Dist/lib/ruby/$(shell $(RUBY) -e 'puts "$(RUBY_VERSION)".split(".")[..1].join(".")').0/rdoc
	rm -r $(LIBDIR)/_Dist/lib/ruby/$(shell $(RUBY) -e 'puts "$(RUBY_VERSION)".split(".")[..1].join(".")').0/rubygems
	echo '# This is a Ruby script that does nothing. We make Ruby load this file on startup just because Ruby needs to load a script on startup for some reason.' > $(LIBDIR)/_Dist/bin/mkxp-z
	cp $(RUBY_SRC)/BSDL $(LIBDIR)/_Dist/lib/ruby
	cp $(RUBY_SRC)/COPYING $(LIBDIR)/_Dist/lib/ruby
	cp $(RUBY_SRC)/COPYING.ja $(LIBDIR)/_Dist/lib/ruby
	rm -f $(OUTDIR)/dist.zip
	cd $(LIBDIR)/_Dist && $(ZIP) -r $(OUTDIR)/dist.zip *
	rm -r $(LIBDIR)/_Dist

$(LIBDIR)/Dist/bin/ruby: $(RUBY_SRC)/Makefile ruby-bindings.h
	cd $(RUBY_SRC) && $(MAKE) install DESTDIR=$(LIBDIR)

$(OUTDIR)/sandbox-bindgen/mkxp-sandbox-bindgen.cpp $(OUTDIR)/sandbox-bindgen/mkxp-sandbox-bindgen.h &: sandbox-bindgen.rb $(LIBDIR)/tags
	mkdir -p $(OUTDIR)/sandbox-bindgen
	cd $(LIBDIR) && $(RUBY) $(PWD)/sandbox-bindgen.rb
	mv $(LIBDIR)/mkxp-sandbox-bindgen.h $(OUTDIR)/sandbox-bindgen
	mv $(LIBDIR)/mkxp-sandbox-bindgen.cpp $(OUTDIR)/sandbox-bindgen

$(LIBDIR)/tags: $(LIBDIR)/tags.c
	$(CTAGS) --fields=kS --kinds-c=epx -o $(LIBDIR)/tags $(LIBDIR)/tags.c

$(LIBDIR)/tags.c: $(RUBY_SRC)/.ext/include/$(WASI_TARGET)/ruby/config.h
	mkdir -p $(LIBDIR)
	echo -e '#include <ruby.h>\n#include <ruby/encoding.h>' | $(WASI_CC) -E -I$(RUBY_SRC)/include -I$(RUBY_SRC)/.ext/include/$(WASI_TARGET) -o $(LIBDIR)/tags.c -

$(RUBY_SRC)/Makefile $(RUBY_SRC)/.ext/include/$(WASI_TARGET)/ruby/config.h $(RUBY_SRC)/BSDL $(RUBY_SRC)/COPYING $(RUBY_SRC)/COPYING.ja &: $(RUBY_SRC)/configure $(LIBDIR)/usr/local/lib/libyaml.a $(LIBDIR)/usr/local/lib/libz.a
	cd $(RUBY_SRC) && ./configure \
		--prefix=/Dist \
		--host $(WASI_TARGET) \
		--build $(shell $(RUBY) -e 'puts(RbConfig::CONFIG["host"])') \
		--with-static-linked-ext \
		--with-ext=$(RUBY_EXTS) \
		--with-libyaml-dir=$(LIBDIR)/usr/local \
		--with-zlib-dir=$(LIBDIR)/usr/local \
		--with-baseruby=$(RUBY) \
		WASMOPT=$(WASM_OPT) \
		WASI_SDK_PATH=$(WASI_SDK) \
		$(WASI_TOOLCHAIN) \
		CFLAGS='$(WASI_CFLAGS) -Dmprotect=mkxp_sandbox_mprotect' \
		CXXFLAGS='$(WASI_CXXFLAGS) -Dmprotect=mkxp_sandbox_mprotect' \
		LDFLAGS='-fuse-ld=$(WASI_SDK)/bin/wasm-ld -Wl,--export-dynamic,--stack-first,-z,stack-size=$(RUBY_STACK_SIZE) $(WASI_LDFLAGS)' \
		XCFLAGS='-DWASM_SETJMP_STACK_BUFFER_SIZE=24576 -DWASM_FIBER_STACK_BUFFER_SIZE=24576 -DWASM_SCAN_STACK_BUFFER_SIZE=24576' \
		optflags='-fno-fast-math' \
		wasmoptflags='--enable-bulk-memory --no-inline=Init_* $(WASM_OPT_FLAGS) --pass-arg=asyncify-ignore-imports' \
		ac_cv_func_realpath=no \
		$(shell $(RUBY) -e 'puts "ac_cv_func_dlopen=no" if Gem::Version.new("$(RUBY_VERSION)") < Gem::Version.new("3.3")') \
		$(shell $(RUBY) -e 'puts "--disable-jit-support" if Gem::Version.new("$(RUBY_VERSION)") < Gem::Version.new("3.3")') \
		--disable-rubygems \
		--disable-install-doc

$(RUBY_SRC)/configure: $(RUBY_SRC)/configure.ac
	cd $(RUBY_SRC) && $(RUBY) tool/downloader.rb -d tool -e gnu config.guess config.sub
	cd $(RUBY_SRC) && $(AUTORECONF) -i

$(RUBY_SRC)/configure.ac:
	mkdir -p $(DOWNLOADS)
	$(CLONE) $(GITHUB)/ruby/ruby $(RUBY_SRC) -b v$(subst .,_,$(RUBY_VERSION))
ifneq ($(shell $(RUBY) -e 'puts "1" if Gem::Version.new("$(RUBY_VERSION)") >= Gem::Version.new("3.4")'),)
	cd $(RUBY_SRC) && $(GIT) apply $(PWD)/ruby-compat-3.4.patch
else
	cd $(RUBY_SRC) && $(GIT) apply $(PWD)/ruby-compat.patch
endif
	cd $(RUBY_SRC) && $(GIT) apply $(PWD)/ruby-prng-time.patch
ifeq ($(MKXPZ_RUBY_HAVE_LTO),1)
	cd $(RUBY_SRC) && $(GIT) apply $(PWD)/ruby-lto.patch
endif
ifeq ($(MKXPZ_RUBY_HAVE_SOCKETS),1)
	cd $(RUBY_SRC) && $(GIT) apply $(PWD)/ruby-sockets.patch
endif
ifeq ($(MKXPZ_RUBY_HAVE_USER_THREADS),1)
	cd $(RUBY_SRC) && $(GIT) apply $(PWD)/ruby-user-threads.patch
endif
	echo '#include "$(PWD)/ruby-bindings.h"' >> $(RUBY_SRC)/gc.c

# WABT (targets the build machine)

WABT_SRC := \
	$(DOWNLOADS)/wabt/src/apply-names.cc \
	$(DOWNLOADS)/wabt/src/binary-reader-ir.cc \
	$(DOWNLOADS)/wabt/src/binary-reader-logging.cc \
	$(DOWNLOADS)/wabt/src/binary-reader.cc \
	$(DOWNLOADS)/wabt/src/binary-writer-spec.cc \
	$(DOWNLOADS)/wabt/src/binary-writer.cc \
	$(DOWNLOADS)/wabt/src/binary.cc \
	$(DOWNLOADS)/wabt/src/binding-hash.cc \
	$(DOWNLOADS)/wabt/src/color.cc \
	$(DOWNLOADS)/wabt/src/common.cc \
	$(DOWNLOADS)/wabt/src/config.cc \
	$(DOWNLOADS)/wabt/src/decompiler.cc \
	$(DOWNLOADS)/wabt/src/error-formatter.cc \
	$(DOWNLOADS)/wabt/src/expr-visitor.cc \
	$(DOWNLOADS)/wabt/src/feature.cc \
	$(DOWNLOADS)/wabt/src/filenames.cc \
	$(DOWNLOADS)/wabt/src/generate-names.cc \
	$(DOWNLOADS)/wabt/src/ir-util.cc \
	$(DOWNLOADS)/wabt/src/ir.cc \
	$(DOWNLOADS)/wabt/src/leb128.cc \
	$(DOWNLOADS)/wabt/src/lexer-source-line-finder.cc \
	$(DOWNLOADS)/wabt/src/lexer-source.cc \
	$(DOWNLOADS)/wabt/src/literal.cc \
	$(DOWNLOADS)/wabt/src/opcode-code-table.c \
	$(DOWNLOADS)/wabt/src/opcode.cc \
	$(DOWNLOADS)/wabt/src/option-parser.cc \
	$(DOWNLOADS)/wabt/src/resolve-names.cc \
	$(DOWNLOADS)/wabt/src/sha256.cc \
	$(DOWNLOADS)/wabt/src/shared-validator.cc \
	$(DOWNLOADS)/wabt/src/stream.cc \
	$(DOWNLOADS)/wabt/src/token.cc \
	$(DOWNLOADS)/wabt/src/tracing.cc \
	$(DOWNLOADS)/wabt/src/type-checker.cc \
	$(DOWNLOADS)/wabt/src/utf8.cc \
	$(DOWNLOADS)/wabt/src/validator.cc \
	$(DOWNLOADS)/wabt/src/wast-lexer.cc \
	$(DOWNLOADS)/wabt/src/wast-parser.cc \
	$(DOWNLOADS)/wabt/src/wat-writer.cc \
	$(DOWNLOADS)/wabt/src/c-writer.cc \
	$(DOWNLOADS)/wabt/src/prebuilt/wasm2c_header_top.cc \
	$(DOWNLOADS)/wabt/src/prebuilt/wasm2c_header_bottom.cc \
	$(DOWNLOADS)/wabt/src/prebuilt/wasm2c_source_includes.cc \
	$(DOWNLOADS)/wabt/src/prebuilt/wasm2c_source_declarations.cc \
	$(DOWNLOADS)/wabt/src/prebuilt/wasm2c_simd_source_declarations.cc \
	$(DOWNLOADS)/wabt/src/prebuilt/wasm2c_atomicops_source_declarations.cc \
	$(DOWNLOADS)/wabt/src/interp/binary-reader-interp.cc \
	$(DOWNLOADS)/wabt/src/interp/interp.cc \
	$(DOWNLOADS)/wabt/src/interp/interp-util.cc \
	$(DOWNLOADS)/wabt/src/interp/istream.cc \

WASM2C_SRC := $(WABT_SRC) $(DOWNLOADS)/wabt/src/tools/wasm2c.cc

$(WASM2C): $(addsuffix .o,$(WASM2C_SRC))
	mkdir -p $(LIBDIR)/bin
	$(NATIVE_CXX) $(NATIVE_LDFLAGS) -o $@ $^

$(INDEXER): $(addsuffix .o,$(WABT_SRC)) $(DOWNLOADS)/wabt/src/indexer.o
	mkdir -p $(LIBDIR)/bin
	$(NATIVE_CXX) $(NATIVE_LDFLAGS) -o $@ $^

$(DOWNLOADS)/wabt/src/%.c.o: $(DOWNLOADS)/wabt/src/%.c $(DOWNLOADS)/wabt/include/wabt/config.h $(DOWNLOADS)/picosha2/picosha2.h
	$(NATIVE_CC) -std=gnu17 $(NATIVE_CFLAGS) -I$(DOWNLOADS)/wabt/include -I$(DOWNLOADS)/picosha2 -c -o $@ $<

$(DOWNLOADS)/wabt/src/%.cc.o: $(DOWNLOADS)/wabt/src/%.cc $(DOWNLOADS)/wabt/include/wabt/config.h $(DOWNLOADS)/picosha2/picosha2.h
	$(NATIVE_CXX) -std=gnu++17 $(NATIVE_CXXFLAGS) -I$(DOWNLOADS)/wabt/include -I$(DOWNLOADS)/picosha2 -c -o $@ $<

$(DOWNLOADS)/wabt/src/indexer.o: indexer.cpp $(DOWNLOADS)/wabt/include/wabt/config.h
	$(NATIVE_CXX) -std=gnu++17 $(NATIVE_CXXFLAGS) -I$(DOWNLOADS)/wabt/include -c -o $@ $<

$(DOWNLOADS)/wabt/include/wabt/config.h: $(DOWNLOADS)/wabt/src/config.h.in
	$(SED) \
		-e 's/#cmakedefine WABT_VERSION_STRING "@WABT_VERSION_STRING@"/#define WABT_VERSION_STRING "$(WABT_VERSION) (mkxp-z internal use)"/' \
		-e 's/#cmakedefine WABT_DEBUG @WABT_DEBUG@/#define WABT_DEBUG 0/' \
		-e 's/#cmakedefine01 HAVE_SNPRINTF/#define HAVE_SNPRINTF 1/' \
		-e 's/#cmakedefine01 HAVE_STRCASECMP/#define HAVE_STRCASECMP 1/' \
		-e 's/#cmakedefine01 COMPILER_IS_GNU/#define COMPILER_IS_GNU 1/' \
		-e 's/#cmakedefine01 \(\w*\)/#define \1 0/' \
		$< > $@

$(WASM2C_SRC) $(DOWNLOADS)/wabt/src/config.h.in &:
	mkdir -p $(DOWNLOADS)
	$(CLONE) $(GITHUB)/WebAssembly/wabt $(DOWNLOADS)/wabt -b $(WABT_VERSION)
	cd $(DOWNLOADS)/wabt && $(GIT) apply $(PWD)/wasm2c-data-segments.patch

$(DOWNLOADS)/picosha2/picosha2.h:
	mkdir -p $(DOWNLOADS)/picosha2
	$(CURL) -L -o $@ https://raw.githubusercontent.com/okdshin/PicoSHA2/$(PICOSHA2_VERSION)/picosha2.h

# libyaml (targets WASI)

$(LIBDIR)/usr/local/lib/libyaml.a: $(DOWNLOADS)/libyaml/Makefile
	cd $(DOWNLOADS)/libyaml && $(MAKE) install DESTDIR=$(LIBDIR)

$(DOWNLOADS)/libyaml/Makefile: $(DOWNLOADS)/libyaml/configure
	cd $(DOWNLOADS)/libyaml && ./configure --host $(WASI_TARGET) $(WASI_TOOLCHAIN) CFLAGS='$(WASI_CFLAGS)' CXXFLAGS='$(WASI_CXXFLAGS)' LDFLAGS='$(WASI_LDFLAGS)'

$(DOWNLOADS)/libyaml/configure: $(DOWNLOADS)/libyaml/configure.ac
	cd $(DOWNLOADS)/libyaml && $(AUTORECONF) -i

$(DOWNLOADS)/libyaml/configure.ac:
	mkdir -p $(DOWNLOADS)
	$(CLONE) $(GITHUB)/yaml/libyaml $(DOWNLOADS)/libyaml -b $(LIBYAML_VERSION)

# zlib (targets WASI)

$(LIBDIR)/usr/local/lib/libz.a: $(DOWNLOADS)/zlib/Makefile
	cd $(DOWNLOADS)/zlib && $(MAKE) install DESTDIR=$(LIBDIR)

$(DOWNLOADS)/zlib/Makefile: $(DOWNLOADS)/zlib/configure
	cd $(DOWNLOADS)/zlib && CHOST=linux $(WASI_TOOLCHAIN) CFLAGS='$(WASI_CFLAGS)' CXXFLAGS='$(WASI_CXXFLAGS)' LDFLAGS='$(WASI_LDFLAGS)' ./configure --static

$(DOWNLOADS)/zlib/configure:
	mkdir -p $(DOWNLOADS)
	$(CLONE) $(GITHUB)/madler/zlib $(DOWNLOADS)/zlib -b v$(ZLIB_VERSION)

# Boost

$(OUTDIR)/boost_predef/include/boost/predef.h:
	mkdir -p $(OUTDIR)/boost_predef
	$(CLONE) $(GITHUB)/boostorg/predef $(OUTDIR)/boost_predef -b boost-$(BOOST_VERSION)
