# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# Hime's full keyboard script (LonaRPG) keeps the key states in
# `DL::CPtr.new(DL.malloc(256), 256)` and passes `ptr.to_i` to
# GetKeyboardState.
#
#   tools/run-engine-tests.sh --suite dl_memory.rb --ruby 19
#
# This file must parse on Ruby 1.8, 1.9 and 3.1.

unless defined?(EngineTest)
  harness_path = File.join(File.dirname(__FILE__), 'harness.rb')
  eval(File.read(harness_path), TOPLEVEL_BINDING, harness_path) # rubocop:disable Security/Eval
end

EngineTest.suite('dl-memory', 8)

EngineTest.load_preloads(%w[windows_fs platform_compat win32_wrap])

EngineTest.test('two DL.malloc calls give two addresses') do
  EngineTest.refute_equal(DL.malloc(4), DL.malloc(4), 'second address')
end

# Windows DL reads a signed char, so 0xff reads back as -1.
EngineTest.test('a DL::CPtr keeps the bytes of its address') do
  address = DL.malloc(4)
  DL::CPtr.new(address, 4)[2] = 0x1ff
  EngineTest.assert_equal([0, 0, -1, 0], (0...4).map { |i| DL::CPtr.new(address)[i] }, 'bytes')
end

EngineTest.test('an address that DL.malloc did not give reads as 0') do
  EngineTest.assert_equal(0, DL::CPtr.new(0x1234)[0], 'byte')
end

EngineTest.test('GetKeyboardState writes to the address of a DL::CPtr') do
  state = DL::CPtr.new(DL.malloc(256), 256)
  256.times { |i| state[i] = 0xff }
  result = Win32API.new('user32.dll', 'GetKeyboardState', 'I', 'I').call(state.to_i)
  EngineTest.assert_equal(1, result, 'GetKeyboardState result')
  # No key is down in the test host.
  EngineTest.assert_equal(0, state[0x41], 'state of the A key')
  EngineTest.assert_equal(0, state[0x07], 'state of a key with no mapping')
end

EngineTest.test('GetKeyboardState writes to a String buffer') do
  buffer = "\xff" * 256
  result = Win32API.new('user32.dll', 'GetKeyboardState', 'P', 'I').call(buffer)
  EngineTest.assert_equal(1, result, 'GetKeyboardState result')
  EngineTest.assert_equal(0, buffer.unpack('C*')[0x41], 'state of the A key')
end

EngineTest.test('RtlMoveMemory reads the memory of DL.malloc') do
  address = DL.malloc(4)
  DL::CPtr.new(address, 4)[1] = 7
  buffer = "\xff" * 4
  Win32API.new('kernel32.dll', 'RtlMoveMemory', 'PII', 'V').call(buffer, address, 4)
  EngineTest.assert_equal([0, 7, 0, 0], buffer.unpack('C*'), 'bytes')
end

# In UTF-8, the bytes 0xC3 0xA9 are one character.
EngineTest.test('RtlMoveMemory reads one byte of a UTF-8 character') do
  address = DL.malloc(2)
  pointer = DL::CPtr.new(address, 2)
  pointer[0] = 0xC3
  pointer[1] = 0xA9
  buffer = "\0" * 2
  Win32API.new('kernel32.dll', 'RtlMoveMemory', 'PII', 'V').call(buffer, address, 1)
  EngineTest.assert_equal([0xC3, 0], buffer.unpack('C*'), 'bytes')
end

EngineTest.test('RtlMoveMemory writes one byte into a UTF-8 character') do
  address = DL.malloc(1)
  DL::CPtr.new(address, 1)[0] = 0x68
  buffer = [0xC3, 0xA9].pack('C*')
  buffer.force_encoding('UTF-8') if buffer.respond_to?(:force_encoding)
  Win32API.new('kernel32.dll', 'RtlMoveMemory', 'PII', 'V').call(buffer, address, 1)
  EngineTest.assert_equal([0x68, 0xA9], buffer.unpack('C*'), 'bytes')
end

EngineTest.finish
exit
