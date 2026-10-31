test_harness = [
    files('test-harness.py'),
    exe,
]

# Graphics tests
foreach test_name, test_options : {
    'benchmark-bitmap-dup': {'is_benchmark': true},
    'kgl2/blank': {},
    'kgl2/clear': {},
    'kgl2/clear-framebuffer': {},
    'kgl2/clone': {},
    'kgl2/compress-alpha': {},
    'kgl2/invert': {},
    'kgl2/light': {},
    'kgl2/shadow': {},
}
    timeout = test_options.get('timeout', 30)
    if test_options.get('is_benchmark', false)
        benchmark(test_name + '+opengl+sub-image-fix', python, args: [test_harness, test_name, 'renderer=2;subImageFix=true'], suite: ['graphics', 'graphics-opengl', 'graphics-opengl-sub-image-fix'], timeout: timeout)
        benchmark(test_name + '+opengl+no-sub-image-fix', python, args: [test_harness, test_name, 'renderer=2;subImageFix=false'], suite: ['graphics', 'graphics-opengl', 'graphics-opengl-no-sub-image-fix'], timeout: timeout)
        if have_angle_vulkan
            benchmark(test_name + '+vulkan', python, args: [test_harness, test_name, 'renderer=3;subImageFix=false'], suite: ['graphics', 'graphics-vulkan'], timeout: timeout)
        endif
        if have_angle_metal
            benchmark(test_name + '+metal', python, args: [test_harness, test_name, 'renderer=4;subImageFix=false'], suite: ['graphics', 'graphics-metal'], timeout: timeout)
        endif
        if have_angle_direct3d11
            benchmark(test_name + '+direct3d11', python, args: [test_harness, test_name, 'renderer=4;subImageFix=false'], suite: ['graphics', 'graphics-direct3d11'], timeout: timeout)
        endif
    else
        test(test_name + '+opengl+sub-image-fix', python, args: [test_harness, test_name, 'renderer=2;subImageFix=true'], suite: ['graphics', 'graphics-opengl', 'graphics-opengl-sub-image-fix'], timeout: timeout)
        test(test_name + '+opengl+no-sub-image-fix', python, args: [test_harness, test_name, 'renderer=2;subImageFix=false'], suite: ['graphics', 'graphics-opengl', 'graphics-opengl-no-sub-image-fix'], timeout: timeout)
        if have_angle_vulkan
            test(test_name + '+vulkan', python, args: [test_harness, test_name, 'renderer=3;subImageFix=false'], suite: ['graphics', 'graphics-vulkan'], timeout: timeout)
        endif
        if have_angle_metal
            test(test_name + '+metal', python, args: [test_harness, test_name, 'renderer=4;subImageFix=false'], suite: ['graphics', 'graphics-metal'], timeout: timeout)
        endif
        if have_angle_direct3d11
            test(test_name + '+direct3d11', python, args: [test_harness, test_name, 'renderer=4;subImageFix=false'], suite: ['graphics', 'graphics-direct3d11'], timeout: timeout)
        endif
    endif
endforeach

# Misc. tests
foreach test_name : [
    'http',
]
    test(test_name, python, args: [test_harness, test_name])
endforeach
