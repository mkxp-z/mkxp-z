# Package the executable as a homebrew .nro: the icon, the title shown by
# hbmenu (a .nacp) and a RomFS with switch/romfs-root/ and win32_wrap.rb

romfs_files = files(
    '../romfs-root/readme.txt',
    '../romfs-root/preload/log_sync.rb',
    '../romfs-root/preload/win32api_stub.rb',
)

nacp = custom_target(
    'nacp',
    output: meson.project_name() + '.nacp',
    command: [
        find_program('nacptool'), '--create',
        meson.project_name(), meson.project_name() + ' (Switch port)', meson.project_version(),
        '@OUTPUT@',
    ],
)

custom_target(
    'nro',
    input: [mkxpz_exe, nacp, '../icon.jpg'],
    output: meson.project_name() + '.nro',
    command: [
        python, files('make_nro.py'), find_program('elf2nro'),
        '@INPUT0@', '@INPUT1@', '@INPUT2@', '@OUTPUT@', '@PRIVATE_DIR@',
        meson.project_source_root() / 'switch' / 'romfs-root',
        files('../../scripts/preload/win32_wrap.rb'),
    ],
    depend_files: romfs_files,
    build_by_default: true,
    install: true,
    install_dir: '.',
    install_tag: meson.project_name(),
)
