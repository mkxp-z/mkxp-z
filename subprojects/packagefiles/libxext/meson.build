project('libxext', 'c', meson_version: '>=1.4.0')

libxext = declare_dependency(
    include_directories: [
        'include',
    ],
    dependencies: [
        dependency('x11', required: true),
        dependency('xau', required: true),
        dependency('xextproto', required: true),
        dependency('xproto', required: true),
    ],
)

meson.override_dependency('xext', libxext, static: true)
