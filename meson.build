project('mkxp-z', 'c', 'cpp', version: '2.4.2', meson_version: '>=1.3.0', default_options: ['cpp_std=c++14', 'buildtype=release'])

host_system = host_machine.system()
host_endian = host_machine.endian()
host_cpu_family = host_machine.cpu_family()

compilers = {'c': meson.get_compiler('c'), 'cpp': meson.get_compiler('cpp')}
is_clang = ['armclang', 'clang', 'emscripten', 'llvm'].contains(compilers['cpp'].get_id())

is_libretro = get_option('libretro')
is_emscripten = host_system == 'emscripten'
core_is_static = is_libretro and (is_emscripten or host_system == 'none' or host_system == 'bare')
is_vitasdk = core_is_static and host_cpu_family == 'arm' and compilers['c'].has_header_symbol('sys/config.h', '__vita__')
is_devkitarm = core_is_static and host_cpu_family == 'arm' and not is_vitasdk
is_devkitppc = core_is_static and host_cpu_family == 'ppc'
is_devkita64 = core_is_static and host_cpu_family == 'aarch64'
is_devkitpro = is_devkitarm or is_devkitppc or is_devkita64
is_orbisdev = core_is_static and host_cpu_family == 'x86_64'

# Position-independent code is not supported on some platforms where we need to build a static library, e.g. https://github.com/vitasdk/vita-toolchain/issues/264
use_pic = is_devkita64 or not core_is_static

if not is_libretro and host_system == 'darwin'
    error('This Meson project no longer supports macOS. Please use the Xcode project instead.')
endif

global_sources = []
global_dependencies = [declare_dependency(sources: vcs_tag(command: ['git', 'rev-parse', '--short=7', 'HEAD'], fallback: 'unknown', input: 'src/git-hash.h.in', output: 'git-hash.h'))]
global_include_dirs = []
global_args = []
global_cpp_args = []
global_link_args = []
link_test_args = []

if is_libretro
    global_cpp_args += '-fno-rtti'
endif

sizeof = {'void*': compilers['cpp'].sizeof('void*'),
          'long': compilers['cpp'].sizeof('long')
         }
if sizeof['void*'] <= 0
    error('Failed to detect size of void*')
endif
if sizeof['long'] <= 0
    error('Failed to detect size of long')
endif
win64 = (sizeof['void*'] != sizeof['long'])
android_api_level = host_system == 'android' ? compilers['c'].get_define('__ANDROID_API__').to_int() : 0
android_ndk_root = host_system == 'android' ? run_command(compilers['c'], '-print-file-name=sysroot', check: true).stdout().strip() / '..' / '..' / '..' / '..' / '..'  : ''

global_args += '-DMKXPZ_BUILD_MESON'
global_args += '-DMKXPZ_VERSION="@0@"'.format(meson.project_version())

have_any_mutex = true
have_any_thread = true

have_std_mutex = true
have_pthread_mutex = true
have_std_thread = true
have_pthread_thread = true

if is_libretro
    libretro_stage1_path = get_option('libretro_stage1_path')

    global_args += '-DMKXPZ_RETRO'
    # https://github.com/android/ndk/issues/442
    if host_system == 'android' and sizeof['void*'] == 4 and android_api_level < 24
        global_args += '-D_FILE_OFFSET_BITS=32'
    else
        global_args += '-D_FILE_OFFSET_BITS=64'
    endif
    global_args += '-D_TIME_BITS=64'

    if not get_option('libretro_save_states')
        global_args += '-DMKXPZ_RETRO_NO_SAVE_STATES'
    endif

    wasi_target = import('fs').read(libretro_stage1_path / 'ruby/mkxp-sandbox-ruby-target.txt').strip()
    if wasi_target.endswith('-wasip1') or wasi_target.endswith('-wasip1-threads') or wasi_target.endswith('-wasi') or wasi_target.endswith('-wasi-threads')
        global_args += '-DMKXPZ_WASI_VERSION_MAJOR=0'
        global_args += '-DMKXPZ_WASI_VERSION_MINOR=1'
    elif wasi_target.endswith('-wasip2')
        global_args += '-DMKXPZ_WASI_VERSION_MAJOR=0'
        global_args += '-DMKXPZ_WASI_VERSION_MINOR=2'
    else
        error('Unsupported WASI target "' + wasi_target + '".')
    endif
    message('WASI target: "' + wasi_target + '"')
