project('pulseaudio', 'c', version: '17.0', meson_version: '>=1.4.0')
pa_api_version = 12
pa_protocol_version = 35

subdir('src/pulse')

pulseaudio = declare_dependency(
    include_directories: [
        'src',
    ],
    dependencies: [
        declare_dependency(sources: pulse_version),
    ],
)

meson.override_dependency('libpulse', pulseaudio, static: true)
