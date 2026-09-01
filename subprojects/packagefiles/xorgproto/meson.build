project('xorgproto', 'c', meson_version: '>=1.4.0')

if meson.get_compiler('c').compiles('''
    #include <sys/select.h>
    int main(void) {
        fd_set s;
        *s.fds_bits = 0;
        return 0;
    }
''', name: 'fds_bits test')
    use_fds_bits = 'fds_bits'
elif meson.get_compiler('c').compiles('''
    #include <sys/select.h>
    int main(void) {
        fd_set s;
        *s.__fds_bits = 0;
        return 0;
    }
''', name: '__fds_bits test')
    use_fds_bits = '__fds_bits'
else
    error('failed to determine how to access the contents of fd_set')
endif

subdir('include/X11')

xorgproto = declare_dependency(
    include_directories: [
        'include',
    ],
    sources: [
        xpoll,
    ],
)

meson.override_dependency('applewmproto', xorgproto, static: true)
meson.override_dependency('bigreqsproto', xorgproto, static: true)
meson.override_dependency('compositeproto', xorgproto, static: true)
meson.override_dependency('damageproto', xorgproto, static: true)
meson.override_dependency('dmxproto', xorgproto, static: true)
meson.override_dependency('dpmsproto', xorgproto, static: true)
meson.override_dependency('dri2proto', xorgproto, static: true)
meson.override_dependency('dri3proto', xorgproto, static: true)
meson.override_dependency('evieproto', xorgproto, static: true)
meson.override_dependency('fixesproto', xorgproto, static: true)
meson.override_dependency('fontcacheproto', xorgproto, static: true)
meson.override_dependency('fontsproto', xorgproto, static: true)
meson.override_dependency('glproto', xorgproto, static: true)
meson.override_dependency('inputproto', xorgproto, static: true)
meson.override_dependency('kbproto', xorgproto, static: true)
meson.override_dependency('lg3dproto', xorgproto, static: true)
meson.override_dependency('presentproto', xorgproto, static: true)
meson.override_dependency('printproto', xorgproto, static: true)
meson.override_dependency('randrproto', xorgproto, static: true)
meson.override_dependency('recordproto', xorgproto, static: true)
meson.override_dependency('renderproto', xorgproto, static: true)
meson.override_dependency('resourceproto', xorgproto, static: true)
meson.override_dependency('scrnsaverproto', xorgproto, static: true)
meson.override_dependency('trapproto', xorgproto, static: true)
meson.override_dependency('videoproto', xorgproto, static: true)
meson.override_dependency('windowswmproto', xorgproto, static: true)
meson.override_dependency('xcalibrateproto', xorgproto, static: true)
meson.override_dependency('xcmiscproto', xorgproto, static: true)
meson.override_dependency('xextproto', xorgproto, static: true)
meson.override_dependency('xf86bigfontproto', xorgproto, static: true)
meson.override_dependency('xf86dgaproto', xorgproto, static: true)
meson.override_dependency('xf86driproto', xorgproto, static: true)
meson.override_dependency('xf86miscproto', xorgproto, static: true)
meson.override_dependency('xf86rushproto', xorgproto, static: true)
meson.override_dependency('xf86vidmodeproto', xorgproto, static: true)
meson.override_dependency('xineramaproto', xorgproto, static: true)
meson.override_dependency('xproto', xorgproto, static: true)
meson.override_dependency('xproxymngproto', xorgproto, static: true)
meson.override_dependency('xwaylandproto', xorgproto, static: true)
