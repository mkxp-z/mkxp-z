project('angle', 'cpp', version: '8020', meson_version: '>= 1.2.0')
if host_machine.system() == 'darwin'
    add_languages('objcpp', native: false)
endif
git_hash = '80760643c85e1dfe07d389ee5c9baa9198e3f870'

override_options = {'cpp_std': 'c++20'}

host_machine_is_macos = host_machine.system() == 'darwin' and meson.get_compiler('cpp').compute_int('!!TARGET_OS_OSX', prefix: '#include <TargetConditionals.h>', low: 0, high: 1) != 0
host_machine_is_ios = host_machine.system() == 'darwin' and not host_machine_is_macos
host_machine_is_windows = host_machine.system() in ['windows', 'cygwin']
host_machine_is_native_windows = host_machine_is_windows and meson.get_compiler('cpp').compute_int('!!WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)', prefix: '#include <winapifamily.h>', low: 0, high: 1) != 0
host_machine_is_uwp = host_machine_is_windows and not host_machine_is_native_windows

includes = [
    '.',
    'src',
    'src/common/base',
    'include',
]

args = [
    '-DEGL_EGL_PROTOTYPES',
    '-DEGL_EGLEXT_PROTOTYPES',
    '-DGL_GLES_PROTOTYPES',
    '-DGL_GLEXT_PROTOTYPES',
]

if not meson.get_compiler('cpp').compiles('''
    static_assert(0.49999997f < 0.5f);
    static_assert(0.49999997f + 0.5f == 1.0f);
    static_assert(0.49999999999999994 < 0.5);
    static_assert(0.49999999999999994 + 0.5 == 1.0);
''', name: 'optimized rounding test')
    args += '-DMKXPZ_ANGLE_NO_OPTIMIZED_ROUND'
endif

if get_option('default_library') == 'static'
    args += '-DANGLE_PLATFORM_EXPORT='
endif

zlib = dependency('zlib', required: true)
angle_compression_utils = declare_dependency(
    include_directories: [
        'third_party/zlib/google',
    ],
    dependencies: zlib,
    link_with: static_library(
        'angle_compression_utils_portable',
        dependencies: [
            zlib,
        ],
        sources: [
            'third_party/zlib/google/compression_utils_portable.cc',
        ],
        build_by_default: false,
    ),
)
dl = meson.get_compiler('cpp').find_library(host_machine_is_windows ? 'kernel32' : 'dl', required: true)
xxhash = dependency('libxxhash', required: true)

dxgi = host_machine_is_windows ? meson.get_compiler('cpp').find_library('dxgi', required: get_option('direct3d11')) : dependency('', required: false)
dxguid = host_machine_is_windows ? meson.get_compiler('cpp').find_library('dxguid', required: get_option('direct3d11')) : dependency('', required: false)
have_direct3d11 = dxgi.found() and dxguid.found()
if have_direct3d11
    args += '-DANGLE_ENABLE_D3D11'
else
    dxgi = dependency('', required: false)
    dxguid = dependency('', required: false)
endif

have_any_direct3d = have_direct3d11
if have_any_direct3d
    args += '-DANGLE_ENABLE_HLSL'
endif

have_metal = get_option('metal').allowed() and host_machine.system() == 'darwin'
if have_metal
    args += '-DANGLE_ENABLE_METAL'
endif

have_null = get_option('null').allowed()
if have_null
    args += '-DANGLE_ENABLE_NULL'
endif

have_opengl = host_machine_is_windows ? meson.get_compiler('cpp').find_library('dxgi', required: get_option('opengl')).found() : get_option('opengl').allowed()
if host_machine.system() == 'darwin'
    have_opengl = false
endif
if have_opengl
    args += [
        '-DANGLE_ENABLE_OPENGL',
        '-DANGLE_ENABLE_GL_DESKTOP_BACKEND',
        '-DANGLE_ENABLE_ESSL',
        '-DANGLE_ENABLE_GLSL',
    ]
endif

spirv_headers = dependency('SPIRV-Headers', required: get_option('vulkan'))
spirv_tools = dependency('SPIRV-Tools', required: get_option('vulkan'))
volk = dependency('volk', required: get_option('vulkan'))
vulkan_memory_allocator = get_option('vulkan').allowed() ? subproject('vulkan-memory-allocator', default_options: {'c_std': get_option('c_std'), 'cpp_std': get_option('cpp_std')}).get_variable('vulkan_memory_allocator') : dependency('', required: false)
have_vulkan = spirv_headers.found() and spirv_tools.found() and volk.found() and vulkan_memory_allocator.found()
if have_vulkan
    args += [
        '-DANGLE_ENABLE_VULKAN',
        '-DANGLE_SHARED_LIBVULKAN',
    ]
    if host_machine_is_macos
        args += [
            '-DVK_USE_PLATFORM_METAL_EXT',
            '-DVK_USE_PLATFORM_MACOS_MVK',
        ]
    endif
    if host_machine_is_windows
        args += '-DVK_USE_PLATFORM_WIN32_KHR'
    endif
else
    spirv_headers = dependency('', required: false)
    spirv_tools = dependency('', required: false)
    volk = dependency('', required: false)
    vulkan_memory_allocator = dependency('', required: false)
endif

glx = have_opengl and not host_machine_is_windows and host_machine.system() not in ['darwin', 'android'] ? dependency('glx', required: get_option('x11')).partial_dependency(compile_args: true, includes: true, sources: true) : dependency('', required: false)
x11 = have_opengl and not host_machine_is_windows and host_machine.system() not in ['darwin', 'android'] ? dependency('x11', required: get_option('x11')).partial_dependency(compile_args: true, includes: true, sources: true) : dependency('', required: false)
xcb = have_vulkan and not host_machine_is_windows and host_machine.system() not in ['darwin', 'android'] ? dependency('x11-xcb', required: get_option('x11')).partial_dependency(compile_args: true, includes: true, sources: true) : dependency('', required: false)
have_x11 = get_option('x11').allowed() and (not have_opengl or (glx.found() and x11.found())) and (not have_vulkan or xcb.found())
if have_x11
    args += [
        '-DANGLE_USE_X11',
        '-DVK_USE_PLATFORM_XCB_KHR',
    ]
    if glx.found() and x11.found()
        x11 = declare_dependency(
            dependencies: x11,
            link_with: static_library(
                'mkxp-angle-x11-shim',
                dependencies: [
                    dl,
                    x11,
                ],
                cpp_args: [
                    '-DMKXP_ANGLE_SHIM_X11_SONAME="' + run_command(subproject('common').get_variable('sonametool'), 'X11', check: true).stdout().strip() + '"',
                ],
                sources: [
                    'mkxp-angle-x11-shim.cpp',
                ],
                build_by_default: false,
            ),
        )
        foreach func : [
            'XCloseDisplay',
            'XCreateColormap',
            'XCreateWindow',
            'XDestroyWindow',
            'XFlush',
            'XFree',
            'XFreeColormap',
            'XGetErrorText',
            'XGetGeometry',
            'XGetVisualInfo',
            'XGetWindowAttributes',
            'XMapWindow',
            'XOpenDisplay',
            'XResizeWindow',
            'XSetErrorHandler',
            'XSync',
        ]
            args += '-D' + func + '=mkxp_angle_x11_shim_' + func
        endforeach
    else
        glx = dependency('', required: false)
        x11 = dependency('', required: false)
    endif
    if xcb.found()
        xcb = declare_dependency(
            dependencies: xcb,
            link_with: static_library(
                'mkxp-angle-xcb-shim',
                dependencies: [
                    dl,
                    xcb,
                ],
                cpp_args: [
                    '-DMKXP_ANGLE_SHIM_XCB_SONAME="' + run_command(subproject('common').get_variable('sonametool'), 'xcb', check: true).stdout().strip() + '"',
                ],
                sources: [
                    'mkxp-angle-xcb-shim.cpp',
                ],
                build_by_default: false,
            ),
        )
        foreach func : [
            'xcb_connect',
            'xcb_connection_has_error',
            'xcb_depth_next',
            'xcb_depth_visuals_iterator',
            'xcb_disconnect',
            'xcb_get_geometry',
            'xcb_get_geometry_reply',
            'xcb_get_input_focus',
            'xcb_get_input_focus_reply',
            'xcb_get_setup',
            'xcb_get_window_attributes',
            'xcb_get_window_attributes_reply',
            'xcb_query_tree',
            'xcb_query_tree_reply',
            'xcb_screen_allowed_depths_iterator',
            'xcb_setup_roots_iterator',
            'xcb_visualtype_next',
        ]
            args += '-D' + func + '=mkxp_angle_xcb_shim_' + func
        endforeach
    endif
else
    glx = dependency('', required: false)
    x11 = dependency('', required: false)
    xcb = dependency('', required: false)
endif