else
    global_args += '-DHAVE_NANOSLEEP'
endif

if sizeof['void*'] <= 4
    global_args += '-DMKXPZ_32_BIT_POINTER'
endif

# A lot of toolchains cannot build a libretro core with LTO enabled without also passing `-ffat-lto-objects`,
# so enable it for all compilers when building with LTO unless building for Android, Darwin or Emscripten,
# since toolchains for those platforms are known to support LTO without `-ffat-lto-objects`
if is_libretro and get_option('b_lto') and host_system != 'android' and host_system != 'darwin' and not is_emscripten and compilers['c'].has_argument('-ffat-lto-objects')
    global_args += '-ffat-lto-objects'
endif

if is_vitasdk or (host_cpu_family == 'arm' and get_option('neon') and compilers['c'].has_argument('-mfpu=neon'))
    global_args += '-mfpu=neon'
endif

if is_vitasdk
    global_args += '-mword-relocations'
endif

if is_devkitarm
    global_args += '-march=armv6k'
    global_args += '-mtune=mpcore'
    global_args += '-mfloat-abi=hard'
    global_args += '-marm'
    global_args += '-mfpu=vfp'
    global_args += '-mtp=soft'
endif

if is_devkita64
    global_args += '-march=armv8-a'
    global_args += '-mtune=cortex-a57'
    global_args += '-mtp=soft'
    global_args += '-ftls-model=local-exec'
endif

# The PlayStation 4 toolchain won't link most programs without either `-r` or `-Wl,--gc-sections`
if is_orbisdev
    link_test_args += '-r'
endif

if is_emscripten
    if get_option('emscripten_threaded')
        global_args += '-pthread'
    else
        have_any_mutex = false
        have_any_thread = false
    endif
endif

if host_endian == 'big'
    global_args += '-DMKXPZ_BIG_ENDIAN'
endif

if is_emscripten or core_is_static or not compilers['cpp'].compiles('struct E {}; int main() { throw E(); }', name: 'C++ exceptions support check')
    global_cpp_args += '-fno-exceptions'
    global_args += '-DMKXPZ_NO_EXCEPTIONS'
    global_args += '-DBOOST_NO_EXCEPTIONS'
endif

if not compilers['cpp'].has_header_symbol('stdio.h', 'sprintf')
    global_args += '-DMKXPZ_NO_SPRINTF'
endif

if not compilers['cpp'].has_header_symbol('stdio.h', 'snprintf')
    global_args += '-DMKXPZ_NO_SNPRINTF'
endif

if not compilers['cpp'].has_header_symbol('stdio.h', 'vsprintf')
    global_args += '-DMKXPZ_NO_VSPRINTF'
endif

if not compilers['cpp'].has_header_symbol('stdio.h', 'vsnprintf')
    global_args += '-DMKXPZ_NO_VSNPRINTF'
endif

if not compilers['cpp'].has_header_symbol('cstdio', 'std::sprintf')
    global_args += '-DMKXPZ_NO_STD_SPRINTF'
endif

if not compilers['cpp'].has_header_symbol('cstdio', 'std::snprintf')
    global_args += '-DMKXPZ_NO_STD_SNPRINTF'
endif

if not compilers['cpp'].has_header_symbol('cstdio', 'std::vsprintf')
    global_args += '-DMKXPZ_NO_STD_VSPRINTF'
endif

if not compilers['cpp'].has_header_symbol('cstdio', 'std::vsnprintf')
    global_args += '-DMKXPZ_NO_STD_VSNPRINTF'
endif

if not compilers['cpp'].has_header_symbol('cmath', 'std::round')
    global_args += '-DMKXPZ_NO_STD_ROUND'
endif

if not compilers['cpp'].has_header_symbol('cmath', 'std::lround')
    global_args += '-DMKXPZ_NO_STD_LROUND'
endif

if not compilers['cpp'].has_header_symbol('cmath', 'std::copysign')
    global_args += '-DMKXPZ_NO_STD_COPYSIGN'
endif

if not compilers['cpp'].has_header_symbol('cmath', 'std::cbrt')
    global_args += '-DMKXPZ_NO_STD_CBRT'
endif

if not compilers['cpp'].has_header_symbol('cmath', 'std::log2')
    global_args += '-DMKXPZ_NO_STD_LOG2'
