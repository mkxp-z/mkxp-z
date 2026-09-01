import os
import shlex
import shutil
import subprocess
import sys

python = sys.argv[1:-5]
c_client = os.path.realpath(sys.argv[-5])
private_dir = sys.argv[-4]
outdir = os.path.realpath(sys.argv[-3])
root = sys.argv[-2]
name = sys.argv[-1]

os.chdir(private_dir)

command = python + [c_client, '-c', 'libxcb', '-l', 'X Version 11', '-s', '3', '-p', root, os.path.join(root, 'src', name + '.xml')]
print('    Running command: ' + ' '.join(map(shlex.quote, command)), file=sys.stderr)
subprocess.run(command, check=True)

shutil.move(name + '.h', os.path.join(outdir, name + '.h'))
