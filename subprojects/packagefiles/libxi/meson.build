project('libxi', 'c', meson_version: '>=1.4.0')

libxi = declare_dependency(
    include_directories: [
        'include',
    ],
    dependencies: [
        dependency('inputproto', required: true),
        dependency('x11', required: true),
        dependency('xext', required: true),
        dependency('xextproto', required: true),
        dependency('xfixes', required: true),
        dependency('xproto', required: true),
    ],
)

meson.override_dependency('xi', libxi, static: true)
