project('systemd', 'c', meson_version: '>=1.4.0')

libudev = declare_dependency(
    include_directories: [
        'src/libudev',
    ],
)

meson.override_dependency('libudev', libudev, static: true)
