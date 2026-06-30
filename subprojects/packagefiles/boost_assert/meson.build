project('boost_assert', 'cpp', meson_version: '>=1.3.0')
boost_assert = declare_dependency(
    include_directories: 'include',
    dependencies: [
        subproject('boost_config').get_variable('boost_config'),
    ],
)
