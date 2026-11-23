import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import time

exe_path = os.path.realpath(sys.argv[1])
test_name = sys.argv[2]
templates = [template for template in (sys.argv[3] if len(sys.argv) > 3 else '').split(';') if template != '']

src_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
build_dir = os.path.dirname(exe_path)
exe_name = os.path.basename(exe_path)
script_dir = os.path.join(src_dir, 'scripts')
test_dir = os.path.join(src_dir, 'tests', test_name.replace('/', os.sep))

with tempfile.TemporaryDirectory(dir=build_dir) as test_harness_dir:
    test_harness_subdir = os.path.join(test_harness_dir, 'test')

    # Copy the test itself
    shutil.copytree(test_dir, test_harness_subdir)

    # Copy the preload and postload scripts into the test directory
    shutil.copytree(script_dir, os.path.join(test_harness_subdir, 'scripts'))

    # Disable audio when running the test
    for filename in ('alsoft.conf', 'alsoft.ini'):
        with open(os.path.join(test_harness_subdir, filename), 'w') as alsoft_conf:
            alsoft_conf.write('[General]\n')
            alsoft_conf.write('drivers = null\n')

    # Copy any dynamic libraries the executable depends on into the test directory, and also any .local files (for Windows DLL redirection; see https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-redirection)
    for entry in os.scandir(build_dir):
        if entry.is_file() and any(entry.name.endswith(suffix) for suffix in ('.bundle', '.dll', '.dylib', '.local', '.so')):
            shutil.copy2(entry.path, os.path.join(test_harness_subdir, entry.name), follow_symlinks=False) # `follow_symlinks=False` makes it so that if the source file is a symbolic link, the created destination file is also a symbolic link instead of a copy of the file that the source file refers to; this is important for dynamic libraries to work correctly

    # Copy the executable into the test directory
    shutil.copy2(exe_path, os.path.join(test_harness_subdir, exe_name))

    # Perform templating on mkxp.json
    if len(templates) > 0:
        with open(os.path.join(test_harness_subdir, 'mkxp.json'), 'r') as config_file:
            config = config_file.read()
        for template in templates:
            key, value = template.split('=', 1)
            config = config.replace('@' + key + '@', value)
        with open(os.path.join(test_harness_subdir, 'mkxp.json'), 'w') as config_file:
            config_file.write(config)

    try:
        # Run the test
        result = subprocess.run(
            (shlex.split(os.environ['MESON_EXE_WRAPPER']) if 'MESON_EXE_WRAPPER' in os.environ else ()) + (
                os.path.join(test_harness_subdir, exe_name),
            ),
            cwd=test_harness_subdir,
            capture_output=True,
        )
    finally:
        # On Windows, attempting to delete the temporary directory that the test is in may result in a permission error from the executable still being in use, so wait until the executable is no longer in use
        while True:
            try:
                os.remove(os.path.join(test_harness_subdir, exe_name))
                break
            except PermissionError:
                pass
            except:
                break
            time.sleep(0.25)

    sys.stdout.buffer.write(result.stdout)
    sys.stderr.buffer.write(result.stderr)
    exit(result.returncode)
