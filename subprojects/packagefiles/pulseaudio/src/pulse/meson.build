pulse_version = configure_file(
    input: 'version.h.in',
    output: 'version.h',
    configuration: {
        'PA_MAJOR': meson.project_version().split('.')[0].to_int(),
        'PA_MINOR': meson.project_version().split('.')[1].to_int(),
        'PA_API_VERSION': pa_api_version,
        'PA_PROTOCOL_VERSION': pa_protocol_version,
    },
)
