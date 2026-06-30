project('boost_throw_exception', 'cpp', meson_version: '>=1.3.0')
boost_throw_exception = declare_dependency(
    include_directories: 'include',
    dependencies: [
        subproject('boost_assert').get_variable('boost_assert'),
        subproject('boost_config').get_variable('boost_config'),
    ],
)