wayland_client = not host_machine_is_windows and host_machine.system() not in ['darwin', 'android'] ? dependency('wayland-client', required: get_option('wayland')).partial_dependency(compile_args: true, includes: true, sources: true) : dependency('', required: false)
wayland_egl = not host_machine_is_windows and host_machine.system() not in ['darwin', 'android'] ? dependency('wayland-egl', required: get_option('wayland')).partial_dependency(compile_args: true, includes: true, sources: true) : dependency('', required: false)
have_wayland = wayland_client.found() and wayland_egl.found()
if have_wayland
    wayland_client = declare_dependency(
        dependencies: wayland_client,
        link_with: static_library(
            'mkxp-angle-wayland-client-shim',
            dependencies: [
                dl,
                wayland_client,
            ],
            cpp_args: [
                '-DMKXP_ANGLE_SHIM_WAYLAND_CLIENT_SONAME="' + run_command(subproject('common').get_variable('sonametool'), 'wayland-client', check: true).stdout().strip() + '"',
            ],
            sources: [
                'mkxp-angle-wayland-client-shim.cpp',
            ],
            build_by_default: false,
        ),
    )
    args += [
        '-DANGLE_USE_WAYLAND',
        '-DVK_USE_PLATFORM_WAYLAND_KHR',
    ]
    foreach func : [
        'wl_display_connect',
        'wl_display_disconnect',
        'wl_display_get_error',
        'wl_proxy_get_display',
    ]
        args += '-D' + func + '=mkxp_angle_wayland_client_shim_' + func
    endforeach
else
    wayland_client = dependency('', required: false)
    wayland_egl = dependency('', required: false)
endif

objcpp_args = args
objcpp_args += '-fno-objc-arc'
objcpp_args += '-fno-objc-weak'

export_args = [
    '-DANGLE_EXPORT=',
    '-DANGLE_STATIC=1',
    '-DANGLE_UTIL_EXPORT=',
    '-DEGLAPI=',
    '-DGL_APICALL=',
    '-DGL_API=',
]

egl_args = []
egl_shim_args = [
    '-DMKXP_ANGLE_SHIM_EGL_SONAME="' + run_command(subproject('common').get_variable('sonametool'), 'EGL', check: true).stdout().strip() + '"',
]
foreach func : [
    'eglBindAPI',
    'eglChooseConfig',
    'eglCreateContext',
    'eglCreatePbufferSurface',
    'eglCreateWindowSurface',
    'eglDestroyContext',
    'eglDestroySurface',
    'eglGetConfigAttrib',
    'eglGetError',
    'eglGetDisplay',
    'eglGetPlatformDisplay',
    'eglGetProcAddress',
    'eglInitialize',
    'eglMakeCurrent',
    'eglQueryAPI',
    'eglQueryString',
    'eglSwapBuffers',
    'eglSwapInterval',
    'eglTerminate',
    'eglWaitGL',
    'eglWaitNative',
]
    egl_args += '-D' + func + '=mkxp_angle_' + func
    egl_shim_args += '-D' + func + '=MKXP_EGL_SHIM_MANGLE(' + func + ')'
endforeach

angle_version_info = static_library(
    'angle_version_info',
    override_options: override_options,
    include_directories: includes,
    dependencies: [
        declare_dependency(sources: configure_file(configuration: {'VCS_TAG': git_hash.substring(0, 12)}, input: 'angle_commit.h.in', output: 'angle_commit.h')),
        declare_dependency(sources: configure_file(configuration: {'VCS_TAG': git_hash.substring(0, 12)}, input: 'ANGLEShaderProgramVersion.h.in', output: 'ANGLEShaderProgramVersion.h')),
    ],
    cpp_args: args + [
        '-DANGLE_REVISION=' + meson.project_version(),
    ],
    sources: [ # angle_version_info in BUILD.gn
        'src/common/angle_version_info.cpp',
    ],
    build_by_default: false,
)

angle_frame_capture_mock = static_library(
    'angle_frame_capture_mock',
    override_options: override_options,
    include_directories: includes,
    dependencies: [
        xxhash,
    ],
    cpp_args: args + [
        get_option('frame_capture_disabled') ? '-DANGLE_CAPTURE_ENABLED=0' : [],
    ],
    sources: [ # angle_frame_capture_mock in BUILD.gn
        'src/libANGLE/capture/FrameCapture_mock.cpp',
        'src/libANGLE/capture/serialize_mock.cpp',
    ],
    build_by_default: false,
)

angle_preprocessor = static_library(
    'angle_preprocessor',
    override_options: override_options,
    include_directories: includes,
    cpp_args: args,
    sources: [ # angle_preprocessor_sources in src/compiler.gni
        'src/compiler/preprocessor/DiagnosticsBase.cpp',
        'src/compiler/preprocessor/DirectiveHandlerBase.cpp',
        'src/compiler/preprocessor/DirectiveParser.cpp',
        'src/compiler/preprocessor/Input.cpp',
        'src/compiler/preprocessor/Lexer.cpp',
        'src/compiler/preprocessor/Macro.cpp',
        'src/compiler/preprocessor/MacroExpander.cpp',
        'src/compiler/preprocessor/Preprocessor.cpp',
        'src/compiler/preprocessor/Token.cpp',
        'src/compiler/preprocessor/preprocessor_lex_autogen.cpp',
        'src/compiler/preprocessor/preprocessor_tab_autogen.cpp',
    ],
    build_by_default: false,
)

