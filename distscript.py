import configparser
import json
import os
import shlex
import shutil
import subprocess
import sys

dist_subprojects = sys.argv[1]
vcs_command = sys.argv[2:]

meson = shlex.split(os.environ['MESONREWRITE'])[:-1]

# Make sure all subprojects are downloaded, even ones that are not used in the current build configuration
# (We need to download subprojects even if dist_subprojects != 'all' since they could be downloaded in a nonstandard location if the MESON_PACKAGE_CACHE_DIR environment variable was set)
subprocess.run(meson + ['subprojects', 'download'], cwd=os.environ['MESON_SOURCE_ROOT'], check=True, env={**os.environ, 'MESON_PACKAGE_CACHE_DIR': ''})

needed_subproject_names = {subproject['name'] for subproject in json.loads(subprocess.run(meson + ['introspect', '--projectinfo'], cwd=os.environ['MESON_BUILD_ROOT'], check=True, stdout=subprocess.PIPE).stdout.decode('utf-8'))['subprojects']}

# Write the current Git hash into git-hash in the release artifact if it isn't already in git-hash
if len(vcs_command) > 0:
    shutil.copy2(os.path.join(os.environ['MESON_SOURCE_ROOT'], 'git-hash'), os.environ['MESON_DIST_ROOT'])
    with open(os.path.join(os.environ['MESON_DIST_ROOT'], 'git-hash'), 'ab') as f:
        f.write(subprocess.run(vcs_command, cwd=os.environ['MESON_SOURCE_ROOT'], check=True, stdout=subprocess.PIPE).stdout)

# Copy each wrap-file tarball and wrap-git source repository into the release artifact
os.makedirs(os.path.join(os.environ['MESON_DIST_ROOT'], 'subprojects', 'packagecache'), exist_ok=True)
for filename in os.listdir(os.path.join(os.environ['MESON_SOURCE_ROOT'], 'subprojects')):
    if not filename.endswith('.wrap'):
        continue
    subproject_name = filename[:-5]

    if dist_subprojects == 'all':
        pass
    elif dist_subprojects == 'needed':
        if subproject_name not in needed_subproject_names:
            continue
    else:
        continue

    config = configparser.ConfigParser()
    config.read(os.path.join(os.environ['MESON_SOURCE_ROOT'], 'subprojects', filename))
    if 'wrap-file' in config:
        shutil.copy2(os.path.join(os.environ['MESON_SOURCE_ROOT'], 'subprojects', 'packagecache', config['wrap-file']['source_filename']), os.path.join(os.environ['MESON_DIST_ROOT'], 'subprojects', 'packagecache'))
        if 'patch_filename' in config['wrap-file']:
            shutil.copy2(os.path.join(os.environ['MESON_SOURCE_ROOT'], 'subprojects', 'packagecache', config['wrap-file']['patch_filename']), os.path.join(os.environ['MESON_DIST_ROOT'], 'subprojects', 'packagecache'))
    elif 'wrap-redirect' not in config:
        shutil.copytree(os.path.join(os.environ['MESON_SOURCE_ROOT'], 'subprojects', subproject_name), os.path.join(os.environ['MESON_DIST_ROOT'], 'subprojects', subproject_name))
        for excluded_dir in ('.git', '.hg', '.svn'):
            if os.path.isdir(os.path.join(os.environ['MESON_DIST_ROOT'], 'subprojects', subproject_name, excluded_dir)):
                shutil.rmtree(os.path.join(os.environ['MESON_DIST_ROOT'], 'subprojects', subproject_name, excluded_dir))
