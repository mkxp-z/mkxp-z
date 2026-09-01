foreach dep_name : [
    host_system != 'linux' ? 'alsa' : '',
    'audioio',
    'avahi-client',
    'epoll-shim',
    'fftw3f',
    'girepository-2.0',
    'gobject-introspection-1.0',
    'gthread-2.0',
    'gtk+-3.0',
    'intl',
    not is_unix ? 'jack' : '',
    'libapparmor',
    'libasyncns',
    'libcanberra',
    'libcrypt',
    'libffado',
    'libpcre2-8',
    not is_unix ? 'libpulse' : '',
    'libpulse-simple',
    'libselinux',
    'libsystemd',
    'libusb-1.0',
    'libxcrypt',
    'mount',
    'ncursesw',
    'oboe',
    host_system != 'android' ? 'opensl' : '',
    'opus',
    not is_unix ? 'oss' : '',
    'portaudio',
    'roc',
    'sdl3',
    'snapd-glib',
    'snapd-glib-2',
    'sndfile',
    'sndio',
    'systemd',
    'vulkan',
    'xscrnsaver',
    'xtrans',
]
    if dep_name != ''
        meson.override_dependency(dep_name, null_dep, static: true)
    endif
endforeach

cmake_base_options = {
    'CMAKE_POLICY_VERSION_MINIMUM': '3.10',
    'CMAKE_OSX_SYSROOT': '/nonexistent',
    'CMAKE_C_STANDARD': common.get_variable('c_std').replace('-std=c', '').replace('-std=gnu', ''),
    'CMAKE_CXX_STANDARD': common.get_variable('cpp_std').replace('-std=c++', '').replace('-std=gnu++', ''),
    'CMAKE_C_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
    'CMAKE_CXX_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
    'CMAKE_OBJC_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
    'CMAKE_OBJCXX_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
    'CMAKE_BUILD_TYPE': 'Custom',
    'CMAKE_EXE_LINKER_FLAGS': run_command(quotetool, common.get_variable('c_linker_flags'), check: true).stdout().strip(),
}

cmake_override_options = {
    'b_coverage': 'false',
}

meson_base_options = cmake_override_options + {
    'default_library': 'static',
    'c_std': c_std,
    'cpp_std': cpp_std,
}

git_hash_fallback = ''
foreach line : import('fs').read('../git-hash').splitlines()
    line = line.strip()
    if line != '' and not line.startswith('#')
        git_hash_fallback = line.substring(0, 7)
    endif
endforeach
distscript_vcs_command = []
if git_hash_fallback != ''
    git_hash_file = configure_file(
        input: 'git-hash.h.in',
        output: 'git-hash.h',
        configuration: {
            'VCS_TAG': git_hash_fallback,
        },
    )
else
    git = meson.version().version_compare('>=1.10.0') ? find_program('git').cmd_array() : find_program('git')
    git_hash_file = vcs_tag(
        input: 'git-hash.h.in',
        output: 'git-hash.h',
        command: [git, 'rev-parse', '--short=7', 'HEAD'],
        fallback: 'unknown',
    )
    distscript_vcs_command = [git, 'rev-parse', 'HEAD']
endif
meson.add_dist_script(python, files('../distscript.py'), get_option('dist_subprojects'), distscript_vcs_command)
global_dependencies += declare_dependency(sources: git_hash_file)

