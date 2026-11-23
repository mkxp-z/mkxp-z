# Test suite for kgl2_wrap.rb
# Author: white-axe (2026)

# Creative Commons CC0: To the extent possible under law, white-axe has
# dedicated all copyright and related and neighboring rights to this script
# to the public domain worldwide.
# https://creativecommons.org/publicdomain/zero/1.0/

require_relative 'scripts/preload/kgl2_wrap'

if Win32API.new('System/KGL2.klib', 'kglVersion', '', 'i').call != 200
  System.puts 'Version should be 200'
  exit 1
end
Win32API.new('System/KGL2.klib', 'kglLoad', '', 'i').call

all_tests_passed = true

bitmap = Bitmap.new(1, 1)
bitmap.fill_rect(bitmap.rect, Color.new(0x12, 0x34, 0x56, 0xff))
status = Win32API.new('System/KGL2.klib', 'kglCompressAlpha', 'ii', 'i').call(bitmap.object_id)
expected_data = "\x12\x34\x56\x00"
if status == 1 && bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 1 ok'
else
  System.puts 'Test 1 failed'
  all_tests_passed = false
end

bitmap = Bitmap.new(1, 1)
bitmap.fill_rect(bitmap.rect, Color.new(0x12, 0x34, 0x56, 0x00))
status = Win32API.new('System/KGL2.klib', 'kglCompressAlpha', 'ii', 'i').call(bitmap.object_id)
expected_data = "\x00\x00\x00\x00"
if status == 1 && bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 2 ok'
else
  System.puts 'Test 2 failed'
  all_tests_passed = false
end

bitmap = Bitmap.new(1, 1)
bitmap.fill_rect(bitmap.rect, Color.new(0xff, 0xff, 0xff, 0x7f))
status = Win32API.new('System/KGL2.klib', 'kglCompressAlpha', 'ii', 'i').call(bitmap.object_id)
expected_data1 = "\x7f\x7f\x7f\x00"
expected_data2 = "\x80\x80\x80\x00"
if status == 1 && [String.new(expected_data1, encoding: 'ASCII-8BIT'), String.new(expected_data2, encoding: 'ASCII-8BIT')].include?(bitmap.raw_data)
  System.puts 'Test 3 ok'
else
  System.puts 'Test 3 failed'
  all_tests_passed = false
end

exit(all_tests_passed ? 0 : 1)
