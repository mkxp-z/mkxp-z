project('mesa', 'c', meson_version: '>=1.4.0')

gl = declare_dependency(
    include_directories: [
        'include',
    ],
)

meson.override_dependency('egl', gl, static: true)
meson.override_dependency('gl', gl, static: true)
meson.override_dependency('glesv2', gl, static: true)
meson.override_dependency('glesv3', gl, static: true)
meson.override_dependency('glx', gl, static: true)
meson.override_dependency('opengl', gl, static: true)

gbm = declare_dependency(
    include_directories: [
        'src/gbm',
        'src/gbm/main',
        'src/gbm/backends/dri',
    ],
)

meson.override_dependency('gbm', gbm, static: true)
