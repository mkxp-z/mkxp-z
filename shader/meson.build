
embedded_shaders = [
    'common.h',
    'transSimple.frag',
    'trans.frag',
    'hue.frag',
    'sprite.frag',
    'plane.frag',
    'gray.frag',
    'bitmapBlit.frag',
    'flatColor.frag',
    'simple.frag',
    'simpleColor.frag',
    'simpleAlpha.frag',
    'simpleAlphaUni.frag',
    'tilemap.frag',
    'flashMap.frag',
    'bicubic.frag',
    'lanczos3.frag',
    'minimal.vert',
    'simple.vert',
    'simpleColor.vert',
    'sprite.vert',
    'tilemap.vert',
    'tilemapvx.vert',
    'blur.frag',
    'blurH.vert',
    'blurV.vert',
    'simpleMatrix.vert',
    'kglInvert.frag',
    'kglCompressAlpha.frag',
    'kglSubtract.frag',
    'kglShadowH.frag',
    'kglShadowV.frag',
]

# xBRZ shader is GPLv3.
if get_option('enable-https') == true
    embedded_shaders += 'xbrz.frag'
endif

embedded_shaders_f = files(embedded_shaders)

embedded_shaders_dep_sources = []

count = 0
foreach file : embedded_shaders_f
    embedded_shaders_dep_sources += custom_target(embedded_shaders[count],
        input: file,
        output: '@0@.xxd'.format(embedded_shaders[count]),
        command: [
            embedtool,
            '@INPUT@',
            '@OUTPUT@',
            'mkxp_shader_@0@'.format(embedded_shaders[count].replace('.', '_')),
        ],
        )
    count += 1
endforeach

global_dependencies += declare_dependency(sources: embedded_shaders_dep_sources)
