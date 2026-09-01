project('xcb-proto', 'c', meson_version: '>=1.4.0')

xcb_proto_root = meson.project_source_root()

xcb_proto_sources = [
    'bigreq',
    'composite',
    'damage',
    'dbe',
    'dpms',
    'dri2',
    'dri3',
    'ge',
    'glx',
    'present',
    'randr',
    'record',
    'render',
    'res',
    'screensaver',
    'shape',
    'shm',
    'sync',
    'xc_misc',
    'xevie',
    'xf86dri',
    'xf86vidmode',
    'xfixes',
    'xinerama',
    'xinput',
    'xkb',
    'xprint',
    'xproto',
    'xselinux',
    'xtest',
    'xvmc',
    'xv',
]

# Verify that each source exists
foreach source : xcb_proto_sources
    files('src' / (source + '.xml'))
endforeach
