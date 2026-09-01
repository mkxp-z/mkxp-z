project('libiconv', 'c', meson_version: '>=1.4.0')

configuration = {
    'BROKEN_WCHAR_H': 0,
    'DLL_VARIABLE': '',
    'EILSEQ': '',
    'HAVE_VISIBILITY': meson.get_compiler('c').has_function_attribute('visibility:default') and meson.get_compiler('c').has_function_attribute('visibility:hidden') ? 1 : 0,
    'ICONV_CONST': '',
    'USE_MBSTATE_T': 0,
}

libiconv_config = import('fs').copyfile(
    'libcharset/config.h.in',
    'config.h',
)

libiconv = declare_dependency(
    include_directories: '.',
    link_with: library(
        'iconv',
        link_with: static_library(
            'localcharset',
            dependencies: declare_dependency(
                sources: [
                    libiconv_config,
                ],
            ),
            c_args: ['-DBUILDING_LIBCHARSET'],
            include_directories: 'libcharset/include',
            sources: [
                'libcharset/lib/localcharset.c',
            ],
            pic: get_option('b_staticpic'),
            gnu_symbol_visibility: 'hidden',
            install: false,
        ),
        dependencies: declare_dependency(
            sources: [
                libiconv_config,
                configure_file(
                    input: 'libcharset/include/localcharset.h.build.in',
                    output: 'localcharset.h',
                    configuration: configuration,
                ),
                configure_file(
                    input: 'include/iconv.h.build.in',
                    output: 'iconv.h',
                    configuration: configuration,
                ),
            ],
        ),
        c_args: ['-DBUILDING_LIBICONV', '-DBUILDING_LIBCHARSET', '-DICONV_CONST=' + configuration['ICONV_CONST'], '-DENABLE_EXTRA'],
        include_directories: 'include',
        sources: [
            'lib/iconv.c',
        ],
        pic: get_option('b_staticpic'),
        gnu_symbol_visibility: 'hidden',
    ),
)

meson.override_dependency('iconv', libiconv)
