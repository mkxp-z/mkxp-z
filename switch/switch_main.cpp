
#ifdef __SWITCH__

// Avoid pulling in Ruby's assert.h from libnx service.h:
// we don't need Ruby APIs in this TU, and ruby/config.h
// is not available in our cross-build include layout.
#define RUBY_ASSERT_H 1

#include <switch.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <malloc.h>
#include <stdlib.h>

// Hidden by newlib under strict -std=c++14
extern "C" int setenv(const char *name, const char *value, int overwrite);
#include <SDL_timer.h>
#include <SDL_audio.h>
#include <AL/alc.h>
#include <AL/alext.h>

// Verbose SWITCH_TRACE output (src/util/switch-trace.h), on when this exists
#define DEBUG_FLAG_FILE "sdmc:/switch/mkxp-z/debug.txt"

// Written before main() so a crash in userAppInit()/the game picker still
// leaves a breadcrumb on the SD card. Kept separate from mkxpz_log.txt.
#define BOOT_LOG_FILE "sdmc:/mkxpz_boot.log"

// Periodic memory, frame rate and audio samples (debug tracing only), written
// from SDL's timer thread to their own file.
#define MEM_LOG_FILE "sdmc:/mkxpz_mem.log"

// Skips the CPU boost below when this exists
#define NO_BOOST_FLAG_FILE "sdmc:/switch/mkxp-z/noboost.txt"

// Homebrew gets the stock 1020 MHz CPU clock, where Ruby-heavy games
// (Pokemon Essentials) stutter on menus, map changes and battles. Run at
// 1785 MHz, the clock the system itself uses in boost mode.
#define BOOST_CPU_HZ 1785000000

extern "C" {
int switch_trace_enabled = 0;
}

// Raw open/write/close: safe before libc constructors have run.
static void boot_log(const char *msg) {
    int fd = open(BOOT_LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd >= 0) {
        write(fd, msg, strlen(msg));
        close(fd);
    }
}

// Open one log candidate and report the result to the boot log, so a silent
// logging failure (seen on hardware: sdmc:/mkxpz_log.txt would not open)
// can never again hide what main() did.
static int open_log_fd(const char *path, int flags) {
    int fd = open(path, flags, 0666);
    char msg[192];
    if (fd >= 0)
        snprintf(msg, sizeof(msg), "logging: stdout -> %s ok\n", path);
    else
        snprintf(msg, sizeof(msg), "logging: open %s FAILED (errno=%d)\n",
                 path, errno);
    boot_log(msg);
    return fd;
}

static const char *applet_type_name(AppletType t) {
    switch (t) {
    case AppletType_Application:       return "application (full memory)";
    case AppletType_SystemApplication: return "system application";
    case AppletType_LibraryApplet:     return "library applet (Album launch, limited memory)";
    case AppletType_OverlayApplet:     return "overlay applet";
    case AppletType_SystemApplet:      return "system applet";
    default:                           return "unknown";
    }
}

static u64 process_info(InfoType type) {
    u64 value = 0;
    svcGetInfo(&value, type, CUR_PROCESS_HANDLE, 0);
    return value;
}

// CPU clock control: clkrst on 8.0.0+, pcv before that.
static bool clockReady;
static u32 cpuClockBefore;
static bool cpuBoosted;
static AppletHookCookie clockHook;

static u32 get_cpu_clock() {
    u32 hz = 0;
    if (!clockReady)
        return 0;
    if (hosversionAtLeast(8, 0, 0)) {
        ClkrstSession s;
        if (R_SUCCEEDED(clkrstOpenSession(&s, PcvModuleId_CpuBus, 3))) {
            clkrstGetClockRate(&s, &hz);
            clkrstCloseSession(&s);
        }
    } else {
        pcvGetClockRate(PcvModule_CpuBus, &hz);
    }
    return hz;
}

static bool set_cpu_clock(u32 hz) {
    if (!clockReady)
        return false;
    if (hosversionAtLeast(8, 0, 0)) {
        ClkrstSession s;
        if (R_FAILED(clkrstOpenSession(&s, PcvModuleId_CpuBus, 3)))
            return false;
        Result rc = clkrstSetClockRate(&s, hz);
        clkrstCloseSession(&s);
        return R_SUCCEEDED(rc);
    }
    return R_SUCCEEDED(pcvSetClockRate(PcvModule_CpuBus, hz));
}

// The system resets clocks on dock/undock and when returning from HOME, so
// reapply the boost on those events, and drop it while out of focus.
static void clock_hook(AppletHookType hook, void *) {
    if (hook == AppletHookType_OnExitRequest)
        return;
    if (appletGetFocusState() == AppletFocusState_InFocus)
        set_cpu_clock(BOOST_CPU_HZ);
    else
        set_cpu_clock(cpuClockBefore);
}

