project('libxkbcommon', 'c', meson_version: '>=1.4.0')

libxkbcommon = declare_dependency(include_directories: 'include')

meson.override_dependency('xkbcommon', libxkbcommon, static: true)
meson.override_dependency('xkbcommon-x11', libxkbcommon, static: true)
