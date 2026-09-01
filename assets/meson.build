embedded_assets = [
    'icon.png',
    'gamecontrollerdb.txt'
]

if get_option('cjk_fallback_font') == true
    embedded_assets += 'wqymicrohei.ttf'
else
    embedded_assets += 'liberation.ttf'
endif

foreach asset : embedded_assets
    global_dependencies += declare_dependency(sources: custom_target(
        '@0@.xxd'.format(asset),
        output: '@0@.xxd'.format(asset),
        command: [embedtool, files(asset), '@OUTPUT@', 'mkxp_assets_@0@'.format(asset.replace('.', '_'))],
    ))
endforeach
