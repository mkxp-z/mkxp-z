# Unit tests for the controller profiles (switch_controls.cpp), built for and
# run on the build machine: meson test -C build. Skipped without a native C++
# compiler and SDL2 development files.
if add_languages('cpp', native: true, required: false)
    sdl2_native = dependency('sdl2', native: true, required: false)
    if sdl2_native.found()
        test(
            'controls',
            executable(
                'test_controls',
                'test_controls.cpp',
                '../../switch_controls.cpp',
                include_directories: include_directories('../..', '../../../src'),
                dependencies: sdl2_native,
                native: true,
                build_by_default: false,
            ),
        )
    endif
endif
