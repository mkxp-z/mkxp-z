project('theoraplay', 'c', meson_version: '>=1.3.0')

theoraplay = declare_dependency(
    include_directories: '.',
    link_with: library(
        'theoraplay',
        c_args: get_option('cflags') + (get_option('have_thread') ? ['-DTHEORAPLAY_ONLY_SINGLE_THREADED=0'] : ['-DTHEORAPLAY_ONLY_SINGLE_THREADED=1']),
        sources: [
            'theoraplay.c',
        ],
        pic: get_option('b_staticpic'),
        gnu_symbol_visibility: 'hidden',
    ),
)
