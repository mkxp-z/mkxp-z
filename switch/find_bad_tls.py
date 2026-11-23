#!/usr/bin/env python3
"""Find devkitA64 -fPIC/-mtp=soft TLS miscompiles in an AArch64 ELF disassembly.

Pattern: bl __aarch64_read_tp followed by `lsl xN, x0, #1` (should be add xN, x0, #off).
"""
import re
import sys
from collections import Counter, defaultdict

path = sys.argv[1]
func_re = re.compile(r'^([0-9a-f]+) <(.+)>:')
insn_re = re.compile(r'^\s*([0-9a-f]+):\s+[0-9a-f]+\s+(\S+)\s*(.*)$')
tp_re = re.compile(r'bl\s+[0-9a-f]+ <__aarch64_read_tp>')
bad_re = re.compile(r'^lsl\s+x\d+,\s*x0,\s*#1$')

counts = Counter()
samples = defaultdict(list)
total_tp = 0
current = '?'
pending = 0

with open(path, 'r', errors='replace') as f:
    for line in f:
        m = func_re.match(line)
        if m:
            current = m.group(2)
            pending = 0
            continue
        im = insn_re.match(line)
        if not im:
            continue
        op = im.group(2)
        operands = im.group(3).strip()
        if op == 'bl' and tp_re.search(line):
            total_tp += 1
            pending = 4
            continue
        if pending > 0:
            if bad_re.match(op + ' ' + operands):
                counts[current] += 1
                if len(samples[current]) < 3:
                    samples[current].append(im.group(1))
            pending -= 1

print(f'thread-pointer calls: {total_tp}')
print(f'functions with bad lsl-after-read_tp: {len(counts)}')
for name, n in counts.most_common():
    print(f'{n:5d}  {name}  e.g. {", ".join(samples[name])}')

sys.exit(1 if counts else 0)