endif

if not compilers['cpp'].has_header_symbol('string', 'std::to_string')
    global_args += '-DMKXPZ_NO_STD_TO_STRING'
endif

if not compilers['cpp'].has_header_symbol('string', 'std::stoi')
    global_args += '-DMKXPZ_NO_STD_STOI'
endif

if not compilers['cpp'].has_header_symbol('string', 'std::stol')
    global_args += '-DMKXPZ_NO_STD_STOL'
endif

if not compilers['cpp'].has_header_symbol('string', 'std::stoll')
    global_args += '-DMKXPZ_NO_STD_STOLL'
endif

if not compilers['cpp'].has_header_symbol('string', 'std::stoul')
    global_args += '-DMKXPZ_NO_STD_STOUL'
endif

if not compilers['cpp'].has_header_symbol('string', 'std::stoull')
    global_args += '-DMKXPZ_NO_STD_STOULL'
endif

if not compilers['cpp'].has_header_symbol('string', 'std::stof')
    global_args += '-DMKXPZ_NO_STD_STOF'
endif

if not compilers['cpp'].has_header_symbol('string', 'std::stod')
    global_args += '-DMKXPZ_NO_STD_STOD'
endif

if not compilers['cpp'].has_header_symbol('string', 'std::stold')
    global_args += '-DMKXPZ_NO_STD_STOLD'
endif

if not compilers['cpp'].has_header_symbol('mutex', 'std::mutex')
    have_std_mutex = false
    global_args += '-DMKXPZ_NO_STD_MUTEX'

    if not compilers['cpp'].has_header_symbol('condition_variable', 'std::condition_variable_any')
        global_args += '-DMKXPZ_NO_STD_CONDITION_VARIABLE_ANY'
    endif
endif

if not compilers['cpp'].has_header_symbol('mutex', 'std::recursive_mutex')
    global_args += '-DMKXPZ_NO_STD_RECURSIVE_MUTEX'
endif

if not compilers['cpp'].links('''
    #include <atomic>
    int main(void) {
        std::atomic<uint64_t> atom(1);
        atom.store(2, std::memory_order_seq_cst);
        return atom.load(std::memory_order_seq_cst) != 2 || atom.fetch_add(3, std::memory_order_seq_cst) != 2 || atom.load(std::memory_order_seq_cst) != 5;
    }
''', args: link_test_args, name: 'check for broken std::atomic<uint64_t>')
    global_args += '-DMKXPZ_NO_STD_ATOMIC_UINT64_T'
endif

if not compilers['cpp'].has_header_symbol('thread', 'std::thread')
    have_std_thread = false
    global_args += '-DMKXPZ_NO_STD_THREAD'
endif

if not compilers['cpp'].has_header_symbol('thread', 'std::this_thread::yield')
    global_args += '-DMKXPZ_NO_STD_THIS_THREAD_YIELD'
endif

if not compilers['cpp'].has_header_symbol('thread', 'std::this_thread::sleep_for')
    global_args += '-DMKXPZ_NO_STD_THIS_THREAD_SLEEP_FOR'
endif

if not compilers['cpp'].has_header_symbol('unistd.h', 'usleep')
    global_args += '-DMKXPZ_NO_USLEEP'
endif

if not compilers['cpp'].has_header_symbol('time.h', 'nanosleep')
    global_args += '-DMKXPZ_NO_NANOSLEEP'
endif

if not compilers['cpp'].has_header('pthread.h') or not compilers['cpp'].links('''
    #include <pthread.h>
    int main(void) {
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        pthread_mutex_init(&mutex, NULL);
        pthread_cond_init(&cond, NULL);
        pthread_cond_signal(&cond);
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&cond, &mutex);
        int result = pthread_self() != pthread_self();
        pthread_mutex_unlock(&mutex);
        pthread_cond_destroy(&cond);
        pthread_mutex_destroy(&mutex);
        return result;
    }
''', args: link_test_args, name: 'pthread.h mutex sanity check')
    if is_devkitarm
        global_args += '-DMKXPZ_DEVKITARM_NO_PTHREAD_H_MUTEX'
    else
        have_pthread_mutex = false
        global_args += '-DMKXPZ_NO_PTHREAD_H_MUTEX'
    endif
endif

