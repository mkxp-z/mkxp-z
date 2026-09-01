project('steamshim', 'c', 'cpp', meson_version: '>=1.4.0')
if host_machine.system() == 'darwin'
    add_languages('objcpp', native: false)
endif

if host_machine.system() == 'darwin'
    steam_api = 'osx' / 'libsteam_api.dylib'
elif host_machine.system() == 'linux' and host_machine.cpu_family() == 'x86_64'
    steam_api = 'linux64' / 'libsteam_api.so'
elif host_machine.system() in ['windows', 'cygwin'] and host_machine.cpu_family() == 'x86'
    steam_api = 'steam_api.dll'
elif host_machine.system() in ['windows', 'cygwin'] and host_machine.cpu_family() == 'x86_64'
    steam_api = 'win64' / 'steam_api64.dll'
else
    error('unsupported platform')
endif

steamshim_child = declare_dependency(
    include_directories: [
        '.',
    ],
    link_with: [
        static_library(
            'steamshim_child',
            sources: [
                'steamshim_child.c',
            ],
        ),
    ],
)

steamshim_parent = declare_dependency(
    dependencies: [
        host_machine.system() == 'darwin'
            ? [
                dependency(
                    'appleframeworks',
                    modules: [
                        'Foundation',
                    ],
                    required: true,
                ),
            ]
            : [],
    ],
    compile_args: [
        '-I' + (get_option('steamworks_path') / 'public'),
    ],
    link_args: [
        get_option('steamworks_path') / 'redistributable_bin' / steam_api,
    ],
    sources: files(
        'steamshim_parent.cpp',
        host_machine.system() == 'darwin'
            ? [
                'chdir_to_executable_path.mm',
            ]
            : [],
    ),
)
