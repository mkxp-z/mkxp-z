# This script is used for listing all of the mirrors of a FreeBSD package repository,
# with the proper priority (mirrors that should be tried earlier are listed before ones that should be tried later).
# Licensed under CC0.

import random
import re
import subprocess
import sys
import typing


protocol = sys.argv[1] # "http", "https", "ftp", etc.
master_host = sys.argv[2] # "pkg.freebsd.org" or a third-party package repository's hostname
pattern = re.compile(r'([0-9]+) ([0-9]+) ([0-9]+) ([^ ]+)\.')
entries_by_priority = {}


# Fenwick tree, as described in "A new data structure for cumulative frequency tables" by Peter M. Fenwick (https://doi.org/10.1002/spe.4380240306).
# The three methods in this class excluding __init__ are taken from figure 4, figure 5 and figure 8, respectively.
class FenwickTree:
    _tree: typing.List[int]

    # Creates a new Fenwick tree containing n items, all of which are initially zero.
    def __init__(self, n: int) -> None:
        self._tree = [0] * n

    # Returns the sum of all items up to and including the given index (zero-based) in O(log n) time.
    def cumsum(self, index: int) -> int:
        cumsum = 0
        while index >= 0:
            cumsum += self._tree[index]
            index &= index + 1
            index -= 1
        return cumsum

    # Sets the item at the given index (zero-based) to the given value in O(log n) time.
    def set(self, index: int, value: int) -> None:
        delta = value - (self.cumsum(index) if index == 0 else self.cumsum(index) - self.cumsum(index - 1))
        while index < len(self._tree):
            self._tree[index] += delta
            index |= index + 1

    # Returns the smallest nonnegative integer less than n such that self.cumsum(self.cumsum_bisect_right(target_cumsum)) is greater than target_cumsum, in O(log n) time.
    # If there is no such integer, returns n instead. All items must be nonnegative, otherwise the behavior is undefined.
    def cumsum_bisect_right(self, target_cumsum: int) -> int:
        rank = 0
        cumsum = 0
        power_of_2 = 1 << (len(self._tree).bit_length() - 1)
        while power_of_2 > 0:
            index = (rank | power_of_2) - 1
            if index < len(self._tree) and cumsum + self._tree[index] <= target_cumsum:
                cumsum += self._tree[index]
                rank |= power_of_2
            power_of_2 >>= 1
        return rank


# Find all of the mirrors of the master repository using a DNS SRV request.
for raw_entry in subprocess.run(['dig', '+short', '_' + protocol + '._tcp.' + master_host, 'SRV'], check=True, stdout=subprocess.PIPE).stdout.decode('utf-8').splitlines():
    match = pattern.fullmatch(raw_entry)
    if match is None:
        continue
    priority = int(match[1])
    weight = int(match[2])
    port = int(match[3])
    hostname = match[4]
    entries_by_priority.setdefault(priority, []).append({'weight': weight, 'port': port, 'hostname': hostname})

# Sort SRV entries with different priorities in ascending order of priority.
for priority in sorted(entries_by_priority.keys()):
    entries = entries_by_priority[priority]
    weights = FenwickTree(len(entries))
    for i, entry in enumerate(entries):
        weights.set(i, entry['weight'])
    # Perform a weighted shuffle on SRV entries with the same priority (higher weight is more likely to be chosen earlier).
    # The algorithm used here is a variation of the Fisher-Yates shuffle, where the item chosen in each iteration of the loop is chosen based on its weight instead of being chosen uniformly.
    for pivot_index in range(len(entries)):
        weights_total = weights.cumsum(len(entries) - 1)
        random_index = random.randrange(pivot_index, len(entries)) if weights_total == 0 else weights.cumsum_bisect_right(random.randrange(weights_total))
        print(protocol + '://' + entries[random_index]['hostname'] + ':' + str(entries[random_index]['port']))
        entries[random_index] = entries[pivot_index]
        weights.set(random_index, entries[random_index]['weight'])
        weights.set(pivot_index, 0)

# Print the master repository URL at the end.
print(sys.argv[1] + '://' + master_host)
