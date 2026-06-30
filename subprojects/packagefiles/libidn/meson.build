project('libidn', 'c', meson_version: '>=1.3.0')

libidn = declare_dependency(
    include_directories: 'lib',
    link_with: library(
        'idn',
        c_args: get_option('cflags') + [
            '-DLIBIDN_BUILDING',
            '-DHAVE_STDBOOL_H=1',
            '-DHAVE_VISIBILITY=' + (meson.get_compiler('c').compiles('__attribute__((__visibility__("default"))) void f(void) {}', name: 'GNU visibility attribute support check') ? '1' : '0'),
            '-DICONV_CONST=',
        ],
        include_directories: 'lib',
        sources: [
            'lib/punycode.c',
        ],
        pic: get_option('b_staticpic'),
        gnu_symbol_visibility: 'hidden',
    ),
)