if host_system == 'darwin' or not compilers['cpp'].has_header('semaphore.h') or not compilers['cpp'].links('''
    #include <semaphore.h>
    int main(void) {
        sem_t sem;
        sem_init(&sem, 0, 0);
        sem_post(&sem);
        sem_wait(&sem);
        sem_destroy(&sem);
        return 0;
    }
''', args: link_test_args, name: 'semaphore.h sanity check')
    global_args += '-DMKXPZ_NO_SEMAPHORE_H'
endif

if not compilers['cpp'].has_header('dispatch/dispatch.h') or not compilers['cpp'].links('''
    #include <dispatch/dispatch.h>
    int main(void) {
        dispatch_semaphore_t sem;
        sem = dispatch_semaphore_create(0);
        dispatch_semaphore_signal(sem);
        dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
        dispatch_release(sem);
        return 0;
    }
''', args: link_test_args, name: 'dispatch/dispatch.h sanity check')
    global_args += '-DMKXPZ_NO_DISPATCH_DISPATCH_H'
endif

if not compilers['cpp'].has_header('pthread.h') or not compilers['cpp'].links('''
    #include <pthread.h>
    void *func(void *arg) {
        return NULL;
    }
    int main(void) {
        pthread_t thread;
        void *ret;
        pthread_create(&thread, NULL, &func, NULL);
        pthread_join(thread, &ret);
        return ret != NULL;
    }
''', args: link_test_args, name: 'pthread.h thread sanity check')
    if is_devkitarm
        global_args += '-DMKXPZ_DEVKITARM_NO_PTHREAD_H_THREAD'
    else
        have_pthread_thread = false
        global_args += '-DMKXPZ_NO_PTHREAD_H_THREAD'
    endif
endif

mkxp_polyfill_needs_cpp17 = false
if compilers['cpp'].has_header_symbol('stdlib.h', 'posix_memalign') and compilers['cpp'].links('''
    #include <stdlib.h>
    int main(void) {
        void *mem;
        return posix_memalign(&mem, 16, 4);
    }
''', args: link_test_args, name: 'posix_memalign sanity check')
    global_args += '-DMKXPZ_HAVE_POSIX_MEMALIGN'
elif compilers['cpp'].has_header_symbol('malloc.h', '_aligned_malloc') and compilers['cpp'].links('''
    #include <malloc.h>
    int main(void) {
        void *mem = _aligned_malloc(4, 16);
        _aligned_free(mem);
        return mem == NULL;
    }
''', args: link_test_args, name: '_aligned_malloc sanity check')
    global_args += '-DMKXPZ_HAVE_ALIGNED_MALLOC'
elif compilers['cpp'].has_header_symbol('stdlib.h', 'aligned_alloc') and compilers['cpp'].links('''
    #include <stdlib.h>
    int main(void) {
        return aligned_alloc(16, 4) == NULL;
    }
''', args: link_test_args, name: 'aligned_alloc sanity check')
    global_args += '-DMKXPZ_HAVE_ALIGNED_ALLOC'
elif compilers['cpp'].has_header_symbol('cstdlib', 'std::aligned_alloc') and compilers['cpp'].links('''
    #include <cstdlib>
    int main(void) {
        return std::aligned_alloc(16, 4) == nullptr;
    }
''', args: link_test_args + ['-std=c++17'], name: 'std::aligned_alloc sanity check')
    mkxp_polyfill_needs_cpp17 = true
    global_args += '-DMKXPZ_HAVE_STD_ALIGNED_ALLOC'
elif compilers['cpp'].links('''
    #include <new>
    int main(void) {
        return operator new[](4, std::align_val_t(16), std::nothrow_t()) == nullptr;
    }
''', args: link_test_args + ['-std=c++17'], name: 'aligned operator new sanity check')
    mkxp_polyfill_needs_cpp17 = true
    global_args += '-DMKXPZ_HAVE_ALIGNED_OPERATOR_NEW'
else
    error('No aligned memory allocation function found.')
endif

if not compilers['cpp'].has_header_symbol('unistd.h', 'fsync') or not compilers['cpp'].links('''
    #include <unistd.h>
    int main(void) {
        return fsync(1);
    }
''', args: link_test_args, name: 'fsync sanity check')
    global_args += '-DMKXPZ_NO_FSYNC'
endif

