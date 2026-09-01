project('theoraplay', 'c', meson_version: '>=1.4.0')

have_thread = get_option('thread').allowed()

theoraplay = declare_dependency(
    include_directories: '.',
    link_with: library(
        'theoraplay',
        dependencies: [
            dependency('theoradec', required: true),
            dependency('vorbis', required: true),
        ],
        c_args: have_thread ? ['-DTHEORAPLAY_ONLY_SINGLE_THREADED=0'] : ['-DTHEORAPLAY_ONLY_SINGLE_THREADED=1'],
        sources: [
            'theoraplay.c',
        ],
    ),
)
