project('libxrandr', 'c', meson_version: '>=1.4.0')

libxrandr = declare_dependency(
    include_directories: [
        'include',
    ],
    dependencies: [
        dependency('randrproto', required: true),
        dependency('xrender', required: true),
    ],
)

meson.override_dependency('xrandr', libxrandr, static: true)