if is_orbisdev or not compilers['cpp'].links('''
    #include <ctype.h>
    #undef isascii
    int isascii(int);
    int main(void) {
        return !isascii('a');
    }
''', args: link_test_args, name: 'isascii sanity check')
    global_args += '-DMKXPZ_NO_ISASCII'
endif

if is_orbisdev or not compilers['cpp'].links('''
    #include <string.h>
    void *memccpy(void *, const void *, int, size_t);
    char buffer[4];
    int main(void) {
        return !memccpy(buffer, "abc", 'b', 4);
    }
''', args: link_test_args, name: 'memccpy sanity check')
    global_args += '-DMKXPZ_NO_MEMCCPY'
endif

if host_system == 'darwin'
    disable_availability = false

    if not compilers['cpp'].compiles('''
        #include <optional>
        int main(void) {
            std::optional<int> optional;
            return optional.value();
        }
    ''', args: ['-std=c++17'], name: 'check for broken std::bad_optional_access')
        disable_availability = true
        global_args += '-DMKXPZ_NO_STD_BAD_OPTIONAL_ACCESS'
    endif

    if not compilers['cpp'].compiles('''
        #include <variant>
        int main(void) {
            std::variant<int, float> variant(0.0f);
            return std::get<int>(variant);
        }
    ''', args: ['-std=c++17'], name: 'check for broken std::bad_variant_access')
        disable_availability = true
        global_args += '-DMKXPZ_NO_STD_BAD_VARIANT_ACCESS'
    endif

    if disable_availability
        mkxp_polyfill_needs_cpp17 = true
        global_args += '-D_LIBCPP_DISABLE_AVAILABILITY'
    endif

    if not compilers['cpp'].links('''
        #include <new>
        int main(void) {
            return operator new[](4, std::align_val_t(16), std::nothrow_t()) == nullptr;
        }
    ''', args: link_test_args + ['-std=c++17'], name: 'aligned operator new sanity check')
        mkxp_polyfill_needs_cpp17 = true
        global_args += '-DMKXPZ_DARWIN_NO_ALIGNED_OPERATOR_NEW'
        global_args += '-faligned-allocation'
    endif
endif

if is_devkitppc or not compilers['cpp'].has_header_symbol('time.h', 'clock_gettime') or not compilers['cpp'].links('''
    #include <time.h>
    int main(void) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        return ts.tv_sec == 0 && ts.tv_nsec == 0;
    }
''', args: link_test_args, name: 'clock_gettime sanity check')
    global_args += '-DMKXPZ_NO_CLOCK_GETTIME'
endif

if is_devkitppc or not compilers['cpp'].has_header_symbol('time.h', 'clock_getres') or not compilers['cpp'].links('''
    #include <time.h>
    int main(void) {
        struct timespec ts;
        clock_getres(CLOCK_REALTIME, &ts);
        return ts.tv_sec == 0 && ts.tv_nsec == 0;
    }
''', args: link_test_args, name: 'clock_getres sanity check')
    global_args += '-DMKXPZ_NO_CLOCK_GETRES'
endif

if not compilers['cpp'].has_header_symbol('chrono', 'std::chrono::system_clock::now') or not compilers['cpp'].links('''
    #include <chrono>
    int main(void) {
        std::chrono::time_point<std::chrono::system_clock> now(std::chrono::system_clock::now());
        return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count() == 0 && std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count() == 0;
    }
''', args: link_test_args, name: 'std::chrono::system_clock::now sanity check')
    global_args += '-DMKXPZ_NO_STD_CHRONO_SYSTEM_CLOCK_NOW'
endif

if is_orbisdev or not compilers['cpp'].has_header_symbol('time.h', 'gmtime_r') or not compilers['cpp'].links('''
    #include <time.h>
    int main(void) {
        time_t timer = 0;
        struct tm buf;
        if (gmtime_r(&timer, &buf) == nullptr) {
            return 1;
        }
        return mktime(&buf) != (time_t)-1;
    }
''', args: link_test_args, name: 'gmtime_r sanity check')
    global_args += '-DMKXPZ_NO_GMTIME_R'
endif

if is_orbisdev or not compilers['cpp'].has_header_symbol('time.h', 'gmtime_s') or not compilers['cpp'].links('''
    #include <time.h>
    int main(void) {
        time_t timer = 0;
        struct tm buf;
        if (gmtime_s(&timer, &buf) == nullptr) {
            return 1;
        }
        return mktime(&buf) != (time_t)-1;
    }
''', args: link_test_args, name: 'gmtime_s (standard version) sanity check')
    global_args += '-DMKXPZ_NO_GMTIME_S'
