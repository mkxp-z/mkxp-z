project('ibus', 'c', meson_version: '>=1.4.0')

ibus = declare_dependency(
    include_directories: [
        'include',
        'include/ibus-1.0',
    ],
    dependencies: [
        dependency('gio-2.0', required: true),
        dependency('glib-2.0', required: true),
        dependency('gobject-2.0', required: true),
        host_machine.system() in ['dragonfly', 'freebsd', 'netbsd', 'openbsd'] ? dependency('libinotify', required: true) : [],
    ],
)

meson.override_dependency('ibus-1.0', ibus, static: true)
