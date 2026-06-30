project('boost_core', 'cpp', meson_version: '>=1.3.0')
boost_core = declare_dependency(
    include_directories: 'include',
    dependencies: [
        subproject('boost_assert').get_variable('boost_assert'),
        subproject('boost_config').get_variable('boost_config'),
        subproject('boost_throw_exception').get_variable('boost_throw_exception'),
    ],
)
