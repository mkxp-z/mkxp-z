project('libxrender', 'c', meson_version: '>=1.4.0')

libxrender = declare_dependency(
    include_directories: [
        'include',
    ],
    dependencies: [
        dependency('renderproto', required: true),
        dependency('x11', required: true),
    ],
)

meson.override_dependency('xrender', libxrender, static: true)
