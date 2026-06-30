project('boost_container_hash', 'cpp', meson_version: '>=1.3.0')
boost_container_hash = declare_dependency(
    include_directories: 'include',
    dependencies: [
        subproject('boost_config').get_variable('boost_config'),
        subproject('boost_describe').get_variable('boost_describe'),
        subproject('boost_mp11').get_variable('boost_mp11'),
    ],
)