static void boost_cpu() {
    struct stat st;
    if (stat(NO_BOOST_FLAG_FILE, &st) == 0) {
        boot_log("cpu: boost skipped (noboost.txt)\n");
        return;
    }
    clockReady = R_SUCCEEDED(hosversionAtLeast(8, 0, 0) ? clkrstInitialize()
                                                         : pcvInitialize());
    cpuClockBefore = get_cpu_clock();
    if (cpuClockBefore == 0)
        cpuClockBefore = 1020000000;
    cpuBoosted = set_cpu_clock(BOOST_CPU_HZ);
    if (cpuBoosted)
        appletHook(&clockHook, clock_hook, NULL);

    char msg[96];
    snprintf(msg, sizeof(msg), "cpu: %u MHz -> %u MHz %s\n",
             cpuClockBefore / 1000000, get_cpu_clock() / 1000000,
             cpuBoosted ? "ok" : "FAILED");
    boot_log(msg);
}

static void restore_cpu() {
    if (cpuBoosted) {
        appletUnhook(&clockHook);
        set_cpu_clock(cpuClockBefore);
    }
    if (clockReady) {
        if (hosversionAtLeast(8, 0, 0))
            clkrstExit();
        else
            pcvExit();
    }
}

// Frame pacing, for the memory log: frames presented and the longest gap
// between two frames since the last sample. Written by the RGSS thread
// (via EventThread::notifyFrame), read by the log timer.
static u64 frameCount;
static u64 frameLastTick;
static u64 frameMaxGap;

extern "C" void switch_note_frame() {
    u64 now = armGetSystemTick();
    if (frameLastTick) {
        u64 gap = now - frameLastTick;
        if (gap > __atomic_load_n(&frameMaxGap, __ATOMIC_RELAXED))
            __atomic_store_n(&frameMaxGap, gap, __ATOMIC_RELAXED);
    }
    frameLastTick = now;
    __atomic_add_fetch(&frameCount, 1, __ATOMIC_RELAXED);
}

static u64 memLogStart;
static u64 memLogPeak;
static u64 memLogLastTick;

// Logs elapsed time, memory use and audio state every few seconds, so a
// hardware log shows whether a "stuck" game is still progressing, has run out
// of memory, or has lost its audio output.
static Uint32 log_memory(Uint32 interval, void *) {
    // libnx maps all free memory as heap up front, so the kernel's
    // used-memory figure is constant; malloc's own numbers are not.
    struct mallinfo mi = mallinfo();
    u64 used = (u64)mi.uordblks;
    if (used > memLogPeak)
        memLogPeak = used;
    // Audio pipeline health: is OpenAL's device still connected and its
    // mixer clock advancing, and what state is SDL's audio device in?
    ALCint connected = -1;
    long long clockMs = -1;
    ALCcontext *ctx = alcGetCurrentContext();
    ALCdevice *dev = ctx ? alcGetContextsDevice(ctx) : NULL;
    if (dev) {
        alcGetIntegerv(dev, ALC_CONNECTED, 1, &connected);
        LPALCGETINTEGER64VSOFT getInt64 =
            (LPALCGETINTEGER64VSOFT)alcGetProcAddress(dev, "alcGetInteger64vSOFT");
        ALCint64SOFT clock = 0;
        if (getInt64) {
            getInt64(dev, ALC_DEVICE_CLOCK_SOFT, 1, &clock);
            clockMs = (long long)(clock / 1000000);
        }
    }
    u64 now = armGetSystemTick();
    u64 frames = __atomic_exchange_n(&frameCount, 0, __ATOMIC_RELAXED);
    u64 maxGap = __atomic_exchange_n(&frameMaxGap, 0, __ATOMIC_RELAXED);
    u64 sinceLastMs = armTicksToNs(now - memLogLastTick) / 1000000ULL;
    memLogLastTick = now;
    char line[256];
    int len = snprintf(line, sizeof(line),
                       "t=%llus fps=%.1f worst_frame=%llums cpu=%uMHz"
                       " malloc used=%lluMB peak=%lluMB heap=%lluMB"
                       " al_connected=%d al_clock=%lldms sdl_audio=%d,%d,%d\n",
                       (unsigned long long)(armTicksToNs(now - memLogStart) / 1000000000ULL),
                       sinceLastMs ? frames * 1000.0 / sinceLastMs : 0.0,
                       (unsigned long long)(armTicksToNs(maxGap) / 1000000ULL),
                       get_cpu_clock() / 1000000,
                       (unsigned long long)(used >> 20), (unsigned long long)(memLogPeak >> 20),
                       (unsigned long long)((u64)mi.arena >> 20), connected, clockMs,
                       SDL_GetAudioDeviceStatus(1), SDL_GetAudioDeviceStatus(2),
                       SDL_GetAudioDeviceStatus(3));
    int fd = open(MEM_LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd >= 0) {
        write(fd, line, len);
        close(fd);
    }
    return interval;
}