angle_translator = static_library(
    'angle_translator',
    override_options: override_options,
    include_directories: includes,
    dependencies: [
        spirv_headers,
        spirv_tools,
        xxhash,
    ],
    cpp_args: args,
    sources: [ # angle_translator_sources in src/compiler.gni
        'src/compiler/translator/BaseTypes.cpp',
        'src/compiler/translator/BuiltInFunctionEmulator.cpp',
        'src/compiler/translator/CallDAG.cpp',
        'src/compiler/translator/CodeGen.cpp',
        'src/compiler/translator/CollectVariables.cpp',
        'src/compiler/translator/Compiler.cpp',
        'src/compiler/translator/ConstantUnion.cpp',
        'src/compiler/translator/Declarator.cpp',
        'src/compiler/translator/Diagnostics.cpp',
        'src/compiler/translator/DirectiveHandler.cpp',
        'src/compiler/translator/ExtensionBehavior.cpp',
        'src/compiler/translator/FlagStd140Structs.cpp',
        'src/compiler/translator/FunctionLookup.cpp',
        'src/compiler/translator/HashNames.cpp',
        'src/compiler/translator/ImmutableStringBuilder.cpp',
        'src/compiler/translator/InfoSink.cpp',
        'src/compiler/translator/Initialize.cpp',
        'src/compiler/translator/IntermNode.cpp',
        'src/compiler/translator/IntermRebuild.cpp',
        'src/compiler/translator/IsASTDepthBelowLimit.cpp',
        'src/compiler/translator/Name.cpp',
        'src/compiler/translator/Operator.cpp',
        'src/compiler/translator/OutputTree.cpp',
        'src/compiler/translator/ParseContext.cpp',
        'src/compiler/translator/PoolAlloc.cpp',
        'src/compiler/translator/QualifierTypes.cpp',
        'src/compiler/translator/ShaderLang.cpp',
        'src/compiler/translator/ShaderVars.cpp',
        'src/compiler/translator/SizeClipCullDistance.cpp',
        'src/compiler/translator/Symbol.cpp',
        'src/compiler/translator/SymbolTable.cpp',
        'src/compiler/translator/SymbolUniqueId.cpp',
        'src/compiler/translator/Types.cpp',
        'src/compiler/translator/ValidateAST.cpp',
        'src/compiler/translator/ValidateGlobalInitializer.cpp',
        'src/compiler/translator/ValidateVaryingLocations.cpp',
        'src/compiler/translator/VariablePacker.cpp',
        'src/compiler/translator/blocklayout.cpp',
        'src/compiler/translator/glslang_lex_autogen.cpp',
        'src/compiler/translator/glslang_tab_autogen.cpp',
        'src/compiler/translator/tree_ops/AddDefaultReturnStatements.cpp',
        'src/compiler/translator/tree_ops/ClampFragDepth.cpp',
        'src/compiler/translator/tree_ops/ClampIndirectIndices.cpp',
        'src/compiler/translator/tree_ops/ClampPointSize.cpp',
        'src/compiler/translator/tree_ops/DeclareAndInitBuiltinsForInstancedMultiview.cpp',
        'src/compiler/translator/tree_ops/DeclarePerVertexBlocks.cpp',
        'src/compiler/translator/tree_ops/DeferGlobalInitializers.cpp',
        'src/compiler/translator/tree_ops/EmulateGLFragColorBroadcast.cpp',
        'src/compiler/translator/tree_ops/EmulateMultiDrawShaderBuiltins.cpp',
        'src/compiler/translator/tree_ops/FoldExpressions.cpp',
        'src/compiler/translator/tree_ops/GatherDefaultUniforms.cpp',
        'src/compiler/translator/tree_ops/InitializeVariables.cpp',
        'src/compiler/translator/tree_ops/MonomorphizeUnsupportedFunctions.cpp',
        'src/compiler/translator/tree_ops/PreTransformTextureCubeGradDerivatives.cpp',
        'src/compiler/translator/tree_ops/PruneEmptyCases.cpp',
        'src/compiler/translator/tree_ops/PruneNoOps.cpp',
        'src/compiler/translator/tree_ops/RecordConstantPrecision.cpp',
        'src/compiler/translator/tree_ops/ReduceInterfaceBlocks.cpp',
        'src/compiler/translator/tree_ops/RemoveArrayLengthMethod.cpp',
        'src/compiler/translator/tree_ops/RemoveAtomicCounterBuiltins.cpp',
        'src/compiler/translator/tree_ops/RemoveDynamicIndexing.cpp',
        'src/compiler/translator/tree_ops/RemoveInactiveInterfaceVariables.cpp',
        'src/compiler/translator/tree_ops/RemoveInvariantDeclaration.cpp',
        'src/compiler/translator/tree_ops/RemoveUnreferencedVariables.cpp',
        'src/compiler/translator/tree_ops/RemoveUnusedFramebufferFetch.cpp',
        'src/compiler/translator/tree_ops/RewriteArrayOfArrayOfOpaqueUniforms.cpp',
        'src/compiler/translator/tree_ops/RewriteAtomicCounters.cpp',
        'src/compiler/translator/tree_ops/RewriteDfdy.cpp',
        'src/compiler/translator/tree_ops/RewritePixelLocalStorage.cpp',
        'src/compiler/translator/tree_ops/RewriteStructSamplers.cpp',
        'src/compiler/translator/tree_ops/RewriteTexelFetchOffset.cpp',
        'src/compiler/translator/tree_ops/ScalarizeVecAndMatConstructorArgs.cpp',
        'src/compiler/translator/tree_ops/SeparateDeclarations.cpp',
        'src/compiler/translator/tree_ops/SeparateStructFromUniformDeclarations.cpp',
        'src/compiler/translator/tree_ops/SimplifyLoopConditions.cpp',
        'src/compiler/translator/tree_ops/SplitSequenceOperator.cpp',
        'src/compiler/translator/tree_util/DriverUniform.cpp',
        'src/compiler/translator/tree_util/FindFunction.cpp',
        'src/compiler/translator/tree_util/FindMain.cpp',
        'src/compiler/translator/tree_util/FindPreciseNodes.cpp',
        'src/compiler/translator/tree_util/FindSymbolNode.cpp',
        'src/compiler/translator/tree_util/IntermNodePatternMatcher.cpp',
        'src/compiler/translator/tree_util/IntermNode_util.cpp',
        'src/compiler/translator/tree_util/IntermTraverse.cpp',
        'src/compiler/translator/tree_util/ReplaceArrayOfMatrixVarying.cpp',
        'src/compiler/translator/tree_util/ReplaceClipCullDistanceVariable.cpp',
        'src/compiler/translator/tree_util/ReplaceShadowingVariables.cpp',
        'src/compiler/translator/tree_util/ReplaceVariable.cpp',
        'src/compiler/translator/tree_util/RewriteSampleMaskVariable.cpp',
        'src/compiler/translator/tree_util/RunAtTheBeginningOfShader.cpp',
        'src/compiler/translator/tree_util/RunAtTheEndOfShader.cpp',
        'src/compiler/translator/util.cpp',
        true
            ? [ # angle_translator_essl_symbol_table_sources in src/compiler.gni
                'src/compiler/translator/ImmutableString_autogen.cpp',
                'src/compiler/translator/SymbolTable_autogen.cpp',
            ]
            : [],
        have_opengl
            ? [ # angle_translator_essl_sources in src/compiler.gni
                'src/compiler/translator/glsl/OutputESSL.cpp',
                'src/compiler/translator/glsl/TranslatorESSL.cpp',
            ]
            : [],
        have_opengl
            ? [ # angle_translator_glsl_sources in src/compiler.gni
                'src/compiler/translator/glsl/BuiltInFunctionEmulatorGLSL.cpp',
                'src/compiler/translator/glsl/ExtensionGLSL.cpp',
                'src/compiler/translator/glsl/TranslatorGLSL.cpp',
                'src/compiler/translator/glsl/VersionGLSL.cpp',
                'src/compiler/translator/tree_ops/glsl/ExpandFragmentOutputsToVec4.cpp',
                'src/compiler/translator/tree_ops/glsl/RewriteRepeatedAssignToSwizzled.cpp',
                'src/compiler/translator/tree_ops/glsl/UseInterfaceBlockFields.cpp',
                'src/compiler/translator/tree_ops/glsl/WrapStructConstructors.cpp',
            ]
            : [],
        have_opengl and host_machine.system() == 'darwin'
            ? [ # angle_translator_glsl_apple_sources in src/compiler.gni
                'src/compiler/translator/tree_ops/glsl/apple/AddAndTrueToLoopCondition.cpp',
                'src/compiler/translator/tree_ops/glsl/apple/RewriteRowMajorMatrices.cpp',
                'src/compiler/translator/tree_ops/glsl/apple/UnfoldShortCircuitAST.cpp',
            ]
            : [],
        have_any_direct3d
            ? [ # angle_translator_hlsl_sources in src/compiler.gni
                'src/compiler/translator/hlsl/ASTMetadataHLSL.cpp',
                'src/compiler/translator/hlsl/BuiltInFunctionEmulatorHLSL.cpp',
                'src/compiler/translator/hlsl/ImageFunctionHLSL.cpp',
                'src/compiler/translator/hlsl/OutputHLSL.cpp',
                'src/compiler/translator/hlsl/ResourcesHLSL.cpp',
                'src/compiler/translator/hlsl/StructureHLSL.cpp',
                'src/compiler/translator/hlsl/TextureFunctionHLSL.cpp',
                'src/compiler/translator/hlsl/TranslatorHLSL.cpp',
                'src/compiler/translator/hlsl/UtilsHLSL.cpp',
                'src/compiler/translator/hlsl/blocklayoutHLSL.cpp',
                'src/compiler/translator/hlsl/emulated_builtin_functions_hlsl_autogen.cpp',
                'src/compiler/translator/tree_ops/hlsl/ArrayReturnValueToOutParameter.cpp',
                'src/compiler/translator/tree_ops/hlsl/BreakVariableAliasingInInnerLoops.cpp',
                'src/compiler/translator/tree_ops/hlsl/ExpandIntegerPowExpressions.cpp',
                'src/compiler/translator/tree_ops/hlsl/RecordUniformBlocksWithLargeArrayMember.cpp',
                'src/compiler/translator/tree_ops/hlsl/RemoveSwitchFallThrough.cpp',
                'src/compiler/translator/tree_ops/hlsl/RewriteElseBlocks.cpp',
                'src/compiler/translator/tree_ops/hlsl/RewriteUnaryMinusOperatorInt.cpp',
                'src/compiler/translator/tree_ops/hlsl/SeparateArrayConstructorStatements.cpp',
                'src/compiler/translator/tree_ops/hlsl/SeparateArrayInitialization.cpp',
                'src/compiler/translator/tree_ops/hlsl/SeparateExpressionsReturningArrays.cpp',
                'src/compiler/translator/tree_ops/hlsl/UnfoldShortCircuitToIf.cpp',
                'src/compiler/translator/tree_ops/hlsl/WrapSwitchStatementsInBlocks.cpp',
            ]
            : [],
        have_vulkan
            ? [ # angle_translator_lib_spirv_sources in src/compiler.gni
                'src/compiler/translator/spirv/BuildSPIRV.cpp',
                'src/compiler/translator/spirv/BuiltinsWorkaround.cpp',
                'src/compiler/translator/spirv/OutputSPIRV.cpp',
                'src/compiler/translator/spirv/TranslatorSPIRV.cpp',
                'src/compiler/translator/tree_ops/spirv/ClampGLLayer.cpp',
                'src/compiler/translator/tree_ops/spirv/EmulateAdvancedBlendEquations.cpp',
                'src/compiler/translator/tree_ops/spirv/EmulateFragColorData.cpp',
                'src/compiler/translator/tree_ops/spirv/EmulateFramebufferFetch.cpp',
                'src/compiler/translator/tree_ops/spirv/EmulateYUVBuiltIns.cpp',
                'src/compiler/translator/tree_ops/spirv/FlagSamplersWithTexelFetch.cpp',
                'src/compiler/translator/tree_ops/spirv/ReswizzleYUVOps.cpp',
                'src/compiler/translator/tree_ops/spirv/RewriteInterpolateAtOffset.cpp',
                'src/compiler/translator/tree_ops/spirv/RewriteR32fImages.cpp',
                'src/compiler/translator/tree_ops/spirv/RewriteSamplerExternalTexelFetch.cpp',
            ]
            : [],
        have_metal
            ? [ # angle_translator_lib_msl_sources in src/compiler.cpp
                'src/compiler/translator/msl/AstHelpers.cpp',
                'src/compiler/translator/msl/ConstantNames.cpp',
                'src/compiler/translator/msl/DiscoverDependentFunctions.cpp',
                'src/compiler/translator/msl/DiscoverEnclosingFunctionTraverser.cpp',
                'src/compiler/translator/msl/DriverUniformMetal.cpp',
                'src/compiler/translator/msl/EmitMetal.cpp',
                'src/compiler/translator/msl/IdGen.cpp',
                'src/compiler/translator/msl/Layout.cpp',
                'src/compiler/translator/msl/MapFunctionsToDefinitions.cpp',
                'src/compiler/translator/msl/MapSymbols.cpp',
                'src/compiler/translator/msl/ModifyStruct.cpp',
                'src/compiler/translator/msl/Pipeline.cpp',
                'src/compiler/translator/msl/ProgramPrelude.cpp',
                'src/compiler/translator/msl/RewritePipelines.cpp',
                'src/compiler/translator/msl/SymbolEnv.cpp',
                'src/compiler/translator/msl/ToposortStructs.cpp',
                'src/compiler/translator/msl/TranslatorMSL.cpp',
                'src/compiler/translator/msl/UtilsMSL.cpp',
                'src/compiler/translator/tree_ops/msl/AddExplicitTypeCasts.cpp',
                'src/compiler/translator/tree_ops/msl/ConvertUnsupportedConstructorsToFunctionCalls.cpp',
                'src/compiler/translator/tree_ops/msl/EnsureLoopForwardProgress.cpp',
                'src/compiler/translator/tree_ops/msl/FixTypeConstructors.cpp',
                'src/compiler/translator/tree_ops/msl/GuardFragDepthWrite.cpp',
                'src/compiler/translator/tree_ops/msl/HoistConstants.cpp',
                'src/compiler/translator/tree_ops/msl/IntroduceVertexIndexID.cpp',
                'src/compiler/translator/tree_ops/msl/RescopeGlobalVariables.cpp',
                'src/compiler/translator/tree_ops/msl/RewriteCaseDeclarations.cpp',
                'src/compiler/translator/tree_ops/msl/RewriteInterpolants.cpp',
                'src/compiler/translator/tree_ops/msl/RewriteOutArgs.cpp',
                'src/compiler/translator/tree_ops/msl/RewriteUnaddressableReferences.cpp',
                'src/compiler/translator/tree_ops/msl/SeparateCompoundExpressions.cpp',
                'src/compiler/translator/tree_ops/msl/TransposeRowMajorMatrices.cpp',
                'src/compiler/translator/tree_ops/msl/WrapMain.cpp',
            ]
            : [],
        have_opengl
            ? [ # angle_translator_glsl_base_sources in src/compiler.cpp
                'src/compiler/translator/glsl/OutputGLSLBase.cpp',
            ]
            : [],
        have_opengl
            ? [ # angle_translator_glsl_and_vulkan_base_sources in src/compiler.cpp
                'src/compiler/translator/glsl/OutputGLSL.cpp',
            ]
            : [],
    ],
    build_by_default: false,
)

