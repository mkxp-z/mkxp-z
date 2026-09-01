arch_deps = configure_file(
    input: 'dbus-arch-deps.h.in',
    output: 'dbus-arch-deps.h',
    format: 'cmake@',
    configuration: {
        'DBUS_VERSION': meson.project_version(),
        'DBUS_MAJOR_VERSION': meson.project_version().split('.')[0].to_int(),
        'DBUS_MINOR_VERSION': meson.project_version().split('.')[1].to_int(),
        'DBUS_MICRO_VERSION': meson.project_version().split('.')[2].to_int(),
        'DBUS_SIZEOF_VOID_P': sizeof_void_p,
        'DBUS_INT16_TYPE': type16,
        'DBUS_INT32_TYPE': type32,
        'DBUS_INT64_TYPE': type64,
        'DBUS_INT64_MODIFIER': modifier64,
        'DBUS_INT64_CONSTANT': '(val##' + modifier64 + ')',
        'DBUS_UINT64_CONSTANT': '(val##U' + modifier64 + ')',
    },
)