endif

if is_orbisdev or not compilers['cpp'].has_header_symbol('time.h', 'gmtime_s') or not compilers['cpp'].links('''
    #include <time.h>
    int main(void) {
        time_t timer = 0;
        struct tm buf;
        errno_t result = gmtime_s(&buf, &timer);
        if (result != 0) {
            return 1;
        }
        return mktime(&buf) != (time_t)-1;
    }
''', args: link_test_args, name: 'gmtime_s (Windows version) sanity check')
    global_args += '-DMKXPZ_NO_GMTIME_S_WOE32'
endif

# Enable integer-only mode in FLAC when building with Vita SDK. Otherwise, we get an internal compiler error in FLAC__window_triangle:
#
# ../subprojects/flac/src/libFLAC/window.c: In function 'FLAC__window_triangle':
# ../subprojects/flac/src/libFLAC/window.c:197:1: error: unrecognizable insn:
#   197 | }
#       | ^
# (insn 191 190 192 21 (set (reg:V4SF 443)
#         (mult:V4SF (reg:V4SF 443)
#             (reg:V4SF 444))) "../subprojects/flac/src/libFLAC/window.c":189:43 -1
#      (nil))
# during RTL pass: vregs
# ../subprojects/flac/src/libFLAC/window.c:197:1: internal compiler error: in extract_insn, at recog.c:2294
# Please submit a full bug report,
# with preprocessed source if appropriate.
# See <https://gcc.gnu.org/bugs/> for instructions.
if is_vitasdk
    global_args += '-DFLAC__INTEGER_ONLY_LIBRARY'
endif

if is_devkitarm
    global_args += '-D__DEVKITPRO__'
    global_args += '-D__DEVKITARM__'
endif

if is_devkitppc
    global_args += '-D__DEVKITPRO__'
    global_args += '-D__DEVKITPPC__'
endif

if is_libretro
    # We need to statically link the C++ standard library (libstdc++/libc++), the compiler runtime library (libgcc/compiler-rt) and libpthread in MSYS2 libretro builds for Windows because those are not part of the operating system
    if host_system == 'windows'
        global_link_args += ['-static-libgcc', '-static-libstdc++', '-Wl,-Bstatic', '-lgcc', '-lstdc++', '-lpthread', '-Wl,-Bdynamic']
    # Android doesn't have a built-in C++ standard library, so we need to statically link against the C++ standard library
    elif host_system == 'android' or get_option('static_libstdcpp')
        compilers['cpp'].has_link_argument('-static-libstdc++', required: true)
        global_link_args += '-static-libstdc++'
    endif

    # If possible, put all functions and data objects in their own sections to allow the linker to remove dead code
    if host_system != 'windows'
        if compilers['c'].has_argument('-ffunction-sections')
            global_args += '-ffunction-sections'
        endif
        if compilers['c'].has_argument('-fdata-sections')
            global_args += '-fdata-sections'
        endif
    endif
    if not core_is_static and compilers['cpp'].has_link_argument('-Wl,--gc-sections')
        global_link_args += '-Wl,--gc-sections'
    endif

    # If possible, stop the linker from reexporting the symbols from the static libraries we use (e.g. zlib)
    if not core_is_static and compilers['cpp'].has_link_argument('-Wl,--version-script,' + meson.current_source_dir() / 'libretro/link.T') # Only works with GNU linker and LLVM linker
        global_link_args += '-Wl,--version-script,' + meson.current_source_dir() / 'libretro/link.T'
    endif
elif get_option('static_executable')
    if host_system == 'windows'
        # '-static-libgcc', '-static-libstdc++' are here to avoid needing to ship a separate libgcc_s_seh-1.dll on Windows; it still works without those flags if you have the dll.
        global_link_args += ['-static-libgcc', '-static-libstdc++', '-Wl,-Bstatic', '-lgcc', '-lstdc++', '-lpthread', '-Wl,-Bdynamic']
    else
        global_link_args += ['-static-libgcc', '-static-libstdc++']
    endif
    global_args += '-DAL_LIBTYPE_STATIC'
endif

if not have_std_mutex and not have_pthread_mutex
    have_any_mutex = false