angle_common = static_library(
    'angle_common',
    override_options: override_options,
    include_directories: includes,
    dependencies: [
        host_machine.system() == 'darwin' ? dependency(
            'appleframeworks',
            modules: [
                'Metal',
            ],
            required: true,
        ) : [],
        host_machine_is_windows ? meson.get_compiler('cpp').find_library('synchronization', required: true) : [],
    ],
    cpp_args: args + (
        not meson.get_compiler('cpp').has_header_symbol('time.h', 'CLOCK_MONOTONIC_RAW')
            ? [
                '-DCLOCK_MONOTONIC_RAW=CLOCK_MONOTONIC',
            ]
            : []
    ),
    objcpp_args: objcpp_args,
    sources: [ # libangle_common_sources in src/libGLESv2.gni
        'src/common/Float16ToFloat32.cpp',
        'src/common/MemoryBuffer.cpp',
        'src/common/PackedEGLEnums_autogen.cpp',
        'src/common/PackedEnums.cpp',
        'src/common/PackedGLEnums_autogen.cpp',
        'src/common/PoolAlloc.cpp',
        'src/common/SimpleMutex.cpp',
        'src/common/WorkerThread.cpp',
        'src/common/aligned_memory.cpp',
        'src/common/android_util.cpp',
        'src/common/angleutils.cpp',
        'src/common/base/anglebase/sha1.cc',
        'src/common/debug.cpp',
        'src/common/entry_points_enum_autogen.cpp',
        'src/common/event_tracer.cpp',
        'src/common/mathutil.cpp',
        'src/common/matrix_utils.cpp',
        'src/common/platform_helpers.cpp',
        'src/common/string_utils.cpp',
        'src/common/system_utils.cpp',
        'src/common/tls.cpp',
        'src/common/uniform_type_info_autogen.cpp',
        'src/common/utilities.cpp',
        not host_machine_is_windows and host_machine.system() != 'darwin'
            ? [ # libangle_common_sources in src/libGLESv2.gni
                'src/common/system_utils_linux.cpp',
                'src/common/system_utils_posix.cpp',
            ]
            : [],
        not host_machine_is_windows and host_machine.system() not in ['android', 'darwin']
            ? [ # libangle_gpu_info_util_linux_sources in src/libGLESv2.gni
                'src/common/linux/window_system.cpp',
            ]
            : [],
        host_machine.system() == 'darwin'
            ? [ # libangle_common_sources in src/libGLESv2.gni
                'src/common/apple_platform_utils.mm',
                'src/common/system_utils_apple.cpp',
                'src/common/system_utils_posix.cpp',
            ]
            : [],
        host_machine_is_macos
            ? [ # libangle_common_sources in src/libGLESv2.gni
                'src/common/gl/cgl/FunctionsCGL.cpp',
                'src/common/system_utils_mac.cpp',
            ]
            : [],
        host_machine_is_ios
            ? [ # libangle_common_sources in src/libGLESv2.gni
                'src/common/system_utils_ios.cpp',
            ]
            : [],
        host_machine_is_uwp
            ? [ # libangle_common_sources in src/libGLESv2.gni
                'src/common/system_utils_win.cpp',
                'src/common/system_utils_winuwp.cpp',
            ]
            : [],
        host_machine_is_native_windows
            ? [ # libangle_common_sources in src/libGLESv2.gni
                'src/common/system_utils_win.cpp',
                'src/common/system_utils_win32.cpp',
            ]
            : [],
    ],
    build_by_default: false,
)

angle_common_shader_state = static_library(
    'angle_common_shader_state',
    override_options: override_options,
    include_directories: includes,
    cpp_args: args,
    sources: [ # libangle_common_shader_state_sources in src/libGLESv2.gni
        'src/common/CompiledShaderState.cpp',
    ],
    build_by_default: false,
)

angle_image_util = static_library(
    'angle_image_util',
    override_options: override_options,
    include_directories: includes,
    cpp_args: args,
    sources: [ # libangle_image_util_sources in src/libGLESv2.gni
        'src/image_util/copyimage.cpp',
        'src/image_util/imageformats.cpp',
        'src/image_util/loadimage.cpp',
        'src/image_util/loadimage_astc.cpp',
        'src/image_util/loadimage_etc.cpp',
        'src/image_util/loadimage_paletted.cpp',
        'src/image_util/storeimage_paletted.cpp',
        false
            ? [ # libangle_image_util_sources in src/libGLESv2.gni
                'src/image_util/AstcDecompressor.cpp',
            ]
            : [ # libangle_image_util_sources in src/libGLESv2.gni
                'src/image_util/AstcDecompressorNoOp.cpp',
            ],
    ],
    build_by_default: false,
)

angle_vulkan_icd = static_library(
    'angle_vulkan_icd',
    override_options: override_options,
    include_directories: includes,
    dependencies: [
        volk,
    ],
    link_with: [
        angle_common,
    ],
    cpp_args: args + [
        '-DANGLE_VK_MOCK_ICD_JSON=""',
    ],
    sources: [ # angle_vulkan_icd in src/common/vulkan/BUILD.gn
        'src/common/vulkan/vulkan_icd.cpp',
    ],
    build_by_default: false,
)

angle_libvulkan_loader = static_library(
    'angle_libvulkan_loader',
    override_options: override_options,
    include_directories: includes,
    link_with: [
        angle_common,
    ],
    cpp_args: args,
    sources: [ # angle_libvulkan_loader in src/common/vulkan/BUILD.gn
        'src/common/vulkan/libvulkan_loader.cpp',
    ],
    build_by_default: false,
)

angle_gpu_info_util = static_library(
    'angle_gpu_info_util',
    override_options: override_options,
    include_directories: includes,
    dependencies: [
        volk,
        host_machine.system() == 'darwin' ? dependency(
            'appleframeworks',
            modules: [
                'CoreFoundation',
                'CoreGraphics',
            ],
            required: true,
        ) : [],
        host_machine_is_macos ? dependency(
            'appleframeworks',
            modules: [
                'IOKit',
            ],
            required: true,
        ) : [],
    ],
    link_with: [
        have_vulkan ? angle_vulkan_icd : [],
        have_vulkan ? angle_libvulkan_loader : [],
    ],
    cpp_args: args,
    objcpp_args: objcpp_args,
    sources: [ # libangle_gpu_info_util_sources in src/libGLESv2.gni
        'src/gpu_info_util/SystemInfo.cpp',
        host_machine_is_windows
            ? [ # libangle_gpu_info_util_win_sources in src/libGLESv2.gni
                'src/gpu_info_util/SystemInfo_win.cpp',
            ]
            : [],
        host_machine.system() == 'android'
            ? [ # libangle_gpu_info_util_android_sources in src/libGLESv2.gni
                'src/gpu_info_util/SystemInfo_android.cpp',
            ]
            : [],
        not host_machine_is_windows and host_machine.system() not in ['android', 'darwin']
            ? [ # libangle_gpu_info_util_linux_sources in src/libGLESv2.gni
                'src/gpu_info_util/SystemInfo_linux.cpp',
            ]
            : [],
        host_machine_is_macos
            ? [ # libangle_gpu_info_util_mac_sources in src/libGLESv2.gni
                'src/gpu_info_util/SystemInfo_apple.mm',
                'src/gpu_info_util/SystemInfo_macos.mm',
            ]
            : [],
        host_machine_is_ios
            ? [ # libangle_gpu_info_util_ios_sources in src/libGLESv2.gni
                'src/gpu_info_util/SystemInfo_apple.mm',
                'src/gpu_info_util/SystemInfo_ios.mm',
            ]
            : [],
        have_vulkan
            ? [ # libangle_gpu_info_util_vulkan_sources in src/libGLESv2.gni
                'src/gpu_info_util/SystemInfo_vulkan.cpp',
            ]
            : [],
    ],
    build_by_default: false,
)

