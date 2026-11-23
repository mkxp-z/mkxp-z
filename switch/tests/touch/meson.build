# Unit tests for the game picker's touch handling (switch_touch.h), built for
# and run on the build machine: meson test -C build. Skipped without a native
# C++ compiler.
if add_languages('cpp', native: true, required: false)
    test(
        'touch',
        executable(
            'test_touch',
            'test_touch.cpp',
            include_directories: include_directories('../..'),
            native: true,
            build_by_default: false,
        ),
    )
endif
