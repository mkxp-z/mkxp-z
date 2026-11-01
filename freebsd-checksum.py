# This script is used to verify the integrity of a FreeBSD package using a FreeBSD package checksum.
# Licensed under CC0.

import base64
import hashlib
import sys
import textwrap

expected_checksum = sys.argv[1]
filename = sys.argv[2]


def encoding_hex(checksum: bytes) -> str:
    return checksum.hex()


def encoding_zbase32(checksum: bytes) -> str:
    checksum = base64.b32encode(checksum).decode()

    # Remove padding
    checksum = checksum.rstrip('=')

    # Convert to z-base-32 alphabet
    checksum = checksum.translate(str.maketrans('ABCDEFGHIJKLMNOPQRSTUVWXYZ234567', 'ybndrfg8ejkmcpqxot1uwisza345h769'))

    return checksum


def encoding_zbase32_freebsd(checksum: bytes) -> str:
    # Reverse the bits in each byte of the checksum, then convert to a binary string
    checksum = ''.join('{:08b}'.format(byte)[::-1] for byte in checksum)

    # Pad the binary string at the end with zeros to make its length a multiple of both 5 and 8 (i.e. of LCM(5, 8) = 40)
    padding_bits = -(len(checksum) // -40) * 40 - len(checksum)
    checksum += '0' * padding_bits

    # Split the binary string into groups of 5 digits
    checksum = textwrap.wrap(checksum, 5)

    # Reverse the digits in each group of 5 digits
    checksum = (bits[::-1] for bits in checksum)

    # Join them back together
    checksum = ''.join(checksum)

    # Split the binary string into groups of 8 digits
    checksum = textwrap.wrap(checksum, 8)

    # Convert back into bytes
    checksum = bytes(int(byte, 2) for byte in checksum)

    # Convert to z-base-32
    checksum = encoding_zbase32(checksum)

    # Remove the padding we added previously
    padding_characters = padding_bits // 5
    assert all(character == 'y' for character in checksum[-padding_characters:])
    checksum = checksum[:-padding_characters]

    return checksum


checksum_format_dict = {
    '0': {'digest': 'sha256', 'encoding': encoding_zbase32_freebsd},
    '1': {'digest': 'sha256', 'encoding': encoding_hex},
    '2': {'digest': 'blake2b', 'encoding': encoding_zbase32_freebsd},
    '5': {'digest': 'blake2s', 'encoding': encoding_zbase32_freebsd},
    '7': {'digest': 'sha512', 'encoding': encoding_zbase32_freebsd},
    '8': {'digest': 'sha512', 'encoding': encoding_hex},
    '10': {'digest': 'blake2b', 'encoding': encoding_hex},
    '11': {'digest': 'blake2s', 'encoding': encoding_hex},
    '12': {'digest': 'sha256', 'encoding': encoding_zbase32},
    '13': {'digest': 'blake2b', 'encoding': encoding_zbase32},
    '14': {'digest': 'blake2s', 'encoding': encoding_zbase32},
}

checksum_format_string = expected_checksum.split('$', 1)[0]
checksum_format = checksum_format_dict.get(checksum_format_string)
if checksum_format is None:
    checksum_format_string = None
    checksum_format = checksum_format_dict['1']

with open(filename, 'rb') as f:
    checksum = hashlib.file_digest(f, checksum_format['digest']).digest()

checksum = checksum_format['encoding'](checksum)

if checksum_format_string is not None:
    checksum = checksum_format_string + '$' + checksum

if checksum == expected_checksum:
    print(filename + ': OK', file=sys.stderr)
else:
    print(filename + ': FAILED', file=sys.stderr)
    print('Expected: ' + expected_checksum, file=sys.stderr)
    print('     Got: ' + checksum, file=sys.stderr)
    exit(1)