angle_dma_buf = host_machine_is_windows or host_machine.system() == 'darwin' ? [] : static_library(
    'angle_dma_buf',
    override_options: override_options,
    include_directories: includes,
    dependencies: [
        volk,
    ],
    cpp_args: args,
    sources: [ # angle_dma_buf in src/common/linux/BUILD.gn
        'src/common/linux/dma_buf_utils.cpp',
    ],
    build_by_default: false,
)

angle_d3d11_format_tables = static_library(
    'angle_d3d11_format_tables',
    override_options: override_options,
    include_directories: includes,
    dependencies: [
        xxhash,
    ],
    cpp_args: args,
    sources: [ # angle_d3d_format_tables in BUILD.gn
        'src/libANGLE/renderer/dxgi_format_map_autogen.cpp',
        'src/libANGLE/renderer/dxgi_support_table_autogen.cpp',
    ],
    build_by_default: false,
)

angle_d3d_shared = static_library(
    'angle_d3d_shared',
    override_options: override_options,
    include_directories: includes,
    dependencies: [
        xxhash,
    ],
    cpp_args: args,
    sources: [ # d3d_shared_sources in src/libANGLE/renderer/d3d/d3d_backend.gni
        'src/libANGLE/renderer/d3d/BufferD3D.cpp',
        'src/libANGLE/renderer/d3d/CompilerD3D.cpp',
        'src/libANGLE/renderer/d3d/DisplayD3D.cpp',
        'src/libANGLE/renderer/d3d/DynamicHLSL.cpp',
        'src/libANGLE/renderer/d3d/DynamicImage2DHLSL.cpp',
        'src/libANGLE/renderer/d3d/EGLImageD3D.cpp',
        'src/libANGLE/renderer/d3d/FramebufferD3D.cpp',
        'src/libANGLE/renderer/d3d/HLSLCompiler.cpp',
        'src/libANGLE/renderer/d3d/ImageD3D.cpp',
        'src/libANGLE/renderer/d3d/IndexBuffer.cpp',
        'src/libANGLE/renderer/d3d/IndexDataManager.cpp',
        'src/libANGLE/renderer/d3d/NativeWindowD3D.cpp',
        'src/libANGLE/renderer/d3d/ProgramD3D.cpp',
        'src/libANGLE/renderer/d3d/ProgramExecutableD3D.cpp',
        'src/libANGLE/renderer/d3d/RenderTargetD3D.cpp',
        'src/libANGLE/renderer/d3d/RenderbufferD3D.cpp',
        'src/libANGLE/renderer/d3d/RendererD3D.cpp',
        'src/libANGLE/renderer/d3d/ShaderD3D.cpp',
        'src/libANGLE/renderer/d3d/ShaderExecutableD3D.cpp',
        'src/libANGLE/renderer/d3d/SurfaceD3D.cpp',
        'src/libANGLE/renderer/d3d/SwapChainD3D.cpp',
        'src/libANGLE/renderer/d3d/TextureD3D.cpp',
        'src/libANGLE/renderer/d3d/VertexBuffer.cpp',
        'src/libANGLE/renderer/d3d/VertexDataManager.cpp',
        'src/libANGLE/renderer/d3d/driver_utils_d3d.cpp',
    ],
    build_by_default: false,
)

angle_d3d11_backend = static_library(
    'angle_d3d11_backend',
    override_options: override_options,
    include_directories: includes,
    dependencies: [
        dxgi,
        dxguid,
        xxhash,
    ],
    link_with: [
        angle_d3d_shared,
        angle_d3d11_format_tables,
    ],
    cpp_args: args,
    sources: [ # d3d11_backend_sources in src/libANGLE/renderer/d3d/d3d_backend.gni
        'src/libANGLE/renderer/d3d/d3d11/Blit11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/Buffer11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/Clear11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/Context11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/DebugAnnotator11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/Device11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/ExternalImageSiblingImpl11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/Fence11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/Framebuffer11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/Image11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/IndexBuffer11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/InputLayoutCache.cpp',
        'src/libANGLE/renderer/d3d/d3d11/PixelTransfer11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/Program11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/ProgramPipeline11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/Query11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/RenderStateCache.cpp',
        'src/libANGLE/renderer/d3d/d3d11/RenderTarget11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/Renderer11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/ResourceManager11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/ShaderExecutable11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/StateManager11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/StreamProducerD3DTexture.cpp',
        'src/libANGLE/renderer/d3d/d3d11/SwapChain11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/TextureStorage11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/TransformFeedback11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/Trim11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/VertexArray11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/VertexBuffer11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/formatutils11.cpp',
        'src/libANGLE/renderer/d3d/d3d11/renderer11_utils.cpp',
        'src/libANGLE/renderer/d3d/d3d11/texture_format_table.cpp',
        'src/libANGLE/renderer/d3d/d3d11/texture_format_table_autogen.cpp',
        host_machine_is_uwp
            ? [ # d3d11_backend_sources in src/libANGLE/renderer/d3d/d3d_backend.gni
                'src/libANGLE/renderer/d3d/d3d11/winrt/CoreWindowNativeWindow.cpp',
                'src/libANGLE/renderer/d3d/d3d11/winrt/InspectableNativeWindow.cpp',
                'src/libANGLE/renderer/d3d/d3d11/winrt/NativeWindow11WinRT.cpp',
                'src/libANGLE/renderer/d3d/d3d11/winrt/SwapChainPanelNativeWindow.cpp',
            ]
            : [ # d3d11_backend_sources in src/libANGLE/renderer/d3d/d3d_backend.gni
                'src/libANGLE/renderer/d3d/d3d11/win32/NativeWindow11Win32.cpp',
            ],
    ],
    build_by_default: false,
)

if host_machine.system() != 'darwin'
    angle_metal_backend = []
else
    angle_metal_backend = static_library(
        'angle_metal_backend',
        override_options: override_options,
        include_directories: includes,
        dependencies: [
            xxhash,
            dependency(
                'appleframeworks',
                modules: [
                    'IOSurface',
                    'Metal',
                    'QuartzCore',
                ],
                required: true,
            ),
            host_machine_is_macos ? dependency(
                'appleframeworks',
                modules: [
                    'Cocoa',
                ],
                required: true,
            ) : [],
        ],
        cpp_args: args,
        objcpp_args: objcpp_args,
        sources: [ # metal_backend_sources in src/libANGLE/renderer/metal/metal_backend.gni
            'src/libANGLE/renderer/metal/BufferMtl.mm',
            'src/libANGLE/renderer/metal/CompilerMtl.mm',
            'src/libANGLE/renderer/metal/ContextMtl.mm',
            'src/libANGLE/renderer/metal/DeviceMtl.mm',
            'src/libANGLE/renderer/metal/DisplayMtl.mm',
            'src/libANGLE/renderer/metal/FrameBufferMtl.mm',
            'src/libANGLE/renderer/metal/IOSurfaceSurfaceMtl.mm',
            'src/libANGLE/renderer/metal/ImageMtl.mm',
            'src/libANGLE/renderer/metal/ProgramExecutableMtl.mm',
            'src/libANGLE/renderer/metal/ProgramMtl.mm',
            'src/libANGLE/renderer/metal/ProvokingVertexHelper.mm',
            'src/libANGLE/renderer/metal/QueryMtl.mm',
            'src/libANGLE/renderer/metal/RenderBufferMtl.mm',
            'src/libANGLE/renderer/metal/RenderTargetMtl.mm',
            'src/libANGLE/renderer/metal/SamplerMtl.mm',
            'src/libANGLE/renderer/metal/ShaderMtl.mm',
            'src/libANGLE/renderer/metal/SurfaceMtl.mm',
            'src/libANGLE/renderer/metal/SyncMtl.mm',
            'src/libANGLE/renderer/metal/TextureMtl.mm',
            'src/libANGLE/renderer/metal/TransformFeedbackMtl.mm',
            'src/libANGLE/renderer/metal/VertexArrayMtl.mm',
            'src/libANGLE/renderer/metal/blocklayoutMetal.cpp',
            'src/libANGLE/renderer/metal/mtl_buffer_manager.mm',
            'src/libANGLE/renderer/metal/mtl_buffer_pool.mm',
            'src/libANGLE/renderer/metal/mtl_command_buffer.mm',
            'src/libANGLE/renderer/metal/mtl_common.mm',
            'src/libANGLE/renderer/metal/mtl_context_device.mm',
            'src/libANGLE/renderer/metal/mtl_format_table_autogen.mm',
            'src/libANGLE/renderer/metal/mtl_format_utils.mm',
            'src/libANGLE/renderer/metal/mtl_library_cache.mm',
            'src/libANGLE/renderer/metal/mtl_msl_utils.mm',
            'src/libANGLE/renderer/metal/mtl_occlusion_query_pool.mm',
            'src/libANGLE/renderer/metal/mtl_pipeline_cache.mm',
            'src/libANGLE/renderer/metal/mtl_render_utils.mm',
            'src/libANGLE/renderer/metal/mtl_resources.mm',
            'src/libANGLE/renderer/metal/mtl_state_cache.mm',
            'src/libANGLE/renderer/metal/mtl_utils.mm',
            'src/libANGLE/renderer/metal/renderermtl_utils.cpp',
        ],
        build_by_default: false,
    )
