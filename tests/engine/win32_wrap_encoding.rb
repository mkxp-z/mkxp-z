# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# Text conversion in the Win32API stand-ins (scripts/preload/
# win32_wrap.rb and win32_wrap_encoding.rb), on each bundled
# interpreter. The engine loads win32_wrap_encoding.rb only on Ruby 1.9
# and 3.1, so Ruby 1.8 runs without the Shift_JIS check.
#
#   tools/run-engine-tests.sh --suite win32_wrap_encoding.rb --ruby 18
#
# This file must parse on all three.

unless defined?(EngineTest)
  harness_path = File.join(File.dirname(__FILE__), 'harness.rb')
  eval(File.read(harness_path), TOPLEVEL_BINDING, harness_path) # rubocop:disable Security/Eval
end

CODE_PAGES = defined?(Encoding)

EngineTest.suite('win32-wrap-encoding', CODE_PAGES ? 11 : 10)

preloads = %w[windows_fs platform_compat pokemon_compat win32_wrap]
preloads << 'win32_wrap_encoding' if CODE_PAGES
EngineTest.load_preloads(preloads)

MULTI_BYTE_TO_WIDE_CHAR = Win32API.new('kernel32', 'MultiByteToWideChar', 'iipipi', 'i')
WIDE_CHAR_TO_MULTI_BYTE = Win32API.new('kernel32', 'WideCharToMultiByte', 'iipipipp', 'i')

def byte_list(text)
  text.unpack('C*')
end

EngineTest.test('MultiByteToWideChar measures a null-terminated string') do
  count = MULTI_BYTE_TO_WIDE_CHAR.call(65_001, 0, "abc\0xyz", -1, nil, 0)
  EngineTest.assert_equal(4, count, 'UTF-16 code units with the null')
end

EngineTest.test('MultiByteToWideChar reads to the end of a string with no null') do
  count = MULTI_BYTE_TO_WIDE_CHAR.call(65_001, 0, 'abc', -1, nil, 0)
  EngineTest.assert_equal(3, count, 'UTF-16 code units')
end

EngineTest.test('MultiByteToWideChar writes UTF-16') do
  out = "\0" * 8
  count = MULTI_BYTE_TO_WIDE_CHAR.call(65_001, 0, 'hi', 2, out, 4)
  EngineTest.assert_equal(2, count, 'code units written')
  EngineTest.assert_equal([0x68, 0, 0x69, 0, 0, 0, 0, 0], byte_list(out), 'bytes written')
end

EngineTest.test('MultiByteToWideChar reads UTF-8') do
  out = "\0" * 4
  count = MULTI_BYTE_TO_WIDE_CHAR.call(65_001, 0, [0xC3, 0xA9].pack('C*'), 2, out, 2)
  EngineTest.assert_equal(1, count, 'code units written')
  EngineTest.assert_equal([0xE9, 0, 0, 0], byte_list(out), 'bytes written')
end

EngineTest.test('MultiByteToWideChar replaces only the bad bytes') do
  out = "\0" * 10
  count = MULTI_BYTE_TO_WIDE_CHAR.call(65_001, 0, [0xC3, 0xA9, 0xFF, 0x41, 0x80, 0xC3].pack('C*'), 6, out, 5)
  EngineTest.assert_equal(5, count, 'code units written')
  EngineTest.assert_equal([0xE9, 0xFFFD, 0x41, 0xFFFD, 0xFFFD], out.unpack('v*'), 'code units')
end

EngineTest.test('WideCharToMultiByte writes UTF-8') do
  out = "\0" * 8
  count = WIDE_CHAR_TO_MULTI_BYTE.call(65_001, 0, [0x68, 0xE9, 0].pack('v*'), -1, out, 8, nil, nil)
  EngineTest.assert_equal(4, count, 'bytes written with the null')
  EngineTest.assert_equal([0x68, 0xC3, 0xA9, 0], byte_list(out)[0, 4], 'bytes')
end

EngineTest.test('WideCharToMultiByte cuts the text to a small buffer') do
  out = "\0" * 2
  count = WIDE_CHAR_TO_MULTI_BYTE.call(65_001, 0, [0x61, 0x62, 0x63].pack('v*'), 3, out, 2, nil, nil)
  EngineTest.assert_equal(2, count, 'bytes written')
  EngineTest.assert_equal([0x61, 0x62], byte_list(out), 'bytes')
end

EngineTest.test('WideCharToMultiByte joins a surrogate pair') do
  count = WIDE_CHAR_TO_MULTI_BYTE.call(65_001, 0, [0xD83D, 0xDE00].pack('v*'), 2, nil, 0, nil, nil)
  out = "\0" * count
  WIDE_CHAR_TO_MULTI_BYTE.call(65_001, 0, [0xD83D, 0xDE00].pack('v*'), 2, out, count, nil, nil)
  EngineTest.assert_equal([0xF0, 0x9F, 0x98, 0x80], byte_list(out), 'bytes')
end

EngineTest.test('WideCharToMultiByte writes bytes into a buffer with UTF-8 text') do
  out = [0xC3, 0xA9, 0xC3, 0xA9].pack('C*')
  out.force_encoding('UTF-8') if CODE_PAGES
  WIDE_CHAR_TO_MULTI_BYTE.call(65_001, 0, [0x68].pack('v*'), 1, out, 1, nil, nil)
  EngineTest.assert_equal([0x68, 0xA9, 0xC3, 0xA9], byte_list(out), 'bytes')
end

# Zeus Video Player (Vinemon's Scene_Movie3) converts each video command
# and its answer in two calls: one to measure, one to convert. It cuts
# the answer at its null.
def zeus_utf8_to_utf16(text)
  size = MULTI_BYTE_TO_WIDE_CHAR.call(65_001, 0, text, text.length, nil, 0)
  out = "\0" * (size * 2)
  MULTI_BYTE_TO_WIDE_CHAR.call(65_001, 0, text, text.length, out, size)
  out
end

def zeus_utf16_to_utf8(text)
  size = WIDE_CHAR_TO_MULTI_BYTE.call(65_001, 0, text, text.length / 2, nil, 0, nil, nil)
  out = "\0" * size
  WIDE_CHAR_TO_MULTI_BYTE.call(65_001, 0, text, text.length / 2, out, size, nil, nil)
  out
end

EngineTest.test('a Zeus Video Player command gets its answer') do
  answer = "\0" * 256
  command = zeus_utf8_to_utf16("where ZeusVideoPlayer source\0")
  Win32API.new('winmm', 'mciSendStringW', 'ppii', 'i').call(command, answer, answer.length / 2, 0)
  text = zeus_utf16_to_utf8(answer)
  EngineTest.assert_equal('0 0 320 240', text[0, text.index("\0")], 'answer')
end

if CODE_PAGES
  EngineTest.test('MultiByteToWideChar reads Shift_JIS') do
    out = "\0" * 4
    count = MULTI_BYTE_TO_WIDE_CHAR.call(932, 0, [0x82, 0xA0].pack('C*'), 2, out, 2)
    EngineTest.assert_equal(1, count, 'code units written')
    EngineTest.assert_equal([0x42, 0x30, 0, 0], byte_list(out), 'bytes written')
  end
end

EngineTest.finish
exit
