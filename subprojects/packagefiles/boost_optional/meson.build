project('boost_optional', 'cpp', meson_version: '>=1.3.0')
boost_optional = declare_dependency(
    include_directories: 'include',
    dependencies: [
        subproject('boost_assert').get_variable('boost_assert'),
        subproject('boost_config').get_variable('boost_config'),
        subproject('boost_core').get_variable('boost_core'),
        subproject('boost_throw_exception').get_variable('boost_throw_exception'),
        subproject('boost_type_traits').get_variable('boost_type_traits'),
    ],
)
