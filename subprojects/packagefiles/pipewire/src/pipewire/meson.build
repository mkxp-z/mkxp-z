pipewire_version = configure_file(
    input: 'version.h.in',
    output: 'version.h',
    configuration: {
        'PIPEWIRE_VERSION_MAJOR': meson.project_version().split('.')[0].to_int(),
        'PIPEWIRE_VERSION_MINOR': meson.project_version().split('.')[1].to_int(),
        'PIPEWIRE_VERSION_MICRO': meson.project_version().split('.')[2].to_int(),
        'PIPEWIRE_API_VERSION': '"' + apiversion + '"',
    },
)
