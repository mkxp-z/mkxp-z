project('ruby-3.1', 'c', 'cpp', meson_version: '>=1.4.0')

find_program('cmp', required: true)

cflags = subproject('common').get_variable('base_cflags')
cppflags = subproject('common').get_variable('base_cppflags')

# Ruby versions older than 3.2 don't support address sanitizer
if meson.get_compiler('c').has_argument('-fno-sanitize=address')
    cflags += '-fno-sanitize=address'
    cppflags += '-fno-sanitize=address'
endif

cflags += get_option('c_args')
cppflags += get_option('cpp_args')

ldflags = subproject('common').get_variable('base_ldflags')

ldflags += get_option('c_link_args')

srcdir = meson.project_source_root()
if build_machine.system() in ['windows', 'cygwin']
    srcdir = run_command(subproject('common').get_variable('cygpath'), '-u', srcdir, check: true).stdout().strip()
endif

destdir = meson.project_build_root() / 'dist'
if build_machine.system() in ['windows', 'cygwin']
    destdir = run_command(subproject('common').get_variable('cygpath'), '-m', destdir, check: true).stdout().strip()
endif

baseruby = run_command(subproject('common').get_variable('quotetool'), (meson.version().version_compare('>=1.10.0') ? find_program('ruby').cmd_array() : find_program('ruby')), check: true).stdout().strip()

ruby_deps = [
    dependency('libffi', required: true),
    dependency('yaml-0.1', required: true),
    dependency('zlib', required: true),
]

