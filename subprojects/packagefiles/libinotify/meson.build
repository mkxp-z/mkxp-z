project('libinotify', 'c', meson_version: '>=1.4.0')

cflags = subproject('common').get_variable('base_cflags')

cflags += get_option('c_args')

ldflags = subproject('common').get_variable('base_ldflags')

ldflags += get_option('c_link_args')

srcdir = meson.project_source_root()
if build_machine.system() in ['windows', 'cygwin']
    srcdir = run_command(subproject('common').get_variable('cygpath'), '-u', srcdir, check: true).stdout().strip()
endif

libinotify = declare_dependency(
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
                'CC=' + run_command(subproject('common').get_variable('quotetool'), meson.get_compiler('c').cmd_array(), check: true).stdout().strip(),
                'AR=' + run_command(subproject('common').get_variable('quotetool'), subproject('common').get_variable('ar'), check: true).stdout().strip(),
                'RANLIB=' + run_command(subproject('common').get_variable('quotetool'), subproject('common').get_variable('ranlib'), check: true).stdout().strip(),
                'AS=false',
                'LD=false',
                'NM=false',
                'OBJCOPY=false',
                'OBJDUMP=false',
                'STRIP=false',
                'CFLAGS=' + run_command(subproject('common').get_variable('quotetool'), cflags, check: true).stdout().strip(),
                'LDFLAGS=' + run_command(subproject('common').get_variable('quotetool'), ldflags, check: true).stdout().strip(),
            ],
            cross_configure_options: [
                '--host=@HOST@',
                '--host=' + subproject('common').get_variable('host_triplet'),
            ],
            env: {
                'CONFIG_SITE': ' ',
                'lt_cv_prog_gnu_ld': subproject('common').get_variable('c_linker_is_gnu') ? 'yes' : 'no',
            },
            verbose: true,
        ).dependency('inotify').partial_dependency(
            sources: true,
        ),
    ],
    compile_args: [
        '-I' + meson.project_build_root() / 'dist' / 'include',
    ],
    link_args: [
        meson.project_build_root() / 'dist' / 'lib' / 'libinotify.a',
    ],
)

meson.override_dependency('libinotify', libinotify, static: true)
