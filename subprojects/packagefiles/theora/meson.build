project('theora', 'c', meson_version: '>=1.4.0')

theoradec = declare_dependency(
    include_directories: 'include',
    dependencies: [
        dependency('ogg', required: true),
    ],
    link_with: library(
        'theoradec',
        dependencies: [
            dependency('ogg', required: true),
        ],
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

meson.override_dependency('theora', theoradec)
meson.override_dependency('theoradec', theoradec)