ruby = declare_dependency(
    dependencies: [
        ruby_deps,
        import('unstable-external_project').add_project(
            'configure',
            configure_options: [
                '--srcdir=' + srcdir,
                '--prefix=' + '@PREFIX@',
                '--prefix=/',
                meson.version().version_compare('>=1.7.0') ? '--bindir=' + '@PREFIX@' / '@BINDIR@' : [],
                '--bindir=/bin',
                '--libdir=' + '@PREFIX@' / '@LIBDIR@',
                '--libdir=/lib',
                '--includedir=' + '@PREFIX@' / '@INCLUDEDIR@',
                '--includedir=/include',
                '--with-rubyhdrdir=/include',
                '--with-rubyarchhdrdir=/include',
                '--with-destdir=' + destdir,
                '--with-baseruby=' + baseruby,
                '--with-soname=ruby',
                '--disable-shared',
                '--disable-install-doc',
                '--with-out-ext=openssl,readline,dbm,gdbm',
                '--with-static-linked-ext',
                '--disable-rubygems',
                '--without-gmp',
                '--enable-mkmf-verbose',
                'CC=' + run_command(subproject('common').get_variable('quotetool'), meson.get_compiler('c').cmd_array(), check: true).stdout().strip(),
                'CXX=' + run_command(subproject('common').get_variable('quotetool'), meson.get_compiler('cpp').cmd_array(), check: true).stdout().strip(),
                'AR=' + run_command(subproject('common').get_variable('quotetool'), subproject('common').get_variable('ar'), check: true).stdout().strip(),
                'RANLIB=' + run_command(subproject('common').get_variable('quotetool'), subproject('common').get_variable('ranlib'), check: true).stdout().strip(),
                'AS=false',
                'LD=false',
                'NM=false',
                'OBJCOPY=false',
                'OBJDUMP=false',
                'STRIP=false',
                'CFLAGS=' + run_command(subproject('common').get_variable('quotetool'), cflags, check: true).stdout().strip() + ' ' + get_option('cflags'),
                'CXXFLAGS=' + run_command(subproject('common').get_variable('quotetool'), cppflags, check: true).stdout().strip() + ' ' + get_option('cppflags'),
                'LDFLAGS=' + run_command(subproject('common').get_variable('quotetool'), ldflags, check: true).stdout().strip() + ' ' + get_option('ldflags'),
            ],
            cross_configure_options: [
                '--host=@HOST@',
                '--host=' + subproject('common').get_variable('host_triplet'),
            ],
            env: {
                'CONFIG_SITE': ' ',
                'MKXP_LIBGCC': meson.get_compiler('c').has_link_argument('-lgcc') ? '-lgcc' : '',
                # Disable LTO while running link tests for Ruby extensions if the linker is LLD (LLVM linker) and we are building for 32-bit ARM.
                # Otherwise, the link tests may fail due to https://github.com/llvm/llvm-project/issues/108262.
                'MKXP_DISABLE_LTO_FOR_LINK_TESTS': (meson.get_compiler('c').get_linker_id().split('.')[-1] == 'lld' or meson.get_compiler('c').get_linker_id() == 'lld-link') and host_machine.cpu_family() == 'arm' ? 'yes' : 'no',
                'DLLWRAP': 'false',
                'WINDRES': run_command(subproject('common').get_variable('quotetool'), subproject('common').get_variable('windres'), check: true).stdout().strip(),
                'YACC': run_command(subproject('common').get_variable('quotetool'), (meson.version().version_compare('>=1.10.0') ? find_program('bison').cmd_array() : find_program('bison')), check: true).stdout().strip(),
                'ac_cv_func_crypt': 'no',
                'ac_cv_func_crypt_r': 'no',
                'ac_cv_header_crypt_h': 'no',
                'ac_cv_lib_crypt_crypt': 'no',
                'ac_cv_lib_z_uncompress': 'yes',
                'rb_cv_broken_crypt': 'no',
                'rb_cv_gcc_atomic_builtins': 'yes',
                'rb_cv_gcc_sync_builtins': 'no',
                'rb_cv_prog_gnu_ld': subproject('common').get_variable('c_linker_is_gnu') ? 'yes' : 'no',
                'rb_cv_symbol_prefix': meson.get_compiler('c').symbols_have_underscore_prefix() ? '_' : 'NONE',
            },
            depends: [
                # Force Meson to fully build all of our vendored dependencies that are required by Ruby before building Ruby itself
                executable(
                    'deps',
                    sources: [
                        'deps-stub.c',
                    ],
                    dependencies: [
                        ruby_deps,
                    ],
                ),
            ],
            verbose: true,
        ).dependency('ruby-static').partial_dependency(
            sources: true,
        ),
        host_machine.system() not in ['windows', 'cygwin', 'darwin']
            ? [
                meson.get_compiler('c').find_library('atomic', required: false),
                meson.get_compiler('c').find_library('dl', required: false),
                meson.get_compiler('c').find_library('rt', required: false),
            ]
            : [],
        host_machine.system() in ['windows', 'cygwin']
            ? [
                meson.get_compiler('c').find_library('shell32', required: true),
                meson.get_compiler('c').find_library('ws2_32', required: true),
                meson.get_compiler('c').find_library('iphlpapi', required: true),
                meson.get_compiler('c').find_library('imagehlp', required: true),
                meson.get_compiler('c').find_library('shlwapi', required: true),
                meson.get_compiler('c').find_library('bcrypt', required: true),
            ]
            : [],
        host_machine.system() == 'freebsd'
            ? [
                meson.get_compiler('c').find_library('procstat', required: true),
            ]
            : [],
        host_machine.system() in ['freebsd', 'netbsd']
            ? [
                meson.get_compiler('c').find_library('execinfo', required: true),
            ]
            : [],
    ],
    compile_args: [
        '-I' + meson.project_build_root() / 'dist' / 'include',
    ],
    link_args: [
        # If the executable format is ELF, we need this linker flag to export Ruby's symbols from our executable so that gems can find them.
        # Otherwise, any Ruby gems that have extensions will fail to load with undefined symbol errors.
        # For non-ELF executable formats (e.g. Mach-O or PE), this behavior is already the default so no linker flag is needed for non-ELF formats.
        meson.get_compiler('c').first_supported_link_argument('-rdynamic', '-Wl,-export-dynamic'),
        (host_machine.system() in ['windows', 'cygwin']) and not meson.get_compiler('c').has_header_symbol('math.h', 'lgamma_r')
            ? [
                meson.project_build_root() / 'build' / 'lgamma_r.o',
            ]
            : [],
        meson.project_build_root() / 'build' / 'ext' / 'extinit.a',
        meson.project_build_root() / 'build' / 'ext' / 'bigdecimal' / 'bigdecimal.a',
        meson.project_build_root() / 'build' / 'ext' / 'cgi' / 'escape' / 'escape.a',
        meson.project_build_root() / 'build' / 'ext' / 'continuation' / 'continuation.a',
        meson.project_build_root() / 'build' / 'ext' / 'coverage' / 'coverage.a',
        meson.project_build_root() / 'build' / 'ext' / 'date' / 'date_core.a',
        meson.project_build_root() / 'build' / 'ext' / 'digest' / 'digest.a',
        meson.project_build_root() / 'build' / 'ext' / 'digest' / 'bubblebabble' / 'bubblebabble.a',
        meson.project_build_root() / 'build' / 'ext' / 'digest' / 'md5' / 'md5.a',
        meson.project_build_root() / 'build' / 'ext' / 'digest' / 'rmd160' / 'rmd160.a',
        meson.project_build_root() / 'build' / 'ext' / 'digest' / 'sha1' / 'sha1.a',
        meson.project_build_root() / 'build' / 'ext' / 'digest' / 'sha2' / 'sha2.a',
        meson.project_build_root() / 'build' / 'ext' / 'etc' / 'etc.a',
        meson.project_build_root() / 'build' / 'ext' / 'fcntl' / 'fcntl.a',
        meson.project_build_root() / 'build' / 'ext' / 'fiddle' / 'fiddle.a',
        meson.project_build_root() / 'build' / 'ext' / 'io' / 'console' / 'console.a',
        meson.project_build_root() / 'build' / 'ext' / 'io' / 'nonblock' / 'nonblock.a',
        meson.project_build_root() / 'build' / 'ext' / 'io' / 'wait' / 'wait.a',
        meson.project_build_root() / 'build' / 'ext' / 'json' / 'generator' / 'generator.a',
        meson.project_build_root() / 'build' / 'ext' / 'json' / 'parser' / 'parser.a',
        meson.project_build_root() / 'build' / 'ext' / 'monitor' / 'monitor.a',
        meson.project_build_root() / 'build' / 'ext' / 'nkf' / 'nkf.a',
        meson.project_build_root() / 'build' / 'ext' / 'objspace' / 'objspace.a',
        meson.project_build_root() / 'build' / 'ext' / 'pathname' / 'pathname.a',
        meson.project_build_root() / 'build' / 'ext' / 'psych' / 'psych.a',
        meson.project_build_root() / 'build' / 'ext' / 'racc' / 'cparse' / 'cparse.a',
        meson.project_build_root() / 'build' / 'ext' / 'rbconfig' / 'sizeof' / 'sizeof.a',
        meson.project_build_root() / 'build' / 'ext' / 'ripper' / 'ripper.a',
        meson.project_build_root() / 'build' / 'ext' / 'socket' / 'socket.a',
        meson.project_build_root() / 'build' / 'ext' / 'stringio' / 'stringio.a',
        meson.project_build_root() / 'build' / 'ext' / 'strscan' / 'strscan.a',
        meson.project_build_root() / 'build' / 'ext' / 'zlib' / 'zlib.a',
        host_machine.system() in ['windows', 'cygwin']
            ? [
                meson.project_build_root() / 'build' / 'ext' / 'win32' / 'resolv' / 'resolv.a',
                meson.project_build_root() / 'build' / 'ext' / 'win32ole' / 'win32ole.a',
            ]
            : [
                meson.project_build_root() / 'build' / 'ext' / 'pty' / 'pty.a',
                meson.project_build_root() / 'build' / 'ext' / 'syslog' / 'syslog.a',
            ],
        meson.project_build_root() / 'build' / 'enc' / 'encinit.a',
        meson.project_build_root() / 'build' / 'enc' / 'libenc.a',
        meson.project_build_root() / 'build' / 'enc' / 'libtrans.a',
        meson.project_build_root() / 'build' / 'libruby-static.a',
    ],
)

ruby_build_dir = meson.project_build_root()

miniruby = custom_target(
    'miniruby',
    output: 'miniruby',
    command: [
        subproject('common').get_variable('python'),
        files('build-miniruby.py'),
        baseruby,
        ruby_build_dir / 'build',
        '@OUTPUT@',
    ],
    depends: [
        executable(
            'miniruby-deps',
            sources: [
                'deps-stub.c',
            ],
            dependencies: [
                ruby,
            ],
        ),
    ],
    build_by_default: true,
)

meson.override_dependency('ruby-3.1', ruby, static: true)
