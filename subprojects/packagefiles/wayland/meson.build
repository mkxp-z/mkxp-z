project('wayland', 'c', version: '1.23.1', meson_version: '>=1.4.0')

subdir('src')

meson.override_find_program('wayland-scanner', wayland_scanner)
meson.override_dependency('wayland-scanner', declare_dependency(variables: {'wayland-scanner': 'wayland-scanner'}), static: true)

wayland = declare_dependency(
    include_directories: [
        'src',
    ],
    dependencies: [
        declare_dependency(sources: wayland_version),
    ],
)

meson.override_dependency('wayland-client', wayland, static: true)
meson.override_dependency('wayland-server', wayland, static: true)

wayland_cursor = declare_dependency(
    include_directories: [
        'cursor',
    ],
)

meson.override_dependency('wayland-cursor', wayland_cursor, static: true)

wayland_egl = declare_dependency(
    include_directories: [
        'egl',
    ],
    dependencies: [
        wayland,
    ],
)

meson.override_dependency('wayland-egl', wayland_egl, static: true)
meson.override_dependency('wayland-egl-backend', wayland_egl, static: true)

wayland_protocols = declare_dependency(
    include_directories: [
        'src',
    ],
    dependencies: [
        declare_dependency(sources: wayland_protocol_headers),
        wayland,
    ],
)

meson.override_dependency('wayland-protocols', wayland_protocols, static: true)
