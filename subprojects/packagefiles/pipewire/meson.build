project('pipewire', 'c', version: '1.6.5', meson_version: '>=1.4.0')
apiversion = '0.3'

subdir('src/pipewire')

pipewire = declare_dependency(
    include_directories: [
        'src',
        'spa/include',
    ],
    dependencies: [
        declare_dependency(sources: pipewire_version),
    ],
)

meson.override_dependency('libpipewire-' + apiversion, pipewire, static: true)
