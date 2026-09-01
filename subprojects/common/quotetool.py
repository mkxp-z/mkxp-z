import shlex
import sys

argv = sys.argv[1:]

print(' '.join(shlex.quote(arg.replace('\\', '/')) for arg in argv))
