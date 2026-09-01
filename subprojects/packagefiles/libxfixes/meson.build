project('libxfixes', 'c', meson_version: '>=1.4.0')

libxfixes = declare_dependency(
    include_directories: [
        'include',
    ],
    dependencies: [
        dependency('x11', required: true),
        dependency('xproto', required: true),
    ],
)

meson.override_dependency('xfixes', libxfixes, static: true)
