import os
import shutil
import sys

srcdir = sys.argv[1]
destdir = os.path.join(os.getenv('MESON_INSTALL_DESTDIR_PREFIX'), sys.argv[2])

shutil.copytree(srcdir, destdir, dirs_exist_ok=True)
