project('spirv-headers', 'c', meson_version: '>=1.4.0')

spirv_headers = declare_dependency(
    include_directories: [
        'include',
    ],
)

spirv_headers_source_dir = meson.project_source_root()

meson.override_dependency('SPIRV-Headers', spirv_headers, static: true)
