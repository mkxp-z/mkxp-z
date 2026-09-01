foreach name : xcb_proto_sources
    xcb_proto_generated += custom_target(
        name + '.h',
        output: name + '.h',
        command: [
            python,
            files('../../xcbgen.py')[0],
            python,
            files('../../src/c_client.py')[0],
            '@PRIVATE_DIR@',
            '@OUTDIR@',
            xcb_proto_root,
            name,
        ],
    )
endforeach
