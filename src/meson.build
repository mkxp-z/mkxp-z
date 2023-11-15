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
if build_static == true
    if host_system == 'windows'
        # '-static-libgcc', '-static-libstdc++' are here to avoid needing to ship a separate libgcc_s_seh-1.dll on Windows; it still works without those flags if you have the dll.
        global_link_args += ['-static-libgcc', '-static-libstdc++', '-Wl,-Bstatic', '-lgcc', '-lstdc++', '-lpthread', '-Wl,-Bdynamic']
    else
        global_link_args += ['-static-libgcc', '-static-libstdc++']
    endif
    global_args += '-DAL_LIBTYPE_STATIC'
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

global_dependencies += [openal, zlib, bz2, sdl2, sdl_sound, pixman, physfs, theora, vorbisfile, vorbis, ogg, sdl2_ttf, freetype, sdl2_image, png, iconv, uchardet]
if host_system == 'windows'
    global_dependencies += compilers['cpp'].find_library('wsock32')
endif

if get_option('shared_fluid') == true
    fluidsynth = dependency('fluidsynth', static: build_static)
    add_project_arguments('-DSHARED_FLUID', language: 'cpp')
    global_dependencies += fluidsynth
    if host_system == 'windows'
        global_dependencies += compilers['cpp'].find_library('dsound')
    endif
endif

if get_option('cjk_fallback_font') == true
    add_project_arguments('-DMKXPZ_CJK_FONT', language: 'cpp')
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
    'theoraplay/theoraplay.c',

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
    'filesystem/filesystemImpl.cpp',
    
    'input/input.cpp',
    'input/keybindings.cpp',

    'net/LUrlParser.cpp',
    'net/net.cpp',

    'system/systemImpl.cpp'
)

global_sources += main_source
