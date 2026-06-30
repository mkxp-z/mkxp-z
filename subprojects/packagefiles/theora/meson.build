project('theora', 'c', meson_version: '>=1.3.0')

theoradec = declare_dependency(
    include_directories: 'include',
    link_with: library(
        'theoradec',
        c_args: get_option('cflags'),
        include_directories: 'include',
        sources: [
            'lib/apiwrapper.c',
            'lib/bitpack.c',
            'lib/decapiwrapper.c',
            'lib/decinfo.c',
            'lib/decode.c',
            'lib/dequant.c',
            'lib/fragment.c',
            'lib/huffdec.c',
            'lib/idct.c',
            'lib/info.c',
            'lib/internal.c',
            'lib/quant.c',
            'lib/state.c',
        ],
        pic: get_option('b_staticpic'),
        gnu_symbol_visibility: 'hidden',
    ),
)
