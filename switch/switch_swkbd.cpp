#ifdef __SWITCH__

// Avoid pulling in Ruby's assert.h from libnx service.h (see switch_main.cpp)
#define RUBY_ASSERT_H 1

#include <switch.h>
#include <string.h>

// Shows the system software keyboard and waits for it to close. Returns 1 with
// the entered UTF-8 text in out, 0 if the player cancelled, or -1 if the
// keyboard could not be shown. Kept out of src/ so libnx's type names (Event,
// Mutex, Thread, ...) stay away from mkxp-z's headers.
extern "C" int switch_swkbd_get_text(char *out, size_t outSize) {
    if (!out || outSize == 0)
        return -1;
    out[0] = '\0';

    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0)))
        return -1;
    swkbdConfigMakePresetDefault(&kbd);

    // libnx reports a cancelled keyboard as a failed result
    Result rc = swkbdShow(&kbd, out, outSize);
    swkbdClose(&kbd);
    if (R_FAILED(rc)) {
        out[0] = '\0';
        return 0;
    }
    return 1;
}

#endif // __SWITCH__
