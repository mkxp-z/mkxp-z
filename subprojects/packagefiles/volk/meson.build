project('volk', 'c', meson_version: '>=1.4.0')

vulkan_headers = subproject('vulkan-headers', default_options: {'c_std': get_option('c_std'), 'cpp_std': get_option('cpp_std')}).get_variable('vulkan_headers')
xcb = dependency('x11-xcb', required: false).partial_dependency(compile_args: true, includes: true, sources: true)
x11 = dependency('x11', required: false).partial_dependency(compile_args: true, includes: true, sources: true)
xrandr = dependency('xrandr', required: false).partial_dependency(compile_args: true, includes: true, sources: true)

deps = [
    vulkan_headers,
    xcb,
    x11,
    xrandr,
]

volk = declare_dependency(
    dependencies: deps,
    include_directories: [
        '.',
    ],
    link_with: [
        library(
            'volk',
            dependencies: [
                deps,
                meson.get_compiler('c').find_library(host_machine.system() in ['windows', 'cygwin'] ? 'kernel32' : 'dl', required: true),
            ],
            include_directories: [
                '.',
            ],
            c_args: [
                '-DVK_USE_PLATFORM_IOS_MVK',
                '-DVK_USE_PLATFORM_MACOS_MVK',
                '-DVK_USE_PLATFORM_METAL_EXT',
                '-DVK_USE_PLATFORM_VI_NN',
                '-DVK_USE_PLATFORM_WAYLAND_KHR',
                host_machine.system() in ['windows', 'cygwin'] ? '-DVK_USE_PLATFORM_WIN32_KHR' : [],
                xcb.found() ? '-DVK_USE_PLATFORM_XCB_KHR' : [],
                x11.found() ? '-DVK_USE_PLATFORM_XLIB_KHR' : [],
                x11.found() and xrandr.found() ? '-DVK_USE_PLATFORM_XLIB_XRANDR_EXT' : [],
            ],
            sources: [
                'volk.c',
            ],
        ),
    ],
)

meson.override_dependency('volk', volk)
