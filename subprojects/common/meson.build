project('common', 'c', 'cpp', meson_version: '>=1.4.0')

python = import('python').find_installation(required: true)

install_directory = [python, files('install-directory.py')[0]]

ar = meson.version().version_compare('>=1.10.0') ? find_program('ar').cmd_array() : [find_program('ar')]
ranlib = ar + 's'
if host_machine.system() in ['windows', 'cygwin']
    windres = meson.version().version_compare('>=1.10.0') ? find_program('windres').cmd_array() : [find_program('windres')]
else
    windres = [python, files('windres-dummy.py')[0]]
endif
if build_machine.system() in ['windows', 'cygwin']
    cygpath = meson.version().version_compare('>=1.10.0') ? find_program('cygpath').cmd_array() : [find_program('cygpath')]
else
    cygpath = [python, files('cygpath-dummy.py')[0]]
endif

# embedtool SOURCE_FILE DESTINATION_FILE NAME
#   Embeds the contents of a file into a C array.
#   SOURCE_FILE: Path to the file to embed.
#   DESTINATION_FILE: Path where the C source file containing the array with the contents of SOURCE_FILE should be written to.
#   NAME: The output C source file will contain an array of uint8_t named NAME containing the contents of SOURCE_FILE and a size_t named NAME_len containing the length of NAME in bytes.
embedtool = [python, files('embedtool.py')[0]]

# quotetool [ARGS...]
#   Outputs a shell-escaped string containing the given arguments, so that shell splitting the string output by this command yields the original arguments. For example, if the arguments are `hello` and `foo bar` and `foo 'bar'`, the output will be `hello 'foo bar' 'foo '"'"'bar'"'"''`.
quotetool = [python, files('quotetool.py')[0]]

# sonametool LIBRARY_NAME
#   Returns the full name of the shared library file for the given library on the host machine. For example, given "X11", this outputs "libX11.so.6" when building for BSD or Linux.
sonametool = [python, files('sonametool.py')[0], host_machine.system()]

host_triplet_cpu = host_machine.cpu()
host_triplet_vendor = 'unknown'
if host_machine.system() in ['windows', 'cygwin']
    host_triplet_os = 'mingw64'
else
    host_triplet_os = host_machine.system()
endif
host_triplet = host_triplet_cpu + '-' + host_triplet_vendor + '-' + host_triplet_os

base_cflags = []
base_cppflags = []
base_ldflags = []

c_std = []
foreach std : get_option('c_std').split(',')
    c_std += '-std=' + std
endforeach
c_std = meson.get_compiler('c').first_supported_argument(c_std)
if c_std.length() == 0
    error('failed to find a supported C standard')
endif
c_std = c_std[0]
base_cflags += c_std

cpp_std = []
foreach std : get_option('cpp_std').split(',')
    cpp_std += '-std=' + std
endforeach
cpp_std = meson.get_compiler('cpp').first_supported_argument(cpp_std)
if cpp_std.length() == 0
    error('failed to find a supported C++ standard')
endif
cpp_std = cpp_std[0]
base_cppflags += cpp_std

c_linker_flags = meson.get_compiler('c').first_supported_link_argument(
    '-fuse-ld=' + meson.get_compiler('c').get_linker_id().split('.')[-1],
)
base_ldflags += c_linker_flags

c_linker_is_gnu = meson.get_compiler('c').get_linker_id() in [
    'ld.bfd',
    'ld.eld',
    'ld.gold',
    'ld.lld',
    'ld.mold',
    'ld.wild',
    'ld.zigcc',
]

# ld64 (the linker used on Apple platforms) emits linker warnings that cause Ruby to fail to configure, let's turn them off
if meson.get_compiler('c').get_linker_id().split('.')[0] == 'ld64'
    base_ldflags += meson.get_compiler('c').first_supported_link_argument('-Wl,-w')
endif

if get_option('optimization') == '0'
    base_cflags += meson.get_compiler('c').first_supported_argument('-O0')
    base_cppflags += meson.get_compiler('cpp').first_supported_argument('-O0')
elif get_option('optimization') == 'g'
    base_cflags += meson.get_compiler('c').first_supported_argument('-Og')
    base_cppflags += meson.get_compiler('cpp').first_supported_argument('-Og')
elif get_option('optimization') == '1'
    base_cflags += meson.get_compiler('c').first_supported_argument('-O1')
    base_cppflags += meson.get_compiler('cpp').first_supported_argument('-O1')
elif get_option('optimization') == '2'
    base_cflags += meson.get_compiler('c').first_supported_argument('-O2')
    base_cppflags += meson.get_compiler('cpp').first_supported_argument('-O2')
elif get_option('optimization') == '3'
    base_cflags += meson.get_compiler('c').first_supported_argument('-O3')
    base_cppflags += meson.get_compiler('cpp').first_supported_argument('-O3')
elif get_option('optimization') == 's'
    base_cflags += meson.get_compiler('c').first_supported_argument('-Os')
    base_cppflags += meson.get_compiler('cpp').first_supported_argument('-Os')
endif

if get_option('debug')
    base_cflags += meson.get_compiler('c').first_supported_argument('-g')
    base_cppflags += meson.get_compiler('cpp').first_supported_argument('-g')
endif

if get_option('b_staticpic')
    base_cflags += meson.get_compiler('c').first_supported_argument('-fPIC')
    base_cppflags += meson.get_compiler('cpp').first_supported_argument('-fPIC')
endif

if get_option('b_lto')
    base_cflags += meson.get_compiler('c').first_supported_argument(
        get_option('b_lto_mode') == 'thin' ? '-flto=thin' : [],
        '-flto=auto',
        '-flto',
    )
    base_cppflags += meson.get_compiler('cpp').first_supported_argument(
        get_option('b_lto_mode') == 'thin' ? '-flto=thin' : [],
        '-flto=auto',
        '-flto',
    )
    base_ldflags += meson.get_compiler('c').first_supported_link_argument(
        get_option('b_lto_mode') == 'thin' ? '-flto=thin' : [],
        '-flto=auto',
        '-flto',
    )
endif

if get_option('b_sanitize') != 'none'
    base_cflags += meson.get_compiler('c').first_supported_argument('-fsanitize=' + get_option('b_sanitize'))
    base_cppflags += meson.get_compiler('cpp').first_supported_argument('-fsanitize=' + get_option('b_sanitize'))
    base_ldflags += meson.get_compiler('c').first_supported_link_argument('-fsanitize=' + get_option('b_sanitize'))
    if 'address' in get_option('b_sanitize').split(',')
        base_cflags += meson.get_compiler('c').first_supported_argument('-fno-omit-frame-pointer')
        base_cppflags += meson.get_compiler('cpp').first_supported_argument('-fno-omit-frame-pointer')
    endif
endif

if get_option('b_ndebug') == 'true' or (get_option('b_ndebug') == 'if-release' and get_option('buildtype') in ['plain', 'release'])
    base_cflags += meson.get_compiler('c').first_supported_argument('-DNDEBUG')
    base_cppflags += meson.get_compiler('cpp').first_supported_argument('-DNDEBUG')
endif

base_cflags += meson.get_compiler('c').first_supported_argument('-fdiagnostics-color=' + get_option('b_colorout'))
base_cppflags += meson.get_compiler('cpp').first_supported_argument('-fdiagnostics-color=' + get_option('b_colorout'))
