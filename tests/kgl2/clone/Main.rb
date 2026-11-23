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

dst_bitmap = Bitmap.new(1, 1)
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0x12, 0x34, 0x56, 0x78))
src_bitmap = Bitmap.new(1, 1)
src_bitmap.fill_rect(src_bitmap.rect, Color.new(0x87, 0x65, 0x43, 0x21))
status = Win32API.new('System/KGL2.klib', 'kglClone', 'ii', 'i').call(dst_bitmap.object_id, src_bitmap.object_id)
dst_expected_data = "\x87\x65\x43\x21"
src_expected_data = "\x87\x65\x43\x21"
if status == 1 && dst_bitmap.raw_data == String.new(dst_expected_data, encoding: 'ASCII-8BIT') && src_bitmap.raw_data == String.new(src_expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 1 ok'
else
  System.puts 'Test 1 failed'
  all_tests_passed = false
end

dst_bitmap = Bitmap.new(1, 2)
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0x12, 0x34, 0x56, 0x78))
src_bitmap = Bitmap.new(1, 1)
src_bitmap.fill_rect(src_bitmap.rect, Color.new(0x87, 0x65, 0x43, 0x21))
status = Win32API.new('System/KGL2.klib', 'kglClone', 'ii', 'i').call(dst_bitmap.object_id, src_bitmap.object_id)
dst_expected_data = "\x12\x34\x56\x78\x12\x34\x56\x78"
src_expected_data = "\x87\x65\x43\x21"
if status == 112 && dst_bitmap.raw_data == String.new(dst_expected_data, encoding: 'ASCII-8BIT') && src_bitmap.raw_data == String.new(src_expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 2 ok'
else
  System.puts 'Test 2 failed'
  all_tests_passed = false
end

exit(all_tests_passed ? 0 : 1)
