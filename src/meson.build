if get_option('cjk_fallback_font') == true
    global_args += '-DMKXPZ_CJK_FONT'
endif

if is_libretro
    cmake = import('cmake')

    global_args += '-DMPG123_NO_LARGENAME'
    global_args += '-DGL_GLES_PROTOTYPES=0'

    global_dependencies += subproject('libretro-common').get_variable('libretro_common')

    uchardet_options = cmake.subproject_options()
    uchardet_options.add_cmake_defines({
        'CMAKE_POLICY_VERSION_MINIMUM': '3.10',
        'CMAKE_ANDROID_API': android_api_level,
        'CMAKE_ANDROID_NDK': android_ndk_root,
        'CMAKE_OSX_SYSROOT': '/nonexistent',
        'CMAKE_C_FLAGS': ' '.join(global_args),
        'CMAKE_CXX_FLAGS': ' '.join(global_args + global_cpp_args),
        'CMAKE_C_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_CXX_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'BUILD_BINARY': false,
        'BUILD_SHARED_LIBS': false,
    })
    global_dependencies += cmake.subproject('uchardet', options: uchardet_options).dependency('libuchardet')

    libiconv_options = {
        'cflags': global_args,
        'cppflags': global_args,
        'default_library': 'static',
        'b_staticpic': use_pic,
    }
    global_dependencies += subproject('libiconv', default_options: libiconv_options).get_variable('libiconv')

    libidn_options = {
        'cflags': global_args,
        'cppflags': global_args,
        'default_library': 'static',
        'b_staticpic': use_pic,
    }
    global_dependencies += subproject('libidn', default_options: libidn_options).get_variable('libidn')

    global_dependencies += [
        subproject('boost_asio').get_variable('boost_asio'),
        subproject('boost_container_hash').get_variable('boost_container_hash'),
        subproject('boost_optional').get_variable('boost_optional'),
        subproject('boost_preprocessor').get_variable('boost_preprocessor'),
        subproject('boost_type_traits').get_variable('boost_type_traits'),
    ]

    zlib_options = cmake.subproject_options()
    zlib_options.add_cmake_defines({
        'CMAKE_POLICY_VERSION_MINIMUM': '3.10',
        'CMAKE_ANDROID_API': android_api_level,
        'CMAKE_ANDROID_NDK': android_ndk_root,
        'CMAKE_OSX_SYSROOT': '/nonexistent',
        'CMAKE_C_FLAGS': ' '.join(global_args),
        'CMAKE_CXX_FLAGS': ' '.join(global_args + global_cpp_args),
        'CMAKE_C_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_CXX_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'BUILD_SHARED_LIBS': false,
        'ZLIB_BUILD_EXAMPLES': false,
    })
    global_dependencies += cmake.subproject(host_system == 'darwin' ? 'zlib-darwin' : 'zlib', options: zlib_options).dependency('zlibstatic')

    physfs_options = cmake.subproject_options()
    physfs_options.add_cmake_defines({
        'CMAKE_POLICY_VERSION_MINIMUM': '3.10',
        'CMAKE_ANDROID_API': android_api_level,
        'CMAKE_ANDROID_NDK': android_ndk_root,
        'CMAKE_OSX_SYSROOT': '/nonexistent',
        'CMAKE_C_FLAGS': ' '.join(global_args),
        'CMAKE_CXX_FLAGS': ' '.join(global_args + global_cpp_args),
        'CMAKE_C_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_CXX_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'PHYSFS_BUILD_STATIC': true,
        'PHYSFS_BUILD_SHARED': false,
        'PHYSFS_BUILD_TEST': false,
        'PHYSFS_BUILD_DOCS': false,
    })
    global_dependencies += cmake.subproject('physfs', options: physfs_options).dependency('physfs-static')

    openal_options = cmake.subproject_options()
    openal_options.add_cmake_defines({
        'CMAKE_POLICY_VERSION_MINIMUM': '3.10',
        'CMAKE_ANDROID_API': android_api_level,
        'CMAKE_ANDROID_NDK': android_ndk_root,
        'CMAKE_OSX_SYSROOT': '/nonexistent',
        'CMAKE_C_FLAGS': ' '.join(global_args),
        'CMAKE_CXX_FLAGS': ' '.join(global_args + global_cpp_args),
        'CMAKE_C_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_CXX_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'LIBTYPE': 'STATIC',
        'ALSOFT_DLOPEN': false,
        'ALSOFT_UTILS': false,
        'ALSOFT_NO_CONFIG_UTIL': true,
        'ALSOFT_EXAMPLES': false,
        'ALSOFT_UPDATE_BUILD_VERSION': false,
        'ALSOFT_EMBED_HRTF_DATA': false,
        'ALSOFT_RTKIT': false,
        'ALSOFT_BACKEND_PIPEWIRE': false,
        'ALSOFT_BACKEND_PULSEAUDIO': false,
        'ALSOFT_BACKEND_ALSA': false,
        'ALSOFT_BACKEND_OSS': false,
        'ALSOFT_BACKEND_SOLARIS': false,
        'ALSOFT_BACKEND_SNDIO': false,
        'ALSOFT_BACKEND_WINMM': false,
        'ALSOFT_BACKEND_DSOUND': false,
        'ALSOFT_BACKEND_WASAPI': false,
        'ALSOFT_BACKEND_OTHERIO': false,
        'ALSOFT_BACKEND_JACK': false,
        'ALSOFT_BACKEND_COREAUDIO': false,
        'ALSOFT_BACKEND_OBOE': false,
        'ALSOFT_BACKEND_OPENSL': false,
        'ALSOFT_BACKEND_PORTAUDIO': false,
        'ALSOFT_BACKEND_SDL3': false,
        'ALSOFT_BACKEND_SDL2': false,
        'ALSOFT_BACKEND_WAVE': false,
    })
    global_dependencies += cmake.subproject('openal-soft', options: openal_options).dependency('OpenAL')
    global_dependencies += cmake.subproject('openal-soft', options: openal_options).dependency('alsoft.fmt')

    fluidsynth_options = cmake.subproject_options()
    fluidsynth_options.add_cmake_defines({
        'CMAKE_POLICY_VERSION_MINIMUM': '3.10',
        'CMAKE_ANDROID_API': android_api_level,
        'CMAKE_ANDROID_NDK': android_ndk_root,
        'CMAKE_OSX_SYSROOT': '/nonexistent',
        'CMAKE_C_FLAGS': ' '.join(global_args),
        'CMAKE_CXX_FLAGS': ' '.join(global_args + global_cpp_args),
        'CMAKE_C_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_CXX_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
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
    global_dependencies += cmake.subproject('fluidsynth', options: fluidsynth_options).dependency('libfluidsynth')

    ogg_options = cmake.subproject_options()
    ogg_options.add_cmake_defines({
        'CMAKE_POLICY_VERSION_MINIMUM': '3.10',
        'CMAKE_ANDROID_API': android_api_level,
        'CMAKE_ANDROID_NDK': android_ndk_root,
        'CMAKE_OSX_SYSROOT': '/nonexistent',
        'CMAKE_C_FLAGS': ' '.join(global_args),
        'CMAKE_CXX_FLAGS': ' '.join(global_args + global_cpp_args),
        'CMAKE_C_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_CXX_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'BUILD_SHARED_LIBS': false,
        'BUILD_TESTING': false,
        'BUILD_FRAMEWORK': false,
    })
    global_dependencies += cmake.subproject('ogg', options: ogg_options).dependency('ogg')

    vorbis_options = cmake.subproject_options()
    vorbis_options.add_cmake_defines({
        'CMAKE_POLICY_VERSION_MINIMUM': '3.10',
        'CMAKE_ANDROID_API': android_api_level,
        'CMAKE_ANDROID_NDK': android_ndk_root,
        'CMAKE_OSX_SYSROOT': '/nonexistent',
        'CMAKE_C_FLAGS': ' '.join(global_args),
        'CMAKE_CXX_FLAGS': ' '.join(global_args + global_cpp_args),
        'CMAKE_C_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_CXX_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'BUILD_SHARED_LIBS': false,
        'BUILD_TESTING': false,
        'BUILD_FRAMEWORK': false,
    })
    global_dependencies += cmake.subproject('vorbis', options: vorbis_options).dependency('vorbis')
    global_dependencies += cmake.subproject('vorbis', options: vorbis_options).dependency('vorbisfile')

    flac_options = cmake.subproject_options()
    flac_options.add_cmake_defines({
        'CMAKE_POLICY_VERSION_MINIMUM': '3.10',
        'CMAKE_ANDROID_API': android_api_level,
        'CMAKE_ANDROID_NDK': android_ndk_root,
        'CMAKE_OSX_SYSROOT': '/nonexistent',
        'CMAKE_C_FLAGS': ' '.join(global_args),
        'CMAKE_CXX_FLAGS': ' '.join(global_args + global_cpp_args),
        'CMAKE_C_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_CXX_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'BUILD_SHARED_LIBS': false,
        'BUILD_CXXLIBS': false,
        'BUILD_PROGRAMS': false,
        'BUILD_EXAMPLES': false,
        'BUILD_TESTING': false,
        'BUILD_DOCS': false,
        'BUILD_UTILS': false,
        'ENABLE_MULTITHREADING': false,
        'INSTALL_MANPAGES': false,
        'WITH_OGG': false,
        'WITH_FORTIFY_SOURCE': false,
        'WITH_STACK_PROTECTOR': false,
        'WITH_AVX': false, # AVX is only used by the encoder (which we don't use so we don't lose performance by disabling it), and enabling it causes compilation errors when targeting PlayStation 4
    })
    global_dependencies += cmake.subproject('flac', options: flac_options).dependency('FLAC')

    opus_options = cmake.subproject_options()
    opus_options.add_cmake_defines({
        'CMAKE_POLICY_VERSION_MINIMUM': '3.10',
        'CMAKE_ANDROID_API': android_api_level,
        'CMAKE_ANDROID_NDK': android_ndk_root,
        'CMAKE_OSX_SYSROOT': '/nonexistent',
        'CMAKE_C_FLAGS': ' '.join(global_args),
        'CMAKE_CXX_FLAGS': ' '.join(global_args + global_cpp_args),
        'CMAKE_C_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_CXX_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'BUILD_SHARED_LIBS': false,
        'OPUS_BUILD_SHARED_LIBRARY': false,
        'OPUS_BUILD_TESTING': false,
        'OPUS_CUSTOM_MODES': false,
        'OPUS_BUILD_PROGRAMS': false,
        'OPUS_DISABLE_INTRINSICS': true,
        'OPUS_FLOAT_APPROX': false,
        'OPUS_BUILD_FRAMEWORK': false,
        'OPUS_STATIC_RUNTIME': false,
        'OPUS_FORTIFY_SOURCE': false,
        'OPUS_STACK_PROTECTOR': false,
    })
    global_dependencies += cmake.subproject('opus', options: opus_options).dependency('opus')

    mpg123_options = cmake.subproject_options()
    mpg123_options.add_cmake_defines({
        'CMAKE_POLICY_VERSION_MINIMUM': '3.10',
        'CMAKE_ANDROID_API': android_api_level,
        'CMAKE_ANDROID_NDK': android_ndk_root,
        'CMAKE_OSX_SYSROOT': '/nonexistent',
        'CMAKE_C_FLAGS': ' '.join(global_args + '-DNO_CATCHSIGNAL' + '-Wno-macro-redefined'),
        'CMAKE_CXX_FLAGS': ' '.join(global_args + global_cpp_args + '-DNO_CATCHSIGNAL' + '-Wno-macro-redefined'),
        'CMAKE_C_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_CXX_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'BUILD_SHARED_LIBS': false,
        'BUILD_PROGRAMS': false,
        'BUILD_LIBOUT123': false,
        'CHECK_MODULES': false,
        'FIFO': false,
    })
    global_dependencies += cmake.subproject('mpg123', options: mpg123_options).dependency('libmpg123')
    global_dependencies += cmake.subproject('mpg123', options: mpg123_options).dependency('compat')

    libsndfile_options = cmake.subproject_options()
    libsndfile_options.add_cmake_defines({
        'CMAKE_POLICY_VERSION_MINIMUM': '3.10',
        'CMAKE_ANDROID_API': android_api_level,
        'CMAKE_ANDROID_NDK': android_ndk_root,
        'CMAKE_OSX_SYSROOT': '/nonexistent',
        'CMAKE_C_FLAGS': ' '.join(global_args),
        'CMAKE_CXX_FLAGS': ' '.join(global_args + global_cpp_args),
        'CMAKE_C_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_CXX_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'BUILD_SHARED_LIBS': false,
        'BUILD_PROGRAMS': false,
        'BUILD_EXAMPLES': false,
        'BUILD_REGTEST': false,
        'BUILD_TESTING': false,
        'ENABLE_CPACK': false,
        'ENABLE_PACKAGE_CONFIG': false,
        'INSTALL_PKGCONFIG_MODULE': false,
        'ENABLE_EXTERNAL_LIBS': true,
        'ENABLE_MPEG': true,
        'ENABLE_EXPERIMENTAL': false,
    })
    global_dependencies += cmake.subproject('libsndfile', options: libsndfile_options).dependency('sndfile')

    pixman_region_options = cmake.subproject_options()
    pixman_region_options.add_cmake_defines({
        'CMAKE_POLICY_VERSION_MINIMUM': '3.10',
        'CMAKE_ANDROID_API': android_api_level,
        'CMAKE_ANDROID_NDK': android_ndk_root,
        'CMAKE_OSX_SYSROOT': '/nonexistent',
        'CMAKE_C_FLAGS': ' '.join(global_args),
        'CMAKE_CXX_FLAGS': ' '.join(global_args + global_cpp_args),
        'CMAKE_C_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_CXX_BYTE_ORDER': host_endian == 'big' ? 'BIG_ENDIAN' : 'LITTLE_ENDIAN',
        'CMAKE_POSITION_INDEPENDENT_CODE': use_pic,
        'CMAKE_BUILD_TYPE': 'None',
    })
    global_dependencies += cmake.subproject('pixman-region', options: pixman_region_options).dependency('pixman-region')

    global_dependencies += subproject('stb').get_variable('stb')
    global_sources += files('stb_image.c', 'stb_sprintf.c')

    global_dependencies += subproject('opengl-registry').get_variable('opengl_registry')

    freetype_options = {
        'cflags': global_args,
        'cppflags': global_args,
        'default_library': 'static',
        'b_staticpic': use_pic,
        'brotli': 'disabled',
        'bzip2': 'disabled',
        'harfbuzz': 'disabled',
        'mmap': 'disabled',
        'png': 'disabled',
        'tests': 'disabled',
        'zlib': 'disabled',
    }
    global_dependencies += subproject('freetype', default_options: freetype_options).get_variable('freetype_dep')

    theora_options = {
        'cflags': global_args,
        'cppflags': global_args,
        'default_library': 'static',
        'b_staticpic': use_pic,
    }
    global_dependencies += subproject('theora', default_options: theora_options).get_variable('theoradec')

    global_dependencies += subproject('priority-deque').get_variable('priority_deque')
else
    physfs = dependency('physfs', version: '>=2.1', static: build_static)
    openal = dependency('openal', static: build_static, method: 'pkg-config')
    theora = dependency('theora', static: build_static)
    vorbisfile = dependency('vorbisfile', static: build_static)
    vorbis = dependency('vorbis', static: build_static)
    ogg = dependency('ogg', static: build_static)
    sdl2 = dependency('SDL2', static: build_static)
    sdl_sound = compilers['cpp'].find_library('SDL2_sound')
    sdl2_ttf = dependency('SDL2_ttf', static: build_static)
    freetype = dependency('freetype2', static: build_static)
    pixman = dependency('pixman-1', static: build_static)
    png = dependency('libpng', static: build_static)
    zlib = dependency('zlib', static: build_static)
    uchardet = dependency('uchardet', static: build_static)

    # As no pkg-config file is generated for static sdl2_image, and pkg-config is
    # the default option for meson detecting dependencies, pkg-config will fail to
    # find sdl2_image.pc in the build's lib/pkgconfig folder and instead pull it
    # from the locally installed packages if it exists.
    # To work around this, we first check to see if cmake can find our sdl2_image
    # sub project and use that, then check using pkg-config as normal if we are not
    # building the sub project.
    # It looks like upstream SDL_image fixed this for SDL3, so we can hopefully
    # remove this workaround after eventually upgrading to SDL3.
    sdl2_image = dependency('SDL2_image', modules: ['SDL2_image::SDL2_image-static', 'SDL2_image::brotlidec-static', 'SDL2_image::brotlicommon-static', 'SDL2_image::hwy', 'SDL2_image::jxl_dec-static'], static: build_static, method: 'cmake', required: false)
    if sdl2_image.found() == false
        sdl2_image = dependency('SDL2_image', modules: ['SDL2_image::SDL2_image-static', 'SDL2_image::brotlidec-static', 'SDL2_image::brotlicommon-static', 'SDL2_image::hwy', 'SDL2_image::jxl_dec-static'], static: build_static)
    endif

    if host_system == 'windows'
        bz2 = dependency('bzip2', static: build_static)
        iconv = compilers['cpp'].find_library('iconv', static: build_static)
    else
        bz2 = compilers['cpp'].find_library('bz2')
        # FIXME: Specifically asking for static doesn't work if iconv isn't
        # installed in the system prefix somewhere
        iconv = compilers['cpp'].find_library('iconv')
        global_dependencies += compilers['cpp'].find_library('charset')
    endif

    # If OpenSSL is present, you get HTTPS support
    if get_option('enable-https') == true
        openssl = dependency('openssl', required: false, static: build_static)
        if openssl.found() == true
            global_dependencies += openssl
            global_args += '-DMKXPZ_SSL'
            if host_system == 'windows'
                global_link_args += '-lcrypt32'
            endif
        else
            warning('Could not locate OpenSSL. HTTPS will be disabled.')
        endif
    endif

    # Windows needs to be treated like a special needs child here
    explicit_libs = ''
    if host_system == 'windows'
        # Newer versions of Ruby will refuse to link without these
        explicit_libs += 'libmsvcrt;libgcc;libmingwex;libgmp;'
    endif

    foreach l : explicit_libs.split(';')
        if l != ''
            global_link_args += '-l:' + l + '.a'
        endif
    endforeach

    alcdev_struct = 'ALCdevice_struct'
    if openal.type_name() == 'pkgconfig'
        if openal.version().version_compare('>=1.20.1')
            alcdev_struct = 'ALCdevice'
        endif
    endif

    global_args += '-DMKXPZ_ALCDEVICE=' + alcdev_struct

    global_dependencies += [openal, zlib, bz2, sdl2, sdl_sound, pixman, physfs, theora, vorbisfile, vorbis, ogg, sdl2_ttf, freetype, sdl2_image, png, iconv, uchardet]
    if host_system == 'windows'
        global_dependencies += compilers['cpp'].find_library('wsock32')
    endif

    if get_option('shared_fluid') == true
        fluidsynth = dependency('fluidsynth', static: build_static)
        global_args += '-DSHARED_FLUID'
        global_dependencies += fluidsynth
        if host_system == 'windows'
            global_dependencies += compilers['cpp'].find_library('dsound')
        endif
    endif
endif

theoraplay_options = {
    'cflags': global_args,
    'cppflags': global_args,
    'default_library': 'static',
    'b_staticpic': use_pic,
    'have_thread': have_any_thread,
}
global_dependencies += subproject('theoraplay', default_options: theoraplay_options).get_variable('theoraplay')

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

main_source = files(
    is_libretro ? 'core.cpp' : 'main.cpp',
    'config.cpp',
    is_libretro ? [] : 'eventthread.cpp',
    is_libretro ? [] : 'settingsmenu.cpp',
    'sharedstate.cpp',

    'audio/alstream.cpp',
    'audio/audio.cpp',
    'audio/audiostream.cpp',
    'audio/fluid-fun.cpp',
    'audio/midisource.cpp',
    is_libretro ? 'audio/sndfilesource.cpp' : 'audio/sdlsoundsource.cpp',
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
    is_libretro ? [] : 'util/win-consoleutils.cpp',

    'etc/etc.cpp',
    'etc/table.cpp',

    'filesystem/filesystem.cpp',
    is_libretro ? [] : 'filesystem/filesystemImpl.cpp',

    is_libretro ? [
        'input/input-retro.cpp',
    ] : [
        'input/input.cpp',
        'input/keybindings.cpp',
    ],

    is_libretro ? [] : [
        'net/LUrlParser.cpp',
        'net/net.cpp',
    ],

    is_libretro ? [] : 'system/systemImpl.cpp',
)

global_sources += main_source
