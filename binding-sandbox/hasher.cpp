/*
** hasher.cpp
**
** This file is part of mkxp.
**
** Copyright (C) 2025 - 2026 The mkxp-z authors
**
** mkxp is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 of the License, or
** (at your option) any later version.
**
** mkxp is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>
#include <picosha2.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "[hasher] error: at least one argument must be passed to this program" << std::endl;
        return 1;
    }

    picosha2::hash256_one_by_one digest;

    for (int i = 2; i < argc; ++i) {
        std::ifstream file(argv[i], std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "[hasher] error: could not open input file " << argv[i] << std::endl;
            return 2;
        }
        std::vector<uint8_t> buffer;
        for (;;) {
            uint8_t byte = file.get();
            if (file.eof()) {
                break;
            }
            buffer.push_back(byte);
        }
        digest.process(buffer.begin(), buffer.end());
    }

    digest.finish();
    std::vector<uint8_t> bytes(picosha2::k_digest_size);
    digest.get_hash_bytes(bytes.begin(), bytes.end());

    std::ofstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "[hasher] error: could not open output file " << argv[1] << std::endl;
        return 3;
    }
    file << "#ifndef MKXPZ_BINDING_SANDBOX_HASH" << std::endl;
    file << "#  define MKXPZ_BINDING_SANDBOX_HASH \"" << std::hex;
    for (uint8_t byte : bytes) {
        file << "\\x" << (unsigned int)byte;
    }
    file << '"' << std::endl << "#endif" << std::endl;

    return 0;
}