endif

angle_null_backend = static_library(
    'angle_null_backend',
    override_options: override_options,
    include_directories: includes,
    dependencies: [
        xxhash,
    ],
    cpp_args: args,
    sources: [ # null_backend_sources in src/libANGLE/renderer/null/null_backend.gni
        'src/libANGLE/renderer/null/BufferNULL.cpp',
        'src/libANGLE/renderer/null/CompilerNULL.cpp',
        'src/libANGLE/renderer/null/ContextNULL.cpp',
        'src/libANGLE/renderer/null/DeviceNULL.cpp',
        'src/libANGLE/renderer/null/DisplayNULL.cpp',
        'src/libANGLE/renderer/null/FenceNVNULL.cpp',
        'src/libANGLE/renderer/null/FramebufferNULL.cpp',
        'src/libANGLE/renderer/null/ImageNULL.cpp',
        'src/libANGLE/renderer/null/ProgramExecutableNULL.cpp',
        'src/libANGLE/renderer/null/ProgramNULL.cpp',
        'src/libANGLE/renderer/null/ProgramPipelineNULL.cpp',
        'src/libANGLE/renderer/null/QueryNULL.cpp',
        'src/libANGLE/renderer/null/RenderbufferNULL.cpp',
        'src/libANGLE/renderer/null/SamplerNULL.cpp',
        'src/libANGLE/renderer/null/ShaderNULL.cpp',
        'src/libANGLE/renderer/null/SurfaceNULL.cpp',
        'src/libANGLE/renderer/null/SyncNULL.cpp',
        'src/libANGLE/renderer/null/TextureNULL.cpp',
        'src/libANGLE/renderer/null/TransformFeedbackNULL.cpp',
        'src/libANGLE/renderer/null/VertexArrayNULL.cpp',
    ],
    build_by_default: false,
)

angle_gl_backend = static_library(
    'angle_gl_backend',
    override_options: override_options,
    include_directories: includes,
    dependencies: [
        glx,
        volk,
        x11,
        xxhash,
    ],
    link_with: [
        host_machine_is_windows ? angle_d3d11_format_tables : [],
        angle_dma_buf,
    ],
    cpp_args: args,
    sources: [ # gl_backend_sources in src/libANGLE/renderer/gl/gl_backend.gni
        'src/libANGLE/renderer/gl/BlitGL.cpp',
        'src/libANGLE/renderer/gl/BufferGL.cpp',
        'src/libANGLE/renderer/gl/ClearMultiviewGL.cpp',
        'src/libANGLE/renderer/gl/CompilerGL.cpp',
        'src/libANGLE/renderer/gl/ContextGL.cpp',
        'src/libANGLE/renderer/gl/DispatchTableGL_autogen.cpp',
        'src/libANGLE/renderer/gl/DisplayGL.cpp',
        'src/libANGLE/renderer/gl/FenceNVGL.cpp',
        'src/libANGLE/renderer/gl/FramebufferGL.cpp',
        'src/libANGLE/renderer/gl/FunctionsGL.cpp',
        'src/libANGLE/renderer/gl/ImageGL.cpp',
        'src/libANGLE/renderer/gl/MemoryObjectGL.cpp',
        'src/libANGLE/renderer/gl/ProgramExecutableGL.cpp',
        'src/libANGLE/renderer/gl/ProgramGL.cpp',
        'src/libANGLE/renderer/gl/ProgramPipelineGL.cpp',
        'src/libANGLE/renderer/gl/QueryGL.cpp',
        'src/libANGLE/renderer/gl/RenderbufferGL.cpp',
        'src/libANGLE/renderer/gl/RendererGL.cpp',
        'src/libANGLE/renderer/gl/SamplerGL.cpp',
        'src/libANGLE/renderer/gl/SemaphoreGL.cpp',
        'src/libANGLE/renderer/gl/ShaderGL.cpp',
        'src/libANGLE/renderer/gl/StateManagerGL.cpp',
        'src/libANGLE/renderer/gl/SurfaceGL.cpp',
        'src/libANGLE/renderer/gl/SyncGL.cpp',
        'src/libANGLE/renderer/gl/TextureGL.cpp',
        'src/libANGLE/renderer/gl/TransformFeedbackGL.cpp',
        'src/libANGLE/renderer/gl/VertexArrayGL.cpp',
        'src/libANGLE/renderer/gl/formatutilsgl.cpp',
        'src/libANGLE/renderer/gl/renderergl_utils.cpp',
        host_machine_is_windows
            ? [ # gl_backend_sources in src/libANGLE/renderer/gl/gl_backend.gni
                'src/libANGLE/renderer/gl/wgl/ContextWGL.cpp',
                'src/libANGLE/renderer/gl/wgl/D3DTextureSurfaceWGL.cpp',
                'src/libANGLE/renderer/gl/wgl/DXGISwapChainWindowSurfaceWGL.cpp',
                'src/libANGLE/renderer/gl/wgl/DisplayWGL.cpp',
                'src/libANGLE/renderer/gl/wgl/FunctionsWGL.cpp',
                'src/libANGLE/renderer/gl/wgl/PbufferSurfaceWGL.cpp',
                'src/libANGLE/renderer/gl/wgl/RendererWGL.cpp',
                'src/libANGLE/renderer/gl/wgl/WindowSurfaceWGL.cpp',
                'src/libANGLE/renderer/gl/wgl/wgl_utils.cpp',
            ]
            : [],
        have_x11
            ? [ # gl_backend_sources in src/libANGLE/renderer/gl/gl_backend.gni
                'src/libANGLE/renderer/gl/glx/ContextGLX.cpp',
                'src/libANGLE/renderer/gl/glx/DisplayGLX.cpp',
                'src/libANGLE/renderer/gl/glx/FunctionsGLX.cpp',
                'src/libANGLE/renderer/gl/glx/PbufferSurfaceGLX.cpp',
                'src/libANGLE/renderer/gl/glx/PixmapSurfaceGLX.cpp',
                'src/libANGLE/renderer/gl/glx/RendererGLX.cpp',
                'src/libANGLE/renderer/gl/glx/WindowSurfaceGLX.cpp',
                'src/libANGLE/renderer/gl/glx/glx_utils.cpp',
            ]
            : [],
        not host_machine_is_windows and host_machine.system() != 'darwin'
            ? [ # gl_backend_sources in src/libANGLE/renderer/gl/gl_backend.gni
                'src/libANGLE/renderer/gl/egl/ContextEGL.cpp',
                'src/libANGLE/renderer/gl/egl/DeviceEGL.cpp',
                'src/libANGLE/renderer/gl/egl/DisplayEGL.cpp',
                'src/libANGLE/renderer/gl/egl/DmaBufImageSiblingEGL.cpp',
                'src/libANGLE/renderer/gl/egl/FunctionsEGL.cpp',
                'src/libANGLE/renderer/gl/egl/FunctionsEGLDL.cpp',
                'src/libANGLE/renderer/gl/egl/ImageEGL.cpp',
                'src/libANGLE/renderer/gl/egl/PbufferSurfaceEGL.cpp',
                'src/libANGLE/renderer/gl/egl/RendererEGL.cpp',
                'src/libANGLE/renderer/gl/egl/SurfaceEGL.cpp',
                'src/libANGLE/renderer/gl/egl/SyncEGL.cpp',
                'src/libANGLE/renderer/gl/egl/WindowSurfaceEGL.cpp',
                'src/libANGLE/renderer/gl/egl/egl_utils.cpp',
            ]
            : [],
        host_machine.system() == 'android'
            ? [ # gl_backend_sources in src/libANGLE/renderer/gl/gl_backend.gni
                'src/libANGLE/renderer/gl/egl/android/DisplayAndroid.cpp',
                'src/libANGLE/renderer/gl/egl/android/NativeBufferImageSiblingAndroid.cpp',
            ]
            : [],
    ],
    build_by_default: false,
)

angle_spirv_base = static_library(
    'angle_spirv_base',
    override_options: override_options,
    include_directories: includes,
    dependencies: [
        spirv_tools,
    ],
    link_with: [
        angle_common,
    ],
    cpp_args: args,
    sources: [ # angle_spirv_base in src/common/spirv/BUILD.gn
        'src/common/spirv/angle_spirv_utils.cpp',
    ],
    build_by_default: false,
)

angle_spirv_builder = static_library(
    'angle_spirv_builder',
    override_options: override_options,
    include_directories: includes,
    dependencies: [
        spirv_headers,
        spirv_tools,
    ],
    link_with: [
        angle_common,
        angle_spirv_base,
    ],
    cpp_args: args,
    sources: [ # angle_spirv_builder in src/common/spirv/BUILD.gn
        'src/common/spirv/spirv_instruction_builder_autogen.cpp',
    ],
    build_by_default: false,
)

