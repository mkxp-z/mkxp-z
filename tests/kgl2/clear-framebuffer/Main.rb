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
bitmap.fill_rect(bitmap.rect, Color.new(0xde, 0xad, 0xbe, 0xef))
bind_status = Win32API.new('System/KGL2.klib', 'kglBindFramebuffer', 'i', 'i').call(bitmap.object_id)
status = Win32API.new('System/KGL2.klib', 'kglClearFramebuffer', '', 'i').call
expected_data = "\x00\x00\x00\x00"
if status == 1 && bind_status == 1 && bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 1 ok'
else
  System.puts 'Test 1 failed'
  all_tests_passed = false
end

exit(all_tests_passed ? 0 : 1)
