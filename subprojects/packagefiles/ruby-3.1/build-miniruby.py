import os
import shlex
import stat
import sys

ruby = sys.argv[1:-2]
srcdir = sys.argv[-2]
output_path = sys.argv[-1]

fake = next(filename for filename in os.listdir(path=srcdir) if filename.endswith('-fake.rb'))

with open(output_path, 'w') as output_file:
    output_file.write('#!/bin/sh\n')
    output_file.write('set -e\n')
    output_file.write('exec ' + ' '.join(map(shlex.quote, ruby + ['--disable=gems', '-I' + srcdir, '-r' + fake])) + ' "$@"\n')

os.chmod(output_path, os.stat(output_path).st_mode | stat.S_IEXEC)
