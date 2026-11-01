embedded_scripts = [
    'win32_wrap.rb',
    'kgl2_wrap.rb',
]

foreach script : embedded_scripts
    global_dependencies += declare_dependency(sources: custom_target(
        '@0@.xxd'.format(script),
        output: '@0@.xxd'.format(script),
        command: [embedtool, files(script), '@OUTPUT@', 'mkxp_scripts_preload_@0@'.format(script.replace('.', '_'))],
    ))
endforeach