gl = dependency('gl', allow_fallback: false, required: get_option('gl_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
if not gl.found()
    gl = subproject('mesa', default_options: meson_base_options).get_variable('gl')
endif

libiconv = dependency('iconv', allow_fallback: false, required: get_option('iconv_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
if not libiconv.found()
    libiconv_options = meson_base_options
    libiconv = subproject('libiconv', default_options: libiconv_options).get_variable('libiconv')
endif
global_dependencies += libiconv

zlib = dependency('zlib', allow_fallback: false, method: 'pkg-config', required: get_option('zlib_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
if not zlib.found()
    zlib_options = cmake.subproject_options()
    zlib_options.add_cmake_defines(cmake_base_options + {
        'BUILD_SHARED_LIBS': false,
        'ZLIB_BUILD_EXAMPLES': false,
    })
    foreach opt, val : cmake_override_options
        zlib_options.set_override_option(opt, val)
    endforeach
    zlib = cmake.subproject('zlib', options: zlib_options).dependency('z')
    zlib_includes = ['-I' + meson.project_source_root() / 'subprojects' / ('zlib-' + zlib.version()), '-I' + meson.project_build_root() / 'subprojects' / ('zlib-' + zlib.version()) / '__CMake_build']
    zlib_libs = ['-L' + meson.project_build_root() / 'subprojects' / ('zlib-' + zlib.version())]
    meson.override_dependency('zlib', zlib, static: true)
else
    zlib_includes = ['-I' + zlib.get_variable(pkgconfig: 'includedir')]
    zlib_libs = ['-L' + zlib.get_variable(pkgconfig: 'libdir')]
endif
global_dependencies += zlib

require_system_openssl = get_option('openssl_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled)
if get_option('openssl').disabled() or (get_option('openssl').auto() and require_system_openssl.enabled())
    require_system_openssl = get_option('openssl')
endif
openssl = dependency('openssl', allow_fallback: false, required: require_system_openssl)
if not openssl.found() and not get_option('openssl').disabled() and not get_option('openssl_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled).enabled()
    openssl_options = meson_base_options + {
        'asm': 'disabled',
    }
    openssl = subproject('openssl', default_options: openssl_options).get_variable('openssl_dep')
    meson.override_dependency('openssl', openssl, static: true)
    if host_system not in ['windows', 'cygwin', 'darwin']
        atomic = meson.get_compiler('c').find_library('atomic', required: false)
        if atomic.found()
            global_dependencies += atomic
        endif
    endif
endif
have_openssl = openssl.found()
if have_openssl
    gplv3_features += 'OpenSSL'
    global_dependencies += openssl
    global_args += '-DMKXPZ_SSL'
else
    meson.override_dependency('openssl', null_dep, static: true)
endif

physfs = dependency('physfs', allow_fallback: false, required: get_option('physfs_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
if not physfs.found()
    physfs_options = cmake.subproject_options()
    physfs_options.add_cmake_defines(cmake_base_options + {
        'CMAKE_OBJC_FLAGS': run_command(quotetool, '-fno-objc-arc', '-fno-objc-weak', check: true).stdout().strip(),
        'PHYSFS_BUILD_STATIC': true,
        'PHYSFS_BUILD_SHARED': false,
        'PHYSFS_BUILD_TEST': false,
        'PHYSFS_BUILD_DOCS': false,
    })
    foreach opt, val : cmake_override_options
        physfs_options.set_override_option(opt, val)
    endforeach
    physfs = declare_dependency(dependencies: [
        cmake.subproject('physfs', options: physfs_options).dependency('physfs-static'),
        host_system == 'darwin'
            ? dependency(
                'appleframeworks',
                modules: [
                    'Foundation',
                    'IOKit',
                ],
                required: true,
            )
            : [],
    ])
    meson.override_dependency('physfs', physfs, static: true)
endif
global_dependencies += physfs

sdl2 = dependency('sdl2', allow_fallback: false, method: 'pkg-config', required: get_option('sdl_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
sdl2main = sdl2

if is_unix and (not sdl2.found() or not sdl2main.found())
    if is_bsd
        libinotify = dependency('libinotify', allow_fallback: false, required: get_option('libinotify_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
        if not libinotify.found()
            libinotify_options = meson_base_options
            libinotify = subproject('libinotify', default_options: libinotify_options).get_variable('libinotify')
        endif
    else
        meson.override_dependency('libinotify', null_dep, static: true)
    endif

    libudev = dependency('libudev', allow_fallback: false, required: get_option('libudev_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
    if not libudev.found()
        libudev = subproject('systemd', default_options: meson_base_options).get_variable('libudev')
    endif

    dbus = dependency('dbus-1', allow_fallback: false, required: get_option('dbus_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
    if not dbus.found()
        dbus = subproject('dbus', default_options: meson_base_options).get_variable('dbus')
    endif

    ibus = dependency('ibus-1.0', allow_fallback: false, required: get_option('ibus_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
    if not ibus.found()
        glib = dependency('glib-2.0', allow_fallback: false, required: get_option('glib_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
        if not glib.found()
            glib = subproject('glib', default_options: meson_base_options + {
                'bsymbolic_functions': false,
                'dtrace': 'disabled',
                'glib_assert': false,
                'glib_checks': false,
                'glib_debug': 'disabled',
                'introspection': 'disabled',
                'libelf': 'disabled',
                'sysprof': 'disabled',
                'systemtap': 'disabled',
                'tests': false,
                'xattr': false,
            }).get_variable('libglib_dep')
        endif

        ibus = subproject('ibus', default_options: meson_base_options).get_variable('ibus')
    endif

    libdrm = dependency('libdrm', allow_fallback: false, required: get_option('libdrm_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
    if not libdrm.found()
        libdrm = subproject('libdrm', default_options: meson_base_options).get_variable('libdrm')
    endif

    x11 = dependency('x11', allow_fallback: false, required: get_option('x11_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
    if not x11.found()
        subproject('xorgproto', default_options: meson_base_options)
        subproject('libxau', default_options: meson_base_options)
        subproject('libxcb', default_options: meson_base_options)
        x11 = subproject('libx11', default_options: meson_base_options).get_variable('libx11')
        subproject('libxfixes', default_options: meson_base_options)
        subproject('libxext', default_options: meson_base_options)
        subproject('libxi', default_options: meson_base_options)
        subproject('libxrender', default_options: meson_base_options)
        subproject('libxrandr', default_options: meson_base_options)
        subproject('libxcursor', default_options: meson_base_options)
    endif

    wayland_client = dependency('wayland-client', allow_fallback: false, required: get_option('wayland_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
    if not wayland_client.found()
        subproject('expat', default_options: meson_base_options)

        wayland_client = subproject('wayland', default_options: meson_base_options).get_variable('wayland')
    endif

    libxkbcommon = dependency('xkbcommon', allow_fallback: false, required: get_option('libxkbcommon_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
    if not libxkbcommon.found()
        libxkbcommon = subproject('libxkbcommon', default_options: meson_base_options).get_variable('libxkbcommon')
    endif

    libdecor = dependency('libdecor-0', allow_fallback: false, required: get_option('libdecor_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
    if not libdecor.found()
        libdecor = subproject('libdecor', default_options: meson_base_options).get_variable('libdecor')
    endif
else
    foreach dep_name : [
        'dbus-1',
        'gio-2.0',
        'glib-2.0',
        'gmodule-2.0',
        'gobject-2.0',
        'ibus-1.0',
        'libdecor-0',
        'libdrm',
        'libinotify',
        'libudev',
        'wayland-client',
        'wayland-cursor',
        'wayland-egl',
        'wayland-egl-backend',
        'wayland-protocols',
        'wayland-scanner',
        'wayland-server',
        'x11',
        'x11-xcb',
        'xau',
        'xcb',
        'xcursor',
        'xext',
        'xfixes',
        'xi',
        'xkbcommon',
        'xkbcommon-x11',
        'xrandr',
        'xrender',
    ]
        meson.override_dependency(dep_name, null_dep, static: true)
    endforeach
endif

if is_unix
    global_args += [
        '-DMKXPZ_WAYLAND_CLIENT_SONAME="' + run_command(sonametool, 'wayland-client', check: true).stdout().strip() + '"',
        '-DMKXPZ_WAYLAND_CURSOR_SONAME="' + run_command(sonametool, 'wayland-cursor', check: true).stdout().strip() + '"',
        '-DMKXPZ_WAYLAND_EGL_SONAME="' + run_command(sonametool, 'wayland-egl', check: true).stdout().strip() + '"',
        '-DMKXPZ_X11_SONAME="' + run_command(sonametool, 'X11', check: true).stdout().strip() + '"',
        '-DMKXPZ_XCURSOR_SONAME="' + run_command(sonametool, 'Xcursor', check: true).stdout().strip() + '"',
        '-DMKXPZ_XEXT_SONAME="' + run_command(sonametool, 'Xext', check: true).stdout().strip() + '"',
        '-DMKXPZ_XFIXES_SONAME="' + run_command(sonametool, 'Xfixes', check: true).stdout().strip() + '"',
        '-DMKXPZ_XI_SONAME="' + run_command(sonametool, 'Xi', check: true).stdout().strip() + '"',
        '-DMKXPZ_XKBCOMMON_SONAME="' + run_command(sonametool, 'xkbcommon', check: true).stdout().strip() + '"',
        '-DMKXPZ_XRANDR_SONAME="' + run_command(sonametool, 'Xrandr', check: true).stdout().strip() + '"',
    ]
endif

if not sdl2.found() or not sdl2main.found()
    have_angle = get_option('angle').allowed()

    if have_angle
        require_system_xxhash = get_option('xxhash_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled)
        xxhash = dependency('libxxhash', allow_fallback: false, required: get_option('angle').auto() and require_system_xxhash.enabled() ? get_option('angle') : require_system_xxhash)
        if not xxhash.found()
            if not get_option('xxhash_system').enabled()
                subproject('xxhash', default_options: meson_base_options)
            else
                have_angle = false
            endif
        endif
    endif

    spirv_headers_source_dir = subproject('spirv-headers', default_options: meson_base_options).get_variable('spirv_headers_source_dir')

    if have_angle
        have_angle_vulkan = get_option('angle_vulkan').allowed()
        require_system_spirv_tools = get_option('spirv_tools_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled)
        spirv_tools = dependency('SPIRV-Tools', allow_fallback: false, required: get_option('angle').auto() and require_system_spirv_tools.enabled() ? get_option('angle') : require_system_spirv_tools)
        if not spirv_tools.found()
            if not require_system_spirv_tools.enabled()
                spirv_tools_options = cmake.subproject_options()
                spirv_tools_options.add_cmake_defines(cmake_base_options + {
                    'CMAKE_CXX_STANDARD': 17,
                    'CMAKE_C_FLAGS': run_command(quotetool, '-I' + spirv_headers_source_dir / 'include', check: true).stdout().strip(),
                    'CMAKE_CXX_FLAGS': run_command(quotetool, '-I' + spirv_headers_source_dir / 'include', check: true).stdout().strip(),
                    'SPIRV-Headers_SOURCE_DIR': spirv_headers_source_dir,
                    'BUILD_SHARED_LIBS': false,
                    'BUILD_TESTING': false,
                    'SKIP_SPIRV_TOOLS_INSTALL': true,
                    'SPIRV-TOOLS_USE_MIMALLOC': false,
                })
                foreach opt, val : cmake_override_options
                    spirv_tools_options.set_override_option(opt, val)
                endforeach
                meson.override_dependency('SPIRV-Tools', cmake.subproject('spirv-tools', options: spirv_tools_options).dependency('SPIRV-Tools-static'), static: true)
            else
                if get_option('angle_vulkan').enabled()
                    error('SPIRV-Tools is required for ANGLE Vulkan backend')
                endif
                have_angle = false
                have_angle_vulkan = false
            endif
        endif
    endif

    if have_angle and have_angle_vulkan
        require_system_volk = get_option('volk_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled)
        volk = dependency('volk', allow_fallback: false, required: get_option('angle').auto() and require_system_spirv_tools.enabled() ? get_option('angle') : require_system_spirv_tools)
        if not volk.found()
            if not require_system_volk.enabled()
                global_dependencies += subproject('volk', default_options: meson_base_options).get_variable('volk')
            else
                if get_option('angle_vulkan').enabled()
                    error('volk is required for ANGLE Vulkan backend')
                endif
                have_angle = false
                have_angle_vulkan = false
            endif
        else
            global_dependencies += volk
        endif
    endif

    if have_angle
        if get_option('angle_direct3d11').disabled()
            angle_direct3d11 = 'disabled'
        elif get_option('angle_direct3d11').enabled()
            angle_direct3d11 = 'enabled'
        else
            angle_direct3d11 = 'auto'
        endif
        if get_option('angle_metal').disabled()
            angle_metal = 'disabled'
        elif get_option('angle_metal').enabled()
            angle_metal = 'enabled'
        else
            angle_metal = 'auto'
        endif
        if get_option('angle_null').disabled()
            angle_null = 'disabled'
        elif get_option('angle_null').enabled()
            angle_null = 'enabled'
        else
            angle_null = 'auto'
        endif
        if get_option('angle_vulkan').disabled()
            angle_vulkan = 'disabled'
        elif get_option('angle_vulkan').enabled()
            angle_vulkan = 'enabled'
        else
            angle_vulkan = 'auto'
        endif
        angle = subproject('angle', default_options: meson_base_options + {
            'direct3d11': angle_direct3d11,
            'metal': angle_metal,
            'null': angle_null,
            'opengl': 'disabled',
            'vulkan': have_angle_vulkan ? angle_vulkan : 'disabled',
        })
        have_angle_direct3d11 = angle.get_variable('have_direct3d11')
        have_angle_metal = angle.get_variable('have_metal')
        have_angle_null = angle.get_variable('have_null')
        have_angle_vulkan = angle.get_variable('have_vulkan')
        global_args += '-DMKXPZ_HAVE_ANGLE'
        if have_angle_direct3d11
            global_args += '-DMKXPZ_HAVE_ANGLE_DIRECT3D11'
        endif
        if have_angle_metal
            global_args += '-DMKXPZ_HAVE_ANGLE_METAL'
        endif
        if have_angle_null
            global_args += '-DMKXPZ_HAVE_ANGLE_NULL'
        endif
        if have_angle_vulkan
            gplv3_features += 'ANGLE Vulkan backend'
            global_args += '-DMKXPZ_HAVE_ANGLE_VULKAN'
        endif
    endif

    sdl2_options = meson_base_options + {
        'test': false,
        'run_test': false,
        'use_atomic': 'enabled',
        'use_audio': 'disabled',
        'use_cpuinfo': 'enabled',
        'use_events': 'enabled',
        'use_file': 'enabled',
        'use_filesystem': 'enabled',
        'use_joystick': 'enabled',
        'use_haptic': 'disabled',
        'use_hidapi': 'disabled',
        'use_loadso': 'enabled',
        'use_locale': 'disabled',
        'use_render': 'enabled',
        'use_power': 'enabled',
        'use_sensor': 'disabled',
        'use_threads': 'enabled',
        'use_timers': 'enabled',
        'use_video': 'enabled',
        'use_video_kmsdrm': is_unix ? 'enabled' : 'disabled',
        'use_video_x11': is_unix ? 'enabled' : 'disabled',
        'use_video_wayland': is_unix ? 'enabled' : 'disabled',
        'use_video_wayland_libdecor': is_unix ? 'enabled' : 'disabled',
        'use_video_opengl': 'enabled',
        'use_video_openglesv2': 'enabled',
        'use_video_vulkan': 'disabled',
        'use_video_offscreen': 'disabled',
        'prefer_dlopen': is_unix,
        'system_iconv': 'enabled',
        'angle': have_angle ? 'enabled' : 'disabled',
    }
    sdl2main = declare_dependency(dependencies: [
        subproject('sdl2', default_options: sdl2_options).get_variable('sdl2main_dep'),
        host_system in ['windows', 'cygwin'] ? meson.get_compiler('c').find_library('mingw32') : [],
    ])
    sdl2 = subproject('sdl2', default_options: sdl2_options).get_variable('sdl2_dep')
    sdl2_includes = ['-I' + meson.project_build_root() / 'subprojects' / ('SDL2-' + sdl2.version()) / 'include', '-I' + meson.project_source_root() / 'subprojects' / ('SDL2-' + sdl2.version()) / 'include']
else
    if get_option('angle').enabled()
        error('ANGLE support requires vendored SDL')
    endif
    have_angle = false
    sdl2_includes = ['-I' + sdl2.get_variable(pkgconfig: 'includedir'), '-I' + (sdl2.get_variable(pkgconfig: 'includedir') / 'SDL2')]
endif
global_dependencies += [sdl2main, sdl2]

have_alsa = false
have_jack = false
have_oss = false
have_pipewire = false
have_pulseaudio = false

openal = dependency('openal', allow_fallback: false, required: get_option('openal_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
if not openal.found()
    fmt = dependency('fmt', allow_fallback: false, required: get_option('fmt_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
    if not fmt.found()
        fmt_options = cmake.subproject_options()
        fmt_options.add_cmake_defines(cmake_base_options + {
            'LIBTYPE': 'STATIC',
            'FMT_DOC': false,
            'FMT_TEST': false,
            'FMT_FUZZ': false,
            'FMT_CUDA_TEST': false,
            'FMT_MODULE': false,
            'FMT_WERROR': false,
        })
        foreach opt, val : cmake_override_options
            fmt_options.set_override_option(opt, val)
        endforeach
        fmt = cmake.subproject('fmt', options: fmt_options).dependency('fmt')
        meson.override_dependency('fmt', fmt, static: true)
    endif

    if is_unix
        have_oss = meson.get_compiler('c').has_header_symbol('sys/soundcard.h', 'SOUND_VERSION', required: get_option('oss'))
        if have_oss
            meson.override_dependency('oss', declare_dependency(version: 'undefined'), static: true)
        else
            meson.override_dependency('oss', null_dep, static: true)
        endif
    endif

    if host_system == 'linux'
        require_system_alsa = get_option('alsa_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled)
        if get_option('alsa').disabled() or (get_option('alsa').auto() and require_system_alsa.enabled())
            require_system_alsa = get_option('alsa')
        endif
        alsa = dependency('alsa', allow_fallback: false, required: require_system_alsa)
        if not alsa.found() and not get_option('alsa').disabled() and not get_option('alsa_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled).enabled()
            alsa_options = meson_base_options
            alsa = subproject('alsa-lib', default_options: alsa_options).get_variable('alsa_lib')
        endif
        have_alsa = alsa.found()
        if not have_alsa
            meson.override_dependency('alsa', null_dep, static: true)
        endif
    endif

    if is_unix
        require_system_jack = get_option('jack_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled)
        if get_option('jack').disabled() or (get_option('jack').auto() and require_system_jack.enabled())
            require_system_jack = get_option('jack')
        endif
        jack = dependency('jack', allow_fallback: false, required: require_system_jack)
        if not jack.found() and not get_option('jack').disabled() and not get_option('jack_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled).enabled()
            jack_options = meson_base_options
            jack = subproject('jack1', default_options: jack_options).get_variable('jack1')
        endif
        have_jack = jack.found()
        if not have_jack
            meson.override_dependency('jack', null_dep, static: true)
        endif

        require_system_pulseaudio = get_option('pulseaudio_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled)
        if get_option('pulseaudio').disabled() or (get_option('pulseaudio').auto() and require_system_pulseaudio.enabled())
            require_system_pulseaudio = get_option('pulseaudio')
        endif
        pulseaudio = dependency('libpulse', allow_fallback: false, required: require_system_pulseaudio)
        if not pulseaudio.found() and not get_option('pulseaudio').disabled() and not get_option('pulseaudio_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled).enabled()
            pulseaudio_options = meson_base_options
            pulseaudio = subproject('pulseaudio', default_options: pulseaudio_options).get_variable('pulseaudio')
        endif
        have_pulseaudio = pulseaudio.found()
        if not have_pulseaudio
            meson.override_dependency('libpulse', null_dep, static: true)
        endif

        require_system_pipewire = get_option('pipewire_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled)
        if get_option('pipewire').disabled() or (get_option('pipewire').auto() and require_system_pipewire.enabled())
            require_system_pipewire = get_option('pipewire')
        endif
        pipewire = dependency('libpipewire-0.3', allow_fallback: false, required: require_system_pipewire)
        if not pipewire.found() and not get_option('pipewire').disabled() and not get_option('pipewire_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled).enabled()
            pipewire_options = meson_base_options
            pipewire = subproject('pipewire', default_options: pipewire_options).get_variable('pipewire')
        endif
        have_pipewire = pipewire.found()
        if not have_pipewire
            meson.override_dependency('libpipewire-0.3', null_dep, static: true)
        endif
    endif

    neon = 'disabled'
    if host_machine.cpu_family() == 'arm'
        if get_option('arm_neon').disabled()
            neon = 'disabled'
        elif get_option('arm_neon').enabled()
            neon = 'enabled'
        else
            neon = 'auto'
        endif
    endif
    if host_machine.cpu_family() == 'aarch64'
        if get_option('aarch64_neon').disabled()
            neon = 'disabled'
        elif get_option('aarch64_neon').enabled()
            neon = 'enabled'
        else
            neon = 'auto'
        endif
    endif
    sse = 'disabled'
    if host_machine.cpu_family() == 'x86'
        if get_option('x86_sse').disabled()
            sse = 'disabled'
        elif get_option('x86_sse').enabled()
            sse = 'enabled'
        else
            sse = 'auto'
        endif
    endif
    if host_machine.cpu_family() == 'x86_64'
        if get_option('x86_64_sse').disabled()
            sse = 'disabled'
        elif get_option('x86_64_sse').enabled()
            sse = 'enabled'
        else
            sse = 'auto'
        endif
    endif
    sse2 = 'disabled'
    if host_machine.cpu_family() == 'x86'
        if get_option('x86_sse2').disabled()
            sse2 = 'disabled'
        elif get_option('x86_sse2').enabled()
            sse2 = 'enabled'
        else
            sse2 = 'auto'
        endif
    endif
    if host_machine.cpu_family() == 'x86_64'
        if get_option('x86_64_sse2').disabled()
            sse2 = 'disabled'
        elif get_option('x86_64_sse2').enabled()
            sse2 = 'enabled'
        else
            sse2 = 'auto'
        endif
    endif
    sse3 = 'disabled'
    if host_machine.cpu_family() == 'x86'
        if get_option('x86_sse3').disabled()
            sse3 = 'disabled'
        elif get_option('x86_sse3').enabled()
            sse3 = 'enabled'
        else
            sse3 = 'auto'
        endif
    endif
    if host_machine.cpu_family() == 'x86_64'
        if get_option('x86_64_sse3').disabled()
            sse3 = 'disabled'
        elif get_option('x86_64_sse3').enabled()
            sse3 = 'enabled'
        else
            sse3 = 'auto'
        endif
    endif
    sse41 = 'disabled'
    if host_machine.cpu_family() == 'x86'
        if get_option('x86_sse41').disabled()
            sse41 = 'disabled'
        elif get_option('x86_sse41').enabled()
            sse41 = 'enabled'
        else
            sse41 = 'auto'
        endif
    endif
    if host_machine.cpu_family() == 'x86_64'
        if get_option('x86_64_sse41').disabled()
            sse41 = 'disabled'
        elif get_option('x86_64_sse41').enabled()
            sse41 = 'enabled'
        else
            sse41 = 'auto'
        endif
    endif
    openal_options = meson_base_options + {
        'rtkit': 'disabled',
        'sdl2': 'disabled',
        'utils': false,
        'neon': neon,
        'sse': sse,
        'sse2': sse2,
        'sse3': sse3,
        'sse41': sse41,
    }
    openal = subproject('openal-soft', default_options: openal_options).get_variable('openal_dep')
endif
global_dependencies += openal

ogg = dependency('ogg', allow_fallback: false, method: 'pkg-config', required: get_option('ogg_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
if not ogg.found()
    ogg_options = cmake.subproject_options()
    ogg_options.add_cmake_defines(cmake_base_options + {
        'BUILD_SHARED_LIBS': false,
        'BUILD_TESTING': false,
        'BUILD_FRAMEWORK': false,
    })
    foreach opt, val : cmake_override_options
        ogg_options.set_override_option(opt, val)
    endforeach
    ogg = cmake.subproject('ogg', options: ogg_options).dependency('ogg')
    ogg_includes = ['-I' + meson.project_source_root() / 'subprojects' / ('libogg-' + ogg.version()) / 'include', '-I' + meson.project_build_root() / 'subprojects' / ('libogg-' + ogg.version()) / '__CMake_build' / 'include']
    meson.override_dependency('ogg', ogg, static: true)
else
    ogg_includes = ['-I' + ogg.get_variable(pkgconfig: 'includedir')]
endif
global_dependencies += ogg

vorbis = dependency('vorbis', allow_fallback: false, required: get_option('vorbis_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
if not vorbis.found()
    vorbis_internal_cflags = []
    vorbis_external_cflags = []
    if ogg.type_name() == 'internal'
        vorbis_internal_cflags += ogg_includes
    else
        vorbis_external_cflags += ogg_includes
    endif
    vorbis_options = cmake.subproject_options()
    vorbis_options.add_cmake_defines(cmake_base_options + {
        'CMAKE_C_FLAGS': run_command(quotetool, vorbis_internal_cflags + vorbis_external_cflags, check: true).stdout().strip(),
        'CMAKE_CXX_FLAGS': run_command(quotetool, vorbis_internal_cflags + vorbis_external_cflags, check: true).stdout().strip(),
        'BUILD_SHARED_LIBS': false,
        'BUILD_TESTING': false,
        'BUILD_FRAMEWORK': false,
    })
    foreach opt, val : cmake_override_options
        vorbis_options.set_override_option(opt, val)
    endforeach
    vorbis = declare_dependency(dependencies: [
        ogg,
        cmake.subproject('vorbis', options: vorbis_options).dependency('vorbis'),
    ])
    vorbisfile = declare_dependency(dependencies: [
        vorbis,
        cmake.subproject('vorbis', options: vorbis_options).dependency('vorbisfile'),
    ])
    meson.override_dependency('vorbis', vorbis, static: true)
    meson.override_dependency('vorbisfile', vorbisfile, static: true)
else
    vorbisfile = dependency('vorbisfile', allow_fallback: false, required: true)
endif
global_dependencies += [vorbis, vorbisfile]

sdl2_sound = dependency('sdl2_sound', allow_fallback: false, required: get_option('sdl_sound_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
if not sdl2_sound.found()
    sdl2_sound_internal_cflags = []
    sdl2_sound_external_cflags = []
    if sdl2.type_name() == 'internal'
        sdl2_sound_internal_cflags += sdl2_includes
    else
        sdl2_sound_external_cflags += sdl2_includes
    endif
    sdl2_sound_options = cmake.subproject_options()
    sdl2_sound_options.add_cmake_defines(cmake_base_options + {
        'CMAKE_C_FLAGS': run_command(quotetool, sdl2_sound_internal_cflags + sdl2_sound_external_cflags, check: true).stdout().strip(),
        'CMAKE_CXX_FLAGS': run_command(quotetool, sdl2_sound_internal_cflags + sdl2_sound_external_cflags, check: true).stdout().strip(),
        'SDLSOUND_BUILD_SHARED': false,
        'SDLSOUND_BUILD_TEST': false,
    })
    foreach opt, val : cmake_override_options
        sdl2_sound_options.set_override_option(opt, val)
    endforeach
    sdl2_sound = declare_dependency(dependencies: [
        sdl2,
        cmake.subproject('sdl2_sound', options: sdl2_sound_options).dependency('SDL2_sound-static'),
    ])
    meson.override_dependency('sdl2_sound', sdl2_sound, static: true)
endif
global_dependencies += sdl2_sound

freetype = dependency('freetype2', allow_fallback: false, method: 'pkg-config', required: get_option('freetype_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
if not freetype.found()
    freetype_options = meson_base_options + {
        'brotli': 'disabled',
        'bzip2': 'disabled',
        'harfbuzz': 'disabled',
        'mmap': 'disabled',
        'png': 'disabled',
        'tests': 'disabled',
        'zlib': 'disabled',
    }
    freetype = subproject('freetype', default_options: freetype_options).get_variable('freetype_dep')
    freetype_includes = ['-I' + meson.project_source_root() / 'subprojects' / ('freetype-VER-' + freetype.version().replace('.', '-')) / 'include']
else
    freetype_includes = ['-I' + (freetype.get_variable(pkgconfig: 'includedir') / 'freetype2')]
endif
global_dependencies += freetype

sdl2_ttf = dependency('sdl2_ttf', allow_fallback: false, required: get_option('sdl_ttf_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
if not sdl2_ttf.found()
    sdl2_ttf_internal_cflags = []
    sdl2_ttf_external_cflags = []
    if freetype.type_name() == 'internal'
        sdl2_ttf_internal_cflags += freetype_includes
    else
        sdl2_ttf_external_cflags += freetype_includes
    endif
    if sdl2.type_name() == 'internal'
        sdl2_ttf_internal_cflags += sdl2_includes
    else
        sdl2_ttf_external_cflags += sdl2_includes
    endif
    sdl2_ttf_options = cmake.subproject_options()
    sdl2_ttf_options.add_cmake_defines(cmake_base_options + {
        'CMAKE_C_FLAGS': run_command(quotetool, sdl2_ttf_internal_cflags + sdl2_ttf_external_cflags, check: true).stdout().strip(),
        'CMAKE_CXX_FLAGS': run_command(quotetool, sdl2_ttf_internal_cflags + sdl2_ttf_external_cflags, check: true).stdout().strip(),
        'BUILD_SHARED_LIBS': false,
        'BUILD_TESTING': false,
    })
    foreach opt, val : cmake_override_options
        sdl2_ttf_options.set_override_option(opt, val)
    endforeach
    sdl2_ttf = declare_dependency(dependencies: [
        freetype,
        sdl2,
        cmake.subproject('sdl2_ttf', options: sdl2_ttf_options).dependency('SDL2_ttf'),
    ])
    meson.override_dependency('sdl2_ttf', sdl2_ttf, static: true)
endif
global_dependencies += sdl2_ttf

if sdl2_ttf.type_name() == 'internal'
    global_args += '-DMKXPZ_TTF_FONT=_TTF_Font'
elif meson.get_compiler('c').compiles('''
    #include <SDL_ttf.h>
    int main(void) {
        TTF_Font *font = TTF_OpenFont("", 0);
        return font != NULL;
    }
''', name: 'TTF_Font test', dependencies: sdl2_ttf)
    global_args += '-DMKXPZ_TTF_FONT=TTF_Font'
elif meson.get_compiler('c').compiles('''
    #include <SDL_ttf.h>
    int main(void) {
        _TTF_Font *font = TTF_OpenFont("", 0);
        return font != NULL;
    }
''', name: '_TTF_Font test', dependencies: sdl2_ttf)
    global_args += '-DMKXPZ_TTF_FONT=_TTF_Font'
else
    error('failed to determine the name of the TTF_Font type')
endif

pixman = dependency('pixman-1', allow_fallback: false, required: get_option('pixman_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
if not pixman.found()
    loongson_mmi = 'disabled'
    if host_machine.cpu_family() == 'loongarch64'
        if get_option('loongarch64_mmi').disabled()
            loongson_mmi = 'disabled'
        elif get_option('loongarch64_mmi').enabled()
            loongson_mmi = 'enabled'
        else
            loongson_mmi = 'auto'
        endif
    endif
    mmx = 'disabled'
    if host_machine.cpu_family() == 'x86'
        if get_option('x86_mmx').disabled()
            mmx = 'disabled'
        elif get_option('x86_mmx').enabled()
            mmx = 'enabled'
        else
            mmx = 'auto'
        endif
    endif
    if host_machine.cpu_family() == 'x86_64'
        if get_option('x86_64_mmx').disabled()
            mmx = 'disabled'
        elif get_option('x86_64_mmx').enabled()
            mmx = 'enabled'
        else
            mmx = 'auto'
        endif
    endif
    sse2 = 'disabled'
    if host_machine.cpu_family() == 'x86'
        if get_option('x86_sse2').disabled()
            sse2 = 'disabled'
        elif get_option('x86_sse2').enabled()
            sse2 = 'enabled'
        else
            sse2 = 'auto'
        endif
    endif
    if host_machine.cpu_family() == 'x86_64'
        if get_option('x86_64_sse2').disabled()
            sse2 = 'disabled'
        elif get_option('x86_64_sse2').enabled()
            sse2 = 'enabled'
        else
            sse2 = 'auto'
        endif
    endif
    ssse3 = 'disabled'
    if host_machine.cpu_family() == 'x86'
        if get_option('x86_ssse3').disabled()
            ssse3 = 'disabled'
        elif get_option('x86_ssse3').enabled()
            ssse3 = 'enabled'
        else
            ssse3 = 'auto'
        endif
    endif
    if host_machine.cpu_family() == 'x86_64'
        if get_option('x86_64_ssse3').disabled()
            ssse3 = 'disabled'
        elif get_option('x86_64_ssse3').enabled()
            ssse3 = 'enabled'
        else
            ssse3 = 'auto'
        endif
    endif
    vmx = 'disabled'
    if host_machine.cpu_family() in ['ppc', 'ppc64']
        if get_option('ppc_altivec').disabled()
            vmx = 'disabled'
        elif get_option('ppc_altivec').enabled()
            vmx = 'enabled'
        else
            vmx = 'auto'
        endif
    endif
    arm_simd = 'disabled'
    if host_machine.cpu_family() == 'arm'
        if get_option('arm_armv6').disabled()
            arm_simd = 'disabled'
        elif get_option('arm_armv6').enabled()
            arm_simd = 'enabled'
        else
            arm_simd = 'auto'
        endif
    endif
    neon = 'disabled'
    if host_machine.cpu_family() == 'arm'
        if get_option('arm_neon').disabled()
            neon = 'disabled'
        elif get_option('arm_neon').enabled()
            neon = 'enabled'
        else
            neon = 'auto'
        endif
    endif
    a64_neon = 'disabled'
    if host_machine.cpu_family() == 'aarch64'
        if get_option('aarch64_neon').disabled()
            a64_neon = 'disabled'
        elif get_option('aarch64_neon').enabled()
            a64_neon = 'enabled'
        else
            a64_neon = 'auto'
        endif
    endif
    mips_dspr2 = 'disabled'
    if host_machine.cpu_family() == 'mips'
        if get_option('mips_dspr2').disabled()
            mips_dspr2 = 'disabled'
        elif get_option('mips_dspr2').enabled()
            mips_dspr2 = 'enabled'
        else
            mips_dspr2 = 'auto'
        endif
    endif
    rvv = 'disabled'
    if host_machine.cpu_family() in ['riscv32', 'riscv64']
        if get_option('riscv_rvv').disabled()
            rvv = 'disabled'
        elif get_option('riscv_rvv').enabled()
            rvv = 'enabled'
        else
            rvv = 'auto'
        endif
    endif
    pixman_options = meson_base_options + {
        'demos': 'disabled',
        'libpng': 'disabled',
        'tests': 'disabled',
        'tls': 'disabled',
        'loongson-mmi': loongson_mmi,
        'mmx': mmx,
        'sse2': sse2,
        'ssse3': ssse3,
        'vmx': vmx,
        'arm-simd': arm_simd,
        'neon': neon,
        'a64-neon': host_system == 'darwin' ? 'disabled' : a64_neon,
        'mips-dspr2': mips_dspr2,
        'rvv': rvv,
    }
    pixman = subproject('pixman', default_options: pixman_options).get_variable('idep_pixman')
endif
global_dependencies += pixman

uchardet = dependency('uchardet', allow_fallback: false, required: get_option('uchardet_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
if not uchardet.found()
    uchardet_options = cmake.subproject_options()
    uchardet_options.add_cmake_defines(cmake_base_options + {
        'BUILD_BINARY': false,
        'BUILD_SHARED_LIBS': false,
    })
    foreach opt, val : cmake_override_options
        uchardet_options.set_override_option(opt, val)
    endforeach
    uchardet = cmake.subproject('uchardet', options: uchardet_options).dependency('libuchardet')
    meson.override_dependency('uchardet', uchardet, static: true)
endif
global_dependencies += uchardet

theora = dependency('theora', allow_fallback: false, required: get_option('theora_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
if not theora.found()
    theora_options = meson_base_options
    theora = subproject('theora', default_options: theora_options).get_variable('theoradec')
endif
global_dependencies += theora

theoraplay_options = meson_base_options
theoraplay = subproject('theoraplay', default_options: theoraplay_options).get_variable('theoraplay')
if not subproject('theoraplay', default_options: theoraplay_options).get_variable('have_thread')
    global_args += '-DMKXPZ_THEORAPLAY_NO_THREAD'
endif
global_dependencies += theoraplay

sdl2_image = dependency('sdl2_image', allow_fallback: false, required: get_option('sdl_image_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
if not sdl2_image.found()
    sdl2_image_internal_cflags = []
    sdl2_image_external_cflags = []
    if sdl2.type_name() == 'internal'
        sdl2_image_internal_cflags += sdl2_includes
    else
        sdl2_image_external_cflags += sdl2_includes
    endif
    sdl2_image_options = cmake.subproject_options()
    sdl2_image_options.add_cmake_defines(cmake_base_options + {
        'CMAKE_C_FLAGS': run_command(quotetool, sdl2_image_internal_cflags + sdl2_image_external_cflags, check: true).stdout().strip(),
        'CMAKE_CXX_FLAGS': run_command(quotetool, sdl2_image_internal_cflags + sdl2_image_external_cflags, check: true).stdout().strip(),
        'BUILD_SHARED_LIBS': false,
        'SDL2IMAGE_JPG_SAVE': true,
        'SDL2IMAGE_PNG_SAVE': true,
        'SDL2IMAGE_PNG_SHARED': false,
        'SDL2IMAGE_JPG_SHARED': false,
        'SDL2IMAGE_JXL': true,
        'SDL2IMAGE_JXL_SHARED': false,
        'SDL2IMAGE_VENDORED': true,
        'SDL2IMAGE_SAMPLES': false,
        'SDL2IMAGE_BACKEND_WIC': false,
        'SDL2IMAGE_BACKEND_IMAGEIO': false,
        'JPEGXL_ENABLE_DEVTOOLS': false,
        'JPEGXL_ENABLE_EXAMPLES': false,
        'JPEGXL_ENABLE_FUZZERS': false,
        'JPEGXL_ENABLE_MANPAGES': false,
        'JPEGXL_ENABLE_TOOLS': false,
        'JPEGXL_STATIC': true,
    })
    foreach opt, val : cmake_override_options
        sdl2_image_options.set_override_option(opt, val)
    endforeach
    sdl2_image = declare_dependency(dependencies: [
        sdl2,
        cmake.subproject('sdl2_image', options: sdl2_image_options).dependency('SDL2_image'),
        cmake.subproject('sdl2_image', options: sdl2_image_options).dependency('brotlicommon-static'),
        cmake.subproject('sdl2_image', options: sdl2_image_options).dependency('brotlidec-static'),
        cmake.subproject('sdl2_image', options: sdl2_image_options).dependency('hwy'),
        cmake.subproject('sdl2_image', options: sdl2_image_options).dependency('jxl_dec-static'),
    ])
    meson.override_dependency('SDL2_image', sdl2_image, static: true)
endif
global_dependencies += sdl2_image

fluidsynth = dependency('fluidsynth', allow_fallback: false, required: get_option('fluidsynth_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
if not fluidsynth.found()
    gcem_options = meson_base_options
    gcem = subproject('gcem', default_options: gcem_options).get_variable('gcem')
    gcem_include_dir = subproject('gcem', default_options: gcem_options).get_variable('gcem_include_dir')
    gcem_includes = ['-I' + gcem_include_dir]

    fluidsynth_internal_cflags = []
    fluidsynth_external_cflags = []
    if gcem.type_name() == 'internal'
        fluidsynth_internal_cflags += gcem_includes
    else
        fluidsynth_external_cflags += gcem_includes
    endif
    fluidsynth_options = cmake.subproject_options()
    fluidsynth_options.add_cmake_defines(cmake_base_options + {
        'CMAKE_C_FLAGS': run_command(quotetool, fluidsynth_internal_cflags + fluidsynth_external_cflags, check: true).stdout().strip(),
        'CMAKE_CXX_FLAGS': run_command(quotetool, fluidsynth_internal_cflags + fluidsynth_external_cflags, check: true).stdout().strip(),
        'GCEM_INCLUDE_DIR': gcem_include_dir,
        'BUILD_SHARED_LIBS': false,
        'enable-alsa': false,
        'enable-aufile': false,
        'enable-dbus': false,
        'enable-ipv6 ': false,
        'enable-jack': false,
        'enable-ladspa': false,
        'enable-libinstpatch': false,
        'enable-libsndfile': false,
        'enable-midishare': false,
        'enable-opensles': false,
        'enable-oboe': false,
        'enable-network': false,
        'enable-oss': false,
        'enable-dsound': false,
        'enable-waveout': false,
        'enable-winmidi': false,
        'enable-sdl3': false,
        'enable-pulseaudio': false,
        'enable-pipewire': false,
        'enable-readline': false,
        'enable-threads': false,
        'enable-openmp': false,
        'enable-unicode': false,
        'enable-native-dls': false,
        'enable-systemd': false,
        'enable-coreaudio': false,
        'enable-coremidi': false,
        'enable-framework': false,
        'enable-dart': false,
        'enable-kai': false,
        'osal': 'embedded',
    })
    foreach opt, val : cmake_override_options
        fluidsynth_options.set_override_option(opt, val)
    endforeach
    fluidsynth = cmake.subproject('fluidsynth', options: fluidsynth_options).dependency('libfluidsynth')
    meson.override_dependency('fluidsynth', fluidsynth, static: true)
endif
global_dependencies += fluidsynth

global_args += '-DMKXPZ_FLUIDSYNTH_SONAME="' + run_command(sonametool, 'fluidsynth', check: true).stdout().strip() + '"'

ruby = dependency('ruby-' + get_option('mri_version'), allow_fallback: false, required: get_option('ruby_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
if not ruby.found()
    libffi = dependency('libffi', allow_fallback: false, method: 'pkg-config', required: get_option('libffi_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
    if not libffi.found()
        libffi_options = meson_base_options
        libffi = subproject('libffi', default_options: libffi_options).get_variable('libffi')
        libffi_includes = subproject('libffi', default_options: libffi_options).get_variable('libffi_includes')
        libffi_libs = subproject('libffi', default_options: libffi_options).get_variable('libffi_libs')
    else
        libffi_includes = ['-I' + libffi.get_variable(pkgconfig: 'includedir')]
        libffi_libs = ['-L' + libffi.get_variable(pkgconfig: 'libdir')]
    endif

    libyaml = dependency('yaml-0.1', allow_fallback: false, method: 'pkg-config', required: get_option('libyaml_system').disable_auto_if(default_system_disabled).enable_auto_if(default_system_enabled))
    if not libyaml.found()
        libyaml_options = cmake.subproject_options()
        libyaml_options.add_cmake_defines(cmake_base_options + {
            'BUILD_SHARED_LIBS': false,
        })
        foreach opt, val : cmake_override_options
            libyaml_options.set_override_option(opt, val)
        endforeach
        libyaml = cmake.subproject('libyaml', options: libyaml_options).dependency('yaml')
        libyaml_includes = ['-I' + meson.project_source_root() / 'subprojects' / 'libyaml' / 'include', '-I' + meson.project_build_root() / 'subprojects' / 'libyaml' / '__CMake_build' / 'include']
        libyaml_libs = ['-L' + meson.project_build_root() / 'subprojects' / 'libyaml']
        meson.override_dependency('yaml-0.1', libyaml, static: true)
    else
        libyaml_includes = ['-I' + libyaml.get_variable(pkgconfig: 'includedir')]
        libyaml_libs = ['-L' + libyaml.get_variable(pkgconfig: 'libdir')]
    endif

    ruby_internal_cflags = []
    ruby_external_cflags = []
    ruby_internal_ldflags = []
    ruby_external_ldflags = []
    if zlib.type_name() == 'internal'
        ruby_internal_cflags += zlib_includes
        ruby_internal_ldflags += zlib_libs
    else
        ruby_external_cflags += zlib_includes
        ruby_external_ldflags += zlib_libs
    endif
    if libyaml.type_name() == 'internal'
        ruby_internal_cflags += libyaml_includes
        ruby_internal_ldflags += libyaml_libs
    else
        ruby_external_cflags += libyaml_includes
        ruby_external_ldflags += libyaml_libs
    endif
    if libffi.type_name() == 'internal'
        ruby_internal_cflags += libffi_includes
        ruby_internal_ldflags += libffi_libs
    else
        ruby_external_cflags += libffi_includes
        ruby_external_ldflags += libffi_libs
    endif
    ruby_options = meson_base_options + {
        'cflags': run_command(quotetool, ruby_internal_cflags + ruby_external_cflags, check: true).stdout().strip(),
        'cppflags': run_command(quotetool, ruby_internal_cflags + ruby_external_cflags, check: true).stdout().strip(),
        'ldflags': run_command(quotetool, ruby_internal_ldflags + ruby_external_ldflags, check: true).stdout().strip()
    }
    ruby = subproject('ruby-' + get_option('mri_version'), default_options: ruby_options).get_variable('ruby')
    ruby_build_dir = subproject('ruby-' + get_option('mri_version'), default_options: ruby_options).get_variable('ruby_build_dir')
endif
global_dependencies += ruby

if host_system in ['windows', 'cygwin']
    global_link_args += ['-static-libgcc', '-static-libstdc++', '-Wl,-Bstatic', '-lpthread', '-Wl,-Bdynamic']
else
    global_link_args += ['-static-libstdc++']
endif

global_args += '-DMKXPZ_ALCDEVICE=ALCdevice'


global_include_dirs += include_directories('.',
    'audio',
    'crypto',
    'display', 'display/gl', 'display/libnsgif', 'display/libnsgif/utils',
    'etc',
    'filesystem', 'filesystem/ghc',
    'input',
    'net',
    'system',
    'util', 'util/sigslot', 'util/sigslot/adapter'
)

global_cpp_args += '-DSHARED_FLUID'

if get_option('cjk_fallback_font') == true
    global_cpp_args += '-DMKXPZ_CJK_FONT'
endif

main_source = files(
    'main.cpp',
    'config.cpp',
    'eventthread.cpp',
    'settingsmenu.cpp',
    'sharedstate.cpp',

    'audio/alstream.cpp',
    'audio/audio.cpp',
    'audio/audiostream.cpp',
    'audio/fluid-fun.cpp',
    'audio/midisource.cpp',
    'audio/sdlsoundsource.cpp',
    'audio/soundemitter.cpp',
    'audio/vorbissource.cpp',

    'crypto/rgssad.cpp',

    'display/autotiles.cpp',
    'display/autotilesvx.cpp',
    'display/bitmap.cpp',
    'display/font.cpp',
    'display/graphics.cpp',
    'display/plane.cpp',
    'display/sprite.cpp',
    'display/tilemap.cpp',
    'display/tilemapvx.cpp',
    'display/viewport.cpp',
    'display/window.cpp',
    'display/windowvx.cpp',

    'display/libnsgif/libnsgif.c',
    'display/libnsgif/lzw.c',

    'display/gl/gl-debug.cpp',
    'display/gl/gl-fun.cpp',
    'display/gl/gl-meta.cpp',
    'display/gl/glstate.cpp',
    'display/gl/scene.cpp',
    'display/gl/shader.cpp',
    'display/gl/texpool.cpp',
    'display/gl/tileatlas.cpp',
    'display/gl/tileatlasvx.cpp',
    'display/gl/tilequad.cpp',
    'display/gl/vertex.cpp',

    'util/iniconfig.cpp',
    'util/win-consoleutils.cpp',

    'etc/etc.cpp',
    'etc/table.cpp',

    'filesystem/filesystem.cpp',
    host_system == 'darwin' ? 'filesystem/filesystemImplApple.mm' : 'filesystem/filesystemImpl.cpp',

    'input/input.cpp',
    'input/keybindings.cpp',

    'net/LUrlParser.cpp',
    'net/net.cpp',

    host_system == 'darwin' ? 'system/systemImplApple.mm' : 'system/systemImpl.cpp',
)

global_sources += main_source