angle_spirv_parser = static_library(
    'angle_spirv_parser',
    override_options: override_options,
    include_directories: includes,
    dependencies: [
        spirv_headers,
        spirv_tools,
    ],
    cpp_args: args,
    sources: [ # angle_spirv_parser in src/common/spirv/BUILD.gn
        'src/common/spirv/spirv_instruction_parser_autogen.cpp',
    ],
    build_by_default: false,
)

angle_vk_mem_alloc_wrapper = static_library(
    'angle_vk_mem_alloc_wrapper',
    override_options: override_options,
    include_directories: includes,
    dependencies: [
        angle_compression_utils,
        spirv_headers,
        spirv_tools,
        volk,
        vulkan_memory_allocator,
    ],
    cpp_args: args + [
        '-DVMA_IMPLEMENTATION',
    ],
    sources: [ # angle_vk_mem_alloc_wrapper in src/libANGLE/renderer/vulkan/BUILD.gn
        'src/libANGLE/renderer/vulkan/vk_mem_alloc_wrapper.cpp',
    ],
    build_by_default: false,
)

angle_vulkan_backend = static_library(
    'angle_vulkan_backend',
    override_options: override_options,
    include_directories: includes,
    dependencies: [
        angle_compression_utils,
        spirv_headers,
        spirv_tools,
        volk,
        wayland_client,
        wayland_egl,
        xcb,
        xxhash,
        host_machine_is_macos ? dependency(
            'appleframeworks',
            modules: [
                'Cocoa',
                'IOSurface',
                'QuartzCore',
            ],
            required: true,
        ) : [],
    ],
    link_with: [
        angle_dma_buf,
        angle_spirv_builder,
        angle_spirv_parser,
        angle_vk_mem_alloc_wrapper,
    ],
    cpp_args: args,
    objcpp_args: objcpp_args,
    sources: [ # vulkan_backend_sources in src/libANGLE/renderer/vulkan/vulkan_backend.gni
        'src/libANGLE/renderer/vulkan/AllocatorHelperPool.cpp',
        'src/libANGLE/renderer/vulkan/BufferVk.cpp',
        'src/libANGLE/renderer/vulkan/CommandQueue.cpp',
        'src/libANGLE/renderer/vulkan/CompilerVk.cpp',
        'src/libANGLE/renderer/vulkan/ContextVk.cpp',
        'src/libANGLE/renderer/vulkan/DebugAnnotatorVk.cpp',
        'src/libANGLE/renderer/vulkan/DeviceVk.cpp',
        'src/libANGLE/renderer/vulkan/DisplayVk.cpp',
        'src/libANGLE/renderer/vulkan/FenceNVVk.cpp',
        'src/libANGLE/renderer/vulkan/FramebufferVk.cpp',
        'src/libANGLE/renderer/vulkan/ImageVk.cpp',
        'src/libANGLE/renderer/vulkan/MemoryObjectVk.cpp',
        'src/libANGLE/renderer/vulkan/MemoryTracking.cpp',
        'src/libANGLE/renderer/vulkan/PersistentCommandPool.cpp',
        'src/libANGLE/renderer/vulkan/ProgramExecutableVk.cpp',
        'src/libANGLE/renderer/vulkan/ProgramPipelineVk.cpp',
        'src/libANGLE/renderer/vulkan/ProgramVk.cpp',
        'src/libANGLE/renderer/vulkan/QueryVk.cpp',
        'src/libANGLE/renderer/vulkan/RenderTargetVk.cpp',
        'src/libANGLE/renderer/vulkan/RenderbufferVk.cpp',
        'src/libANGLE/renderer/vulkan/SamplerVk.cpp',
        'src/libANGLE/renderer/vulkan/SecondaryCommandBuffer.cpp',
        'src/libANGLE/renderer/vulkan/SecondaryCommandPool.cpp',
        'src/libANGLE/renderer/vulkan/SemaphoreVk.cpp',
        'src/libANGLE/renderer/vulkan/ShaderInterfaceVariableInfoMap.cpp',
        'src/libANGLE/renderer/vulkan/ShaderVk.cpp',
        'src/libANGLE/renderer/vulkan/ShareGroupVk.cpp',
        'src/libANGLE/renderer/vulkan/Suballocation.cpp',
        'src/libANGLE/renderer/vulkan/SurfaceVk.cpp',
        'src/libANGLE/renderer/vulkan/SyncVk.cpp',
        'src/libANGLE/renderer/vulkan/TextureVk.cpp',
        'src/libANGLE/renderer/vulkan/TransformFeedbackVk.cpp',
        'src/libANGLE/renderer/vulkan/UtilsVk.cpp',
        'src/libANGLE/renderer/vulkan/VertexArrayVk.cpp',
        'src/libANGLE/renderer/vulkan/VkImageImageSiblingVk.cpp',
        'src/libANGLE/renderer/vulkan/VulkanSecondaryCommandBuffer.cpp',
        'src/libANGLE/renderer/vulkan/android/vk_android_utils.cpp',
        'src/libANGLE/renderer/vulkan/spv_utils.cpp',
        'src/libANGLE/renderer/vulkan/vk_api_perf_counters.cpp',
        'src/libANGLE/renderer/vulkan/vk_barrier_data.cpp',
        'src/libANGLE/renderer/vulkan/vk_cache_utils.cpp',
        'src/libANGLE/renderer/vulkan/vk_caps_utils.cpp',
        'src/libANGLE/renderer/vulkan/vk_format_table_autogen.cpp',
        'src/libANGLE/renderer/vulkan/vk_format_utils.cpp',
        'src/libANGLE/renderer/vulkan/vk_helpers.cpp',
        'src/libANGLE/renderer/vulkan/vk_internal_shaders_autogen.cpp',
        'src/libANGLE/renderer/vulkan/vk_mandatory_format_support_table_autogen.cpp',
        'src/libANGLE/renderer/vulkan/vk_ref_counted_event.cpp',
        'src/libANGLE/renderer/vulkan/vk_renderer.cpp',
        'src/libANGLE/renderer/vulkan/vk_resource.cpp',
        'src/libANGLE/renderer/vulkan/vk_utils.cpp',
        not host_machine_is_windows and host_machine.system() not in ['darwin', 'android']
            ? [ # vulkan_backend_dma_buf_sources in src/libANGLE/renderer/vulkan/vulkan_backend.gni
                'src/libANGLE/renderer/vulkan/linux/DeviceVkLinux.cpp',
                'src/libANGLE/renderer/vulkan/linux/DisplayVkLinux.cpp',
                'src/libANGLE/renderer/vulkan/linux/DisplayVkOffscreen.cpp',
                'src/libANGLE/renderer/vulkan/linux/DmaBufImageSiblingVkLinux.cpp',
                'src/libANGLE/renderer/vulkan/linux/display/DisplayVkSimple.cpp',
                'src/libANGLE/renderer/vulkan/linux/display/WindowSurfaceVkSimple.cpp',
                'src/libANGLE/renderer/vulkan/linux/headless/DisplayVkHeadless.cpp',
                'src/libANGLE/renderer/vulkan/linux/headless/WindowSurfaceVkHeadless.cpp',
            ]
            : [],
        host_machine.system() == 'android'
            ? [ # vulkan_backend_sources in src/libANGLE/renderer/vulkan/vulkan_backend.gni
                'src/libANGLE/renderer/vulkan/android/AHBFunctions.cpp',
                'src/libANGLE/renderer/vulkan/android/DisplayVkAndroid.cpp',
                'src/libANGLE/renderer/vulkan/android/HardwareBufferImageSiblingVkAndroid.cpp',
                'src/libANGLE/renderer/vulkan/android/WindowSurfaceVkAndroid.cpp',
            ]
            : [],
        host_machine_is_windows
            ? [ # vulkan_backend_sources in src/libANGLE/renderer/vulkan/vulkan_backend.gni
                'src/libANGLE/renderer/vulkan/win32/DisplayVkWin32.cpp',
                'src/libANGLE/renderer/vulkan/win32/WindowSurfaceVkWin32.cpp',
            ]
            : [],
        have_x11
            ? [ # vulkan_backend_sources in src/libANGLE/renderer/vulkan/vulkan_backend.gni
                'src/libANGLE/renderer/vulkan/linux/xcb/DisplayVkXcb.cpp',
                'src/libANGLE/renderer/vulkan/linux/xcb/WindowSurfaceVkXcb.cpp',
            ]
            : [],
        have_wayland
            ? [ # vulkan_backend_sources in src/libANGLE/renderer/vulkan/vulkan_backend.gni
                'src/libANGLE/renderer/vulkan/linux/wayland/DisplayVkWayland.cpp',
                'src/libANGLE/renderer/vulkan/linux/wayland/WindowSurfaceVkWayland.cpp',
            ]
            : [],
        host_machine_is_macos
            ? [ # vulkan_backend_sources in src/libANGLE/renderer/vulkan/vulkan_backend.gni
                'src/libANGLE/renderer/vulkan/mac/DisplayVkMac.mm',
                'src/libANGLE/renderer/vulkan/mac/IOSurfaceSurfaceVkMac.mm',
                'src/libANGLE/renderer/vulkan/mac/WindowSurfaceVkMac.mm',
            ]
            : [],
    ],
    build_by_default: false,
)

