if get_option('mri_includes') == ''
    ver = get_option('mri_version')
    if ver.version_compare('<=1.8')
        global_args += '-DMKXPZ_LEGACY_RUBY'
    elif ver.version_compare('>=3.0') and compilers['cpp'].get_id() == 'clang'
        global_args += '-fdeclspec'
    endif
    global_dependencies += dependency('ruby-' + ver)

else
    if host_system == 'windows'
        include_paths = get_option('mri_includes').split(';')
    else
        include_paths = get_option('mri_includes').split(':')
    endif
    foreach inc : include_paths
        global_args += ('-I' + inc)
    endforeach
    global_dependencies += compilers['cpp'].find_library(get_option('mri_library'), dirs: get_option('mri_libpath'))
endif

global_include_dirs += include_directories('.')

binding_source = [files(
    'binding-mri.cpp',
    'binding-util.cpp',
    'table-binding.cpp',
    'etc-binding.cpp',
    'bitmap-binding.cpp',
    'font-binding.cpp',
    'graphics-binding.cpp',
    'input-binding.cpp',
    'sprite-binding.cpp',
    'viewport-binding.cpp',
    'plane-binding.cpp',
    'window-binding.cpp',
    'tilemap-binding.cpp',
    'audio-binding.cpp',
    'module_rpg.cpp',
    'filesystem-binding.cpp',
    'windowvx-binding.cpp',
    'tilemapvx-binding.cpp',
    'http-binding.cpp'
)]

if steamworks == true
    binding_source += files('cusl-binding.cpp')
endif

if miniffi == true
    binding_source += files('miniffi-binding.cpp')
    binding_source += files('miniffi.cpp')
endif

if (get_option('build_gem') == true)
    binding_source += files('gem-binding.cpp')
endif

global_sources += binding_source