project('dbus', 'c', version: '1.16.2', meson_version: '>=1.4.0')

sizeof_void_p = meson.get_compiler('c').sizeof('void *')
if sizeof_void_p <= 0
    error('failed to get size of void pointer')
endif

sizes = [
    16,
    32,
    64,
]

foreach type, modifier : {
    'short': '',
    'int': '',
    'long': 'L',
    'long long': 'LL',
    '__int64': 'I64',
}
    size = 8 * meson.get_compiler('c').sizeof(type)
    if size in sizes and get_variable('type' + size.to_string(), '') == ''
        set_variable('type' + size.to_string(), type)
        set_variable('modifier' + size.to_string(), modifier)
    endif
endforeach

foreach size : sizes
    if get_variable('type' + size.to_string(), '') == ''
        error('failed to find ' + size.to_string() + '-bit integer type')
    endif
    message(size.to_string() + '-bit integer type: ' + get_variable('type' + size.to_string()) + ' (modifier \'' + get_variable('modifier' + size.to_string()) + '\')')
endforeach

subdir('dbus')

dbus = declare_dependency(
    include_directories: [
        '.',
    ],
    dependencies: [
        declare_dependency(sources: arch_deps),
    ],
)

meson.override_dependency('dbus-1', dbus, static: true)
