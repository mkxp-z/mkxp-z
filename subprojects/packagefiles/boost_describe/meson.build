project('boost_describe', 'cpp', meson_version: '>=1.3.0')
boost_describe = declare_dependency(
    include_directories: 'include',
    dependencies: [
        subproject('boost_mp11').get_variable('boost_mp11'),
    ],
)
