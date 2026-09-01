project('alsa-lib', 'c', meson_version: '>=1.4.0')

alsa_lib = declare_dependency(
    include_directories: [
        'include',
    ],
)

meson.override_dependency('alsa', alsa_lib, static: true)