endif

if not have_any_mutex or not have_std_thread and not have_pthread_thread
    have_any_thread = false
endif

if not have_any_mutex
    global_args += '-DMKXPZ_NO_MUTEX'
    global_args += '-DMKXPZ_NO_THREADED_AUDIO'
endif

if not have_any_thread
    global_args += '-DMKXPZ_NO_THREAD'
endif

# ====================
# Ext libs
# ====================

if not is_libretro
    # STEAMWORKS
    steamworks = false
    steamworks_path = get_option('steamworks_path')
    if steamworks_path != ''
        libname = 'steam_api'
        if host_system == 'linux'
            if sizeof['void*'] == 4
                bindir = 'linux32'
            else
                bindir = 'linux64'
            endif
        else
            if win64 == true
                bindir = 'win64'
                libname += '64'
            else
                bindir = ''
            endif
        endif

        steam_libpath = steamworks_path + '/redistributable_bin/' + bindir
        steamlib = compilers['cpp'].find_library(libname, required: false, dirs: [steam_libpath])

        if steamlib.found() == true
            global_include_dirs += include_directories('steamshim')
            global_args += '-DMKXPZ_STEAM'
            global_sources += 'steamshim/steamshim_child.c'
            steamworks = true
        endif
    endif

    # GLES
    gfx_backend = get_option('gfx_backend')
    if gfx_backend == 'gles'
        # Needs to be manually set up for now
        global_args += '-DGLES2_HEADER'
    elif gfx_backend == 'gl'
        global_dependencies += dependency('gl')
        # boop
    endif
endif

# ====================
# Main source
# ====================

# Suppress warnings
global_cpp_args += ['-Wno-non-virtual-dtor', '-Wno-reorder']
global_args += ['-Wno-uninitialized', '-Wno-unknown-pragmas']
if is_clang
    global_args += ['-Wno-undefined-var-template', '-Wno-delete-non-abstract-non-virtual-dtor']
else
    global_args += ['-Wno-stringop-truncation']
endif
if host_system == 'windows'
    global_args += '-masm=att'
    if host_cpu_family == 'x86'
        global_args += '-msse'
    endif
endif

if not is_libretro
    # Decide whether or not to use MiniFFI
    miniffi = get_option('use_miniffi')
    if miniffi == true
        miniffi = true
        global_args += '-DMKXPZ_MINIFFI'
    endif

    # Defines
    if get_option('workdir_current')
        global_args += '-DWORKDIR_CURRENT'
    endif

    if get_option('cxx11_experimental') == true
        global_args += '-DMKXPZ_EXP_FS'
    endif

    if get_option('force32') == true
        global_args += '-m32'
    endif

    build_static = false
    if get_option('static_executable') == true
        build_static = true
    endif

    global_args += '-DMKXPZ_INIT_GL_LATER'
endif

subdir('tools')
subdir('src')
subdir(is_libretro ? 'binding-sandbox' : 'binding')
subdir('shader')
subdir('assets')

global_include_dirs += include_directories('src')
if is_libretro
    global_include_dirs += include_directories('binding-sandbox')
endif
global_include_dirs += include_directories('binding')

if core_is_static
    global_dependencies_processed = []
    foreach dep : global_dependencies
        global_dependencies_processed += dep.as_link_whole()
    endforeach
else
    global_dependencies_processed = global_dependencies
endif

global_dependencies += declare_dependency(
    link_with: static_library(
        'mkxp-polyfill',
        dependencies: global_dependencies_processed,
        c_args: global_args,
        cpp_args: global_args + global_cpp_args + (mkxp_polyfill_needs_cpp17 ? ['-std=c++17'] : []),
        include_directories: global_include_dirs,
        sources: [
            'src/mkxp-polyfill.cpp',
        ],
        gnu_symbol_visibility: 'hidden',
        install: false,
        pic: use_pic,
    ),
)