angle_glesv2_thin = static_library(
    'GLESv2_thin',
    override_options: override_options,
    include_directories: includes,
    dependencies: [
        xxhash,
    ],
    cpp_args: args + export_args,
    sources: [ # libglesv2_entry_point_sources in src/libGLESv2.gni
        'src/libGLESv2/egl_ext_stubs.cpp',
        'src/libGLESv2/egl_stubs.cpp',
        'src/libGLESv2/egl_stubs_getprocaddress_autogen.cpp',
        'src/libGLESv2/entry_points_egl_autogen.cpp',
        'src/libGLESv2/entry_points_egl_ext_autogen.cpp',
        'src/libGLESv2/entry_points_gles_1_0_autogen.cpp',
        'src/libGLESv2/entry_points_gles_2_0_autogen.cpp',
        'src/libGLESv2/entry_points_gles_3_0_autogen.cpp',
        'src/libGLESv2/entry_points_gles_3_1_autogen.cpp',
        'src/libGLESv2/entry_points_gles_3_2_autogen.cpp',
        'src/libGLESv2/entry_points_gles_ext_autogen.cpp',
        'src/libGLESv2/global_state.cpp',
    ],
    build_by_default: false,
)

angle_glesv2 = static_library(
    'GLESv2',
    override_options: override_options,
    include_directories: includes,
    link_with: [
        angle_glesv2_thin,
    ],
    cpp_args: args,
    sources: [ # libglesv2_sources in src/libGLESv2.gni
        'src/libGLESv2/libGLESv2_autogen.cpp',
    ],
    build_by_default: false,
)

mkxp_angle_egl_shim = static_library(
    'mkxp-angle-egl-shim',
    dependencies: [
        dl,
    ],
    override_options: override_options,
    include_directories: includes,
    cpp_args: args + export_args + egl_shim_args,
    sources: [
        'mkxp-angle-egl-shim.cpp',
    ],
    build_by_default: false,
)

angle_egl = static_library(
    'EGL',
    override_options: override_options,
    include_directories: includes,
    link_with: [
        angle_glesv2,
        mkxp_angle_egl_shim,
    ],
    cpp_args: args + export_args + egl_args,
    sources: [ # libegl_sources in src/libGLESv2.gni
        'src/libEGL/libEGL_autogen.cpp',
    ],
    build_by_default: false,
)

angle_util = static_library(
    'angle_util',
    override_options: override_options,
    include_directories: includes,
    cpp_args: args + [
        '-DM_PI=3.14159265358979323846',
    ],
    sources: [ # _util_sources in util/BUILD.gn
        'util/EGLWindow.cpp',
        'util/Matrix.cpp',
        'util/OSWindow.cpp',
        'util/autogen/angle_features_autogen.cpp',
        'util/geometry_utils.cpp',
        'util/random_utils.cpp',
        'util/shader_utils.cpp',
        not host_machine_is_windows and host_machine.system() not in ['darwin', 'android']
            ? [
                'util/display/DisplayPixmap.cpp',
                'util/display/DisplayWindow.cpp',
            ]
            : [],
    ],
    build_by_default: false,
)

angle = library(
    'ANGLE',
    override_options: override_options,
    include_directories: includes,
    dependencies: [
        angle_compression_utils,
        xxhash,
        host_machine_is_windows
            ? [
                meson.get_compiler('cpp').find_library('gdi32', required: true),
                meson.get_compiler('cpp').find_library('user32', required: true),
            ]
            : [],
    ],
    link_with: [
        angle_common,
        angle_common_shader_state,
        angle_egl,
        angle_frame_capture_mock,
        angle_glesv2,
        angle_gpu_info_util,
        angle_image_util,
        angle_preprocessor,
        angle_version_info,
        angle_translator,
        angle_util,
        have_direct3d11 ? angle_d3d11_backend : [],
        have_null ? angle_null_backend : [],
        have_metal ? angle_metal_backend : [],
        have_opengl ? angle_gl_backend : [],
        have_vulkan ? angle_vulkan_backend : [],
    ],
    cpp_args: args,
    sources: [ # libangle_sources in src/libGLESv2.gni
        'src/libANGLE/AttributeMap.cpp',
        'src/libANGLE/BlobCache.cpp',
        'src/libANGLE/Buffer.cpp',
        'src/libANGLE/Caps.cpp',
        'src/libANGLE/Compiler.cpp',
        'src/libANGLE/Config.cpp',
        'src/libANGLE/Context.cpp',
        'src/libANGLE/ContextMutex.cpp',
        'src/libANGLE/Context_gles_1_0.cpp',
        'src/libANGLE/Debug.cpp',
        'src/libANGLE/Device.cpp',
        'src/libANGLE/Display.cpp',
        'src/libANGLE/EGLSync.cpp',
        'src/libANGLE/Error.cpp',
        'src/libANGLE/Fence.cpp',
        'src/libANGLE/Framebuffer.cpp',
        'src/libANGLE/FramebufferAttachment.cpp',
        'src/libANGLE/GLES1Renderer.cpp',
        'src/libANGLE/GLES1State.cpp',
        'src/libANGLE/GlobalMutex.cpp',
        'src/libANGLE/HandleAllocator.cpp',
        'src/libANGLE/Image.cpp',
        'src/libANGLE/ImageIndex.cpp',
        'src/libANGLE/IndexRangeCache.cpp',
        'src/libANGLE/LoggingAnnotator.cpp',
        'src/libANGLE/MemoryObject.cpp',
        'src/libANGLE/MemoryProgramCache.cpp',
        'src/libANGLE/MemoryShaderCache.cpp',
        'src/libANGLE/Observer.cpp',
        'src/libANGLE/PixelLocalStorage.cpp',
        'src/libANGLE/Platform.cpp',
        'src/libANGLE/Program.cpp',
        'src/libANGLE/ProgramExecutable.cpp',
        'src/libANGLE/ProgramLinkedResources.cpp',
        'src/libANGLE/ProgramPipeline.cpp',
        'src/libANGLE/Query.cpp',
        'src/libANGLE/Renderbuffer.cpp',
        'src/libANGLE/ResourceManager.cpp',
        'src/libANGLE/Sampler.cpp',
        'src/libANGLE/Semaphore.cpp',
        'src/libANGLE/Shader.cpp',
        'src/libANGLE/ShareGroup.cpp',
        'src/libANGLE/State.cpp',
        'src/libANGLE/Stream.cpp',
        'src/libANGLE/Surface.cpp',
        'src/libANGLE/Texture.cpp',
        'src/libANGLE/Thread.cpp',
        'src/libANGLE/TransformFeedback.cpp',
        'src/libANGLE/Uniform.cpp',
        'src/libANGLE/VaryingPacking.cpp',
        'src/libANGLE/VertexArray.cpp',
        'src/libANGLE/VertexAttribute.cpp',
        'src/libANGLE/angletypes.cpp',
        'src/libANGLE/es3_copy_conversion_table_autogen.cpp',
        'src/libANGLE/format_map_autogen.cpp',
        'src/libANGLE/formatutils.cpp',
        'src/libANGLE/gles_extensions_autogen.cpp',
        'src/libANGLE/queryconversions.cpp',
        'src/libANGLE/queryutils.cpp',
        'src/libANGLE/renderer/BufferImpl.cpp',
        'src/libANGLE/renderer/ContextImpl.cpp',
        'src/libANGLE/renderer/DeviceImpl.cpp',
        'src/libANGLE/renderer/DisplayImpl.cpp',
        'src/libANGLE/renderer/EGLReusableSync.cpp',
        'src/libANGLE/renderer/EGLSyncImpl.cpp',
        'src/libANGLE/renderer/Format_table_autogen.cpp',
        'src/libANGLE/renderer/FramebufferImpl.cpp',
        'src/libANGLE/renderer/ImageImpl.cpp',
        'src/libANGLE/renderer/ProgramImpl.cpp',
        'src/libANGLE/renderer/ProgramPipelineImpl.cpp',
        'src/libANGLE/renderer/QueryImpl.cpp',
        'src/libANGLE/renderer/RenderbufferImpl.cpp',
        'src/libANGLE/renderer/ShaderImpl.cpp',
        'src/libANGLE/renderer/SurfaceImpl.cpp',
        'src/libANGLE/renderer/TextureImpl.cpp',
        'src/libANGLE/renderer/TransformFeedbackImpl.cpp',
        'src/libANGLE/renderer/VertexArrayImpl.cpp',
        'src/libANGLE/renderer/driver_utils.cpp',
        'src/libANGLE/renderer/load_functions_table_autogen.cpp',
        'src/libANGLE/renderer/renderer_utils.cpp',
        'src/libANGLE/validationEGL.cpp',
        'src/libANGLE/validationES.cpp',
        'src/libANGLE/validationES1.cpp',
        'src/libANGLE/validationES2.cpp',
        'src/libANGLE/validationES3.cpp',
        'src/libANGLE/validationES31.cpp',
        'src/libANGLE/validationES32.cpp',
        'src/libANGLE/validationESEXT.cpp',
    ],
)

angle_dep = declare_dependency(
    include_directories: 'include',
    link_with: angle,
)

meson.override_dependency('angle', angle_dep)
