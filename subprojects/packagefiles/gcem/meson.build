project('gcem', 'c', meson_version: '>=1.4.0')

gcem = declare_dependency(
    include_directories: [
        'include'
    ],
)

gcem_include_dir = meson.project_source_root() / 'include'

meson.override_dependency('gcem', gcem, static: true)