if is_libretro
    libretro_ruby_sources = []
    foreach i : range(import('fs').read(libretro_stage1_path / 'ruby/mkxp-sandbox-ruby-num-outputs.txt').to_int())
        libretro_ruby_sources += libretro_stage1_path / 'ruby/mkxp-sandbox-ruby_@0@.c'.format(i)
    endforeach

    global_dependencies += declare_dependency(
        link_with: static_library(
            'ruby',
            c_args: global_args + [
                '-frounding-math',
                '-Wno-unused-function',
                '-Wno-unused-value',
                '-Wno-unused-variable',
                '-Wno-unused-but-set-variable',
            ] + (is_clang ? [] : ['-fsignaling-nans']),
            include_directories: [
                include_directories('binding-sandbox'),
                include_directories(libretro_stage1_path),
                include_directories(libretro_stage1_path / 'sandbox-bindgen'),
                include_directories(libretro_stage1_path / 'ruby'),
            ],
            sources: libretro_ruby_sources,
            gnu_symbol_visibility: 'hidden',
            install: false,
            pic: use_pic,
        ),
        include_directories: [
            include_directories(libretro_stage1_path),
            include_directories(libretro_stage1_path / 'sandbox-bindgen'),
            include_directories(libretro_stage1_path / 'ruby'),
        ],
    )

    global_sources += libretro_stage1_path / 'sandbox-bindgen/mkxp-sandbox-bindgen.cpp'

    global_sources += custom_target(
        'dist-zip',
        input: libretro_stage1_path / 'dist.zip',
        output: 'dist.zip.cpp',
        command: [
            embedtool,
            '@INPUT@',
            '@OUTPUT@',
            'dist_zip',
        ],
    )

    global_sources += custom_target(
        'preload-zip',
        input: libretro_stage1_path / 'preload.zip',
        output: 'preload.zip.cpp',
        command: [
            embedtool,
            '@INPUT@',
            '@OUTPUT@',
            'preload_zip',
        ],
    )

    if core_is_static
        global_dependencies_processed = []
        foreach dep : global_dependencies
            global_dependencies_processed += dep.as_link_whole()
        endforeach
    else
        global_dependencies_processed = global_dependencies
    endif

    libretro = build_target(
        meson.project_name() + '_libretro',
        name_prefix: '',
        target_type: core_is_static ? 'static_library' : 'shared_library',
        dependencies: global_dependencies_processed,
        c_args: global_args,
        cpp_args: global_args + global_cpp_args,
        link_args: global_link_args,
        pic: use_pic,
        gnu_symbol_visibility: 'hidden',
        install: true, # Prevents Meson from creating thin archives; see https://github.com/mesonbuild/meson/issues/9479
        include_directories: global_include_dirs,
        sources: global_sources,
    )
else
    rpath = ''

    if host_system == 'windows'
        windows_resource_directory = '../' + get_option('windows_resource_directory')
        subdir('windows')
        global_sources += windows_resources
        global_include_dirs += include_directories('windows')
    else
        subdir('linux')
        rpath = '$ORIGIN/lib'
        if get_option('appimage') != true
            if sizeof['long'] == 8 and get_option('force32') != true
                rpath += '64'
            else
                rpath += '32'
            endif
        endif
    endif

    exe_name = meson.project_name()

    if host_system == 'linux' and get_option('appimage') == false
        exe_name += '.' + host_machine.cpu()
    endif

    if steamworks == true
        exe_name = 'steam_' + exe_name
        la = ''
        if build_static == true
            if host_system == 'windows'
                la = '-static'
            else
                la = '-static-libgcc -static-libstdc++'
            endif
        endif

        shim_args = [
            '-DGAME_LAUNCH_NAME="' + exe_name + '"',
            '-I' + steamworks_path + '/public'
            ]

        if get_option('steam_appid') != ''
            shim_args += '-DSTEAM_APPID=' + get_option('steam_appid')
        endif

        if get_option('steamshim_debug') == true
            shim_args += '-DSTEAMSHIM_DEBUG'
            shim_ws = 'console'
        else
            shim_ws = 'windows'
        endif

        executable(meson.project_name(),
            sources: files('steamshim/steamshim_parent.cpp'),
            dependencies: steamlib,
            cpp_args: shim_args,
            link_args: la.split(),
            win_subsystem: shim_ws,
            install: (host_system != 'windows'))
    endif

    executable(exe_name,
        sources: global_sources,
        dependencies: global_dependencies,
        include_directories: global_include_dirs,
        install_rpath: rpath,
        link_args: global_link_args,
        cpp_args: global_args + global_cpp_args,
        objc_args: global_args,
        objcpp_args: global_args + global_cpp_args,
        win_subsystem: 'windows',
        install: (host_system != 'windows')
    )
endif
