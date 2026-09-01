wayland_version = configure_file(
    input: 'wayland-version.h.in',
    output: 'wayland-version.h',
    configuration: {
        'WAYLAND_VERSION': meson.project_version(),
        'WAYLAND_VERSION_MAJOR': meson.project_version().split('.')[0].to_int(),
        'WAYLAND_VERSION_MINOR': meson.project_version().split('.')[1].to_int(),
        'WAYLAND_VERSION_MICRO': meson.project_version().split('.')[2].to_int(),
    },
)

wayland_scanner = executable(
    'wayland-scanner',
    include_directories: [
        '.',
    ],
    sources: [
        'wayland-util.c',
        'scanner.c',
    ],
    dependencies: [
        dependency('expat', required: true, native: true),
        declare_dependency(sources: wayland_version),
        declare_dependency(
            sources: custom_target(
                'wayland.dtd.h',
                output: 'wayland.dtd.h',
                command: [
                    import('python').find_installation(required: true),
                    files('embed.py')[0],
                    files('../protocol/wayland.dtd')[0],
                    'wayland_dtd',
                ],
                capture: true,
            ),
        ),
    ],
    native: true,
)

wayland_protocol_headers = [
    custom_target(
        'wayland-client-protocol.h',
        output: 'wayland-client-protocol.h',
        command: [
            wayland_scanner,
            'client-header',
            files('../protocol/wayland.xml')[0],
            '@OUTPUT@',
        ],
    ),
    custom_target(
        'wayland-client-protocol-core.h',
        output: 'wayland-client-protocol-core.h',
        command: [
            wayland_scanner,
            '-c',
            'client-header',
            files('../protocol/wayland.xml')[0],
            '@OUTPUT@',
        ],
    ),
    custom_target(
        'wayland-server-protocol.h',
        output: 'wayland-server-protocol.h',
        command: [
            wayland_scanner,
            'server-header',
            files('../protocol/wayland.xml')[0],
            '@OUTPUT@',
        ],
    ),
    custom_target(
        'wayland-server-protocol-core.h',
        output: 'wayland-server-protocol-core.h',
        command: [
            wayland_scanner,
            '-c',
            'server-header',
            files('../protocol/wayland.xml')[0],
            '@OUTPUT@',
        ],
    ),
]
