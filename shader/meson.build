
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
have_extra_shaders = get_option('extra_shaders').allowed()
if have_extra_shaders
    gplv3_features += 'Extra shaders'
    global_args += '-DMKXPZ_HAVE_EXTRA_SHADERS'
    embedded_shaders += 'xbrz.frag'
endif

foreach shader : embedded_shaders
    global_dependencies += declare_dependency(sources: custom_target(
        '@0@.xxd'.format(shader),
        output: '@0@.xxd'.format(shader),
        command: [embedtool, files(shader), '@OUTPUT@', 'mkxp_shader_@0@'.format(shader.replace('.', '_'))],
    ))
endforeach
