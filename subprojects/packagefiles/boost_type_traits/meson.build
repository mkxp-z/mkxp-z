project('boost_type_traits', 'cpp', meson_version: '>=1.3.0')
boost_type_traits = declare_dependency(
    include_directories: 'include',
    dependencies: [
        subproject('boost_config').get_variable('boost_config'),
        subproject('boost_static_assert').get_variable('boost_static_assert'),
    ],
)
