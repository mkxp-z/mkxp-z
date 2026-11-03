# make_nro.py ELF2NRO ELF NACP ICON OUTPUT STAGING ROMFS_DIR PRELOAD...
#
# elf2nro takes a single RomFS directory, so copy ROMFS_DIR into STAGING with
# the extra PRELOAD scripts in its preload/ folder, then run elf2nro.
import os
import shutil
import subprocess
import sys

elf2nro, elf, nacp, icon, output, staging, romfs_dir = sys.argv[1:8]
romfs = os.path.join(staging, 'romfs')

shutil.rmtree(romfs, ignore_errors=True)
shutil.copytree(romfs_dir, romfs)
for script in sys.argv[8:]:
    shutil.copy(script, os.path.join(romfs, 'preload'))

subprocess.run([elf2nro, elf, output, '--icon=' + icon, '--nacp=' + nacp, '--romfsdir=' + romfs], check=True)