static void log_launch_environment() {
    printf("launch: applet type = %s\n", applet_type_name(appletGetAppletType()));
    printf("launch: memory total=%lluMB used=%lluMB\n",
           (unsigned long long)(process_info(InfoType_TotalMemorySize) >> 20),
           (unsigned long long)(process_info(InfoType_UsedMemorySize) >> 20));

    if (switch_trace_enabled) {
        close(open(MEM_LOG_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666));
        memLogStart = memLogLastTick = armGetSystemTick();
        SDL_AddTimer(5000, log_memory, NULL);
    }
}

// Helper to redirect stdout/stderr to a file on the SD card
void setup_file_logging() {
    // O_TRUNC clears the file every time you launch the app.
    // Preferred location is the SD root log; on hardware the open of the
    // pre-existing (emulator-created) file failed, so fall back to the
    // switch folder, then to appending to the boot log itself.
    int fd = open_log_fd("sdmc:/mkxpz_log.txt", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0)
        fd = open_log_fd("sdmc:/switch/mkxpz_log.txt",
                         O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0)
        fd = open_log_fd(BOOT_LOG_FILE, O_WRONLY | O_CREAT | O_APPEND);

    if (fd >= 0) {
        // Redirect stdout (1) and stderr (2) to the file descriptor
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        
        // Close the original file descriptor (dup2 kept a copy)
        close(fd);

        // CRITICAL: Disable buffering. 
        // If the app crashes, buffered text won't be written to the file.
        // With _IONBF, every printf is written immediately.
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);

        struct stat st;
        switch_trace_enabled = stat(DEBUG_FLAG_FILE, &st) == 0;
        printf("--- MKXP-Z Switch Log Start ---%s\n",
               switch_trace_enabled ? " (debug tracing on)" : "");
        log_launch_environment();
    }
}

static bool socketsReady = false;

extern "C" {

// This hook is called by libnx before main()
void userAppInit(void) {
    // Start a fresh boot log for this launch.
    int fd = open(BOOT_LOG_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd >= 0) {
        const char *msg = "userAppInit: start\n";
        write(fd, msg, strlen(msg));
        close(fd);
    }

    // 1. Initialize ROMFS (read-only filesystem inside the NRO)
    Result rc = romfsInit();
    boot_log(R_SUCCEEDED(rc) ? "userAppInit: romfsInit ok\n"
                             : "userAppInit: romfsInit FAILED\n");

    // 2. Logging to sdmc:/mkxpz_log.txt is set up by main() via
    //    switch_enable_logging(), after the game picker: the picker draws
    //    on the libnx console, which needs stdout.

    // 3. Networking is deliberately DISABLED here. Starting the BSD socket
    //    service in userAppInit() crashes on real hardware with
    //    2168-0002 (0x4A8), before main() or any logging runs. The Ruby
    //    socket extension is still linked; without this service, socket
    //    operations simply fail with errors instead of taking down the app.
    // socketsReady = R_SUCCEEDED(socketInitializeDefault());

    boost_cpu();

    boot_log("userAppInit: done (networking disabled)\n");
}

// This hook is called after main() returns
void userAppExit(void) {
    restore_cpu();
    if (socketsReady)
        socketExit();
    romfsExit();
}

} // extern "C"

// Called before alcOpenDevice(). With debug tracing on, OpenAL Soft logs its
// warnings (underruns, backend errors) to stderr, i.e. mkxpz_log.txt. If
// sdmc:/switch/mkxp-z/alwave.txt also exists, OpenAL mixes into
// sdmc:/mkxpz_al.wav instead of the speakers, to tell mixer problems apart
// from SDL/audren output problems.
static void switch_setup_openal_debug() {
    if (!switch_trace_enabled)
        return;
    setenv("ALSOFT_LOGLEVEL", "3", 1);

    struct stat st;
    if (stat("sdmc:/switch/mkxp-z/alwave.txt", &st) != 0)
        return;
    const char *conf = "sdmc:/switch/mkxp-z/alsoft_wave.conf";
    FILE *f = fopen(conf, "w");
    if (!f)
        return;
    fputs("[general]\ndrivers = wave\n[wave]\nfile = sdmc:/mkxpz_al.wav\n", f);
    fclose(f);
    setenv("ALSOFT_CONF", conf, 1);
    setenv("ALSOFT_DRIVERS", "wave", 1);
    printf("openal: wave backend -> sdmc:/mkxpz_al.wav\n");
}

// Called by src/main.cpp after the game picker, before OpenAL is opened
extern "C" void switch_enable_logging() {
    setup_file_logging();
    switch_setup_openal_debug();
}

#endif // __SWITCH__
