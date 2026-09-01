project('libxcursor', 'c', meson_version: '>=1.4.0')

libxcursor = declare_dependency(
    include_directories: [
        'include',
    ],
    dependencies: [
        dependency('fixesproto', required: true),
        dependency('x11', required: true),
    ],
)

meson.override_dependency('xcursor', libxcursor, static: true)
