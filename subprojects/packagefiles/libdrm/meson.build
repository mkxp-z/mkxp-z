project('libdrm', 'c', meson_version: '>=1.4.0')

libdrm = declare_dependency(
    include_directories: [
        '.',
        'include/drm',
    ],
)

meson.override_dependency('libdrm', libdrm, static: true)
