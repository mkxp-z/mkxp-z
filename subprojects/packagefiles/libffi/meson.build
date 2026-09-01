project('libffi', 'c', 'cpp', meson_version: '>=1.4.0')

find_program('cmp', required: true)
find_program('diff', required: true)

cflags = subproject('common').get_variable('base_cflags')
cppflags = subproject('common').get_variable('base_cppflags')

cflags += get_option('c_args')
cppflags += get_option('cpp_args')

ldflags = subproject('common').get_variable('base_ldflags')

ldflags += get_option('c_link_args')

srcdir = meson.project_source_root()
if build_machine.system() in ['windows', 'cygwin']
    srcdir = run_command(subproject('common').get_variable('cygpath'), '-u', srcdir, check: true).stdout().strip()
endif

if meson.get_compiler('c').get_argument_syntax() == 'msvc'
    cc_basename = 'cl'
else
    cc_basename = 'gcc'
endif

libffi_includes = [
    '-I' + meson.project_build_root() / 'dist' / 'include',
]

libffi_libs = [
    '-L' + meson.project_build_root() / 'dist' / 'lib',
]

libffi = declare_dependency(
    dependencies: [
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
                '--enable-static',
                '--disable-shared',
                '--disable-multi-os-directory',
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
                'CFLAGS=' + run_command(subproject('common').get_variable('quotetool'), cflags, check: true).stdout().strip(),
                'CXXFLAGS=' + run_command(subproject('common').get_variable('quotetool'), cppflags, check: true).stdout().strip(),
                'LDFLAGS=' + run_command(subproject('common').get_variable('quotetool'), ldflags, check: true).stdout().strip(),
            ],
            cross_configure_options: [
                '--host=@HOST@',
                '--host=' + subproject('common').get_variable('host_triplet'),
            ],
            env: {
                'CONFIG_SITE': ' ',
                'MKXP_CC_BASENAME': cc_basename,
                'lt_cv_prog_gnu_ld': subproject('common').get_variable('c_linker_is_gnu') ? 'yes' : 'no',
            },
            verbose: true,
        ).dependency('ffi').partial_dependency(
            sources: true,
        ),
    ],
    compile_args: [
        libffi_includes,
    ],
    link_args: [
        meson.project_build_root() / 'dist' / 'lib' / (cc_basename == 'cl' ? 'ffi.lib' : 'libffi.a'),
    ],
)

meson.override_dependency('libffi', libffi, static: true)
