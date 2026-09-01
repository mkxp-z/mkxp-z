xcb_proto_generated += custom_target(
    'xproto.h',
    output: 'xproto.h',
    command: [
        python,
        files('../xcbgen.py')[0],
        python,
        files('../src/c_client.py')[0],
        '@PRIVATE_DIR@',
        '@OUTDIR@',
        xcb_proto_root,
        'xproto',
    ],
)

subdir('xcb')
