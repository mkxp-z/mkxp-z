project('xxhash', 'c', meson_version: '>=1.4.0')

xxhash = declare_dependency(
    include_directories: '.',
    link_with: library(
        'xxhash',
        sources: 'xxhash.c',
    ),
)

meson.override_dependency('libxxhash', xxhash)
