project('libx11', 'c', meson_version: '>=1.4.0')

libx11 = declare_dependency(
    include_directories: [
        'include',
    ],
    dependencies: [
        dependency('x11-xcb', required: true),
        dependency('kbproto', required: true),
        dependency('inputproto', required: true),
        dependency('xextproto', required: true),
        dependency('xproto', required: true),
    ],
)

meson.override_dependency('x11', libx11, static: true)
