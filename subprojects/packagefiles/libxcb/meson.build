project('libxcb', 'c', meson_version: '>=1.4.0')

python = import('python').find_installation(required: true)
xcb_proto_root = subproject('xcb-proto', default_options: {'c_std': get_option('c_std'), 'cpp_std': get_option('cpp_std')}).get_variable('xcb_proto_root')
xcb_proto_sources = subproject('xcb-proto', default_options: {'c_std': get_option('c_std'), 'cpp_std': get_option('cpp_std')}).get_variable('xcb_proto_sources')
xcb_proto_generated = []

subdir('include')

libxcb = declare_dependency(
    include_directories: [
        'include',
    ],
    sources: xcb_proto_generated,
)

meson.override_dependency('xcb', libxcb, static: true)
meson.override_dependency('x11-xcb', libxcb, static: true)
