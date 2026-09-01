project('jack1', 'c', meson_version: '>=1.4.0')

jack1 = declare_dependency(
    include_directories: [
        '.'
    ],
)

meson.override_dependency('jack', jack1, static: true)
