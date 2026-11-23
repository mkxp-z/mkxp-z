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
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap = Bitmap.new(1, 1)
src_bitmap.fill_rect(src_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
x = 0
y = 0
opacity = 255
light_blending = 1
bind_status = Win32API.new('System/KGL2.klib', 'kglBindFramebuffer', 'i', 'i').call(dst_bitmap.object_id)
setting_status = Win32API.new('System/KGL2.klib', 'kglLightBlending', 'i', 'i').call(light_blending)
shader_status = Win32API.new('System/KGL2.klib', 'kglLightShader', 'iiii', 'i').call(src_bitmap.object_id, x, y, opacity)
expected_data = "\x00\x00\x00\xff"
if setting_status == 1 && bind_status == 1 && shader_status == 1 && dst_bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 1 ok'
else
  System.puts 'Test 1 failed'
  all_tests_passed = false
end

dst_bitmap = Bitmap.new(1, 1)
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0x03, 0x02, 0x01, 0xff))
src_bitmap = Bitmap.new(1, 1)
src_bitmap.fill_rect(src_bitmap.rect, Color.new(0x01, 0x02, 0x03, 0xff))
x = 0
y = 0
opacity = 255
light_blending = 1
bind_status = Win32API.new('System/KGL2.klib', 'kglBindFramebuffer', 'i', 'i').call(dst_bitmap.object_id)
setting_status = Win32API.new('System/KGL2.klib', 'kglLightBlending', 'i', 'i').call(light_blending)
shader_status = Win32API.new('System/KGL2.klib', 'kglLightShader', 'iiii', 'i').call(src_bitmap.object_id, x, y, opacity)
expected_data = "\x02\x00\x00\xff"
if setting_status == 1 && bind_status == 1 && shader_status == 1 && dst_bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 2 ok'
else
  System.puts 'Test 2 failed'
  all_tests_passed = false
end

dst_bitmap = Bitmap.new(1, 1)
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0x00, 0x00, 0x00, 0x00))
src_bitmap = Bitmap.new(1, 1)
src_bitmap.fill_rect(src_bitmap.rect, Color.new(0x00, 0x00, 0x00, 0x00))
x = 0
y = 0
opacity = 255
light_blending = 1
bind_status = Win32API.new('System/KGL2.klib', 'kglBindFramebuffer', 'i', 'i').call(dst_bitmap.object_id)
setting_status = Win32API.new('System/KGL2.klib', 'kglLightBlending', 'i', 'i').call(light_blending)
shader_status = Win32API.new('System/KGL2.klib', 'kglLightShader', 'iiii', 'i').call(src_bitmap.object_id, x, y, opacity)
expected_data = "\x00\x00\x00\xff"
if setting_status == 1 && bind_status == 1 && shader_status == 1 && dst_bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 3 ok'
else
  System.puts 'Test 3 failed'
  all_tests_passed = false
end

dst_bitmap = Bitmap.new(1, 1)
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap = Bitmap.new(1, 1)
src_bitmap.fill_rect(src_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
x = 0
y = 0
opacity = 0
light_blending = 1
bind_status = Win32API.new('System/KGL2.klib', 'kglBindFramebuffer', 'i', 'i').call(dst_bitmap.object_id)
setting_status = Win32API.new('System/KGL2.klib', 'kglLightBlending', 'i', 'i').call(light_blending)
shader_status = Win32API.new('System/KGL2.klib', 'kglLightShader', 'iiii', 'i').call(src_bitmap.object_id, x, y, opacity)
expected_data = "\xff\xff\xff\xff"
if setting_status == 1 && bind_status == 1 && shader_status == 1 && dst_bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 4 ok'
else
  System.puts 'Test 4 failed'
  all_tests_passed = false
end

dst_bitmap = Bitmap.new(1, 1)
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap = Bitmap.new(1, 1)
src_bitmap.fill_rect(src_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
x = 0
y = 0
opacity = 127
light_blending = 1
bind_status = Win32API.new('System/KGL2.klib', 'kglBindFramebuffer', 'i', 'i').call(dst_bitmap.object_id)
setting_status = Win32API.new('System/KGL2.klib', 'kglLightBlending', 'i', 'i').call(light_blending)
shader_status = Win32API.new('System/KGL2.klib', 'kglLightShader', 'iiii', 'i').call(src_bitmap.object_id, x, y, opacity)
expected_data1 = "\x7f\x7f\x7f\xff"
expected_data2 = "\x80\x80\x80\xff"
if setting_status == 1 && bind_status == 1 && shader_status == 1 && [String.new(expected_data1, encoding: 'ASCII-8BIT'), String.new(expected_data2, encoding: 'ASCII-8BIT')].include?(dst_bitmap.raw_data)
  System.puts 'Test 5 ok'
else
  System.puts 'Test 5 failed'
  all_tests_passed = false
end

dst_bitmap = Bitmap.new(1, 1)
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap = Bitmap.new(1, 1)
src_bitmap.fill_rect(src_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
x = 0
y = 0
opacity = 100
light_blending = 0
bind_status = Win32API.new('System/KGL2.klib', 'kglBindFramebuffer', 'i', 'i').call(dst_bitmap.object_id)
setting_status = Win32API.new('System/KGL2.klib', 'kglLightBlending', 'i', 'i').call(light_blending)
shader_status = Win32API.new('System/KGL2.klib', 'kglLightShader', 'iiii', 'i').call(src_bitmap.object_id, x, y, opacity)
expected_data = "\xff\xff\xff\xff"
if setting_status == 1 && bind_status == 1 && shader_status == 1 && dst_bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 6 ok'
else
  System.puts 'Test 6 failed'
  all_tests_passed = false
end

dst_bitmap = Bitmap.new(1, 1)
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap = Bitmap.new(1, 1)
src_bitmap.fill_rect(src_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
x = 0
y = 0
opacity = 101
light_blending = 0
bind_status = Win32API.new('System/KGL2.klib', 'kglBindFramebuffer', 'i', 'i').call(dst_bitmap.object_id)
setting_status = Win32API.new('System/KGL2.klib', 'kglLightBlending', 'i', 'i').call(light_blending)
shader_status = Win32API.new('System/KGL2.klib', 'kglLightShader', 'iiii', 'i').call(src_bitmap.object_id, x, y, opacity)
expected_data = "\x00\x00\x00\xff"
if setting_status == 1 && bind_status == 1 && shader_status == 1 && dst_bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 7 ok'
else
  System.puts 'Test 7 failed'
  all_tests_passed = false
end

dst_bitmap = Bitmap.new(2, 2)
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap = Bitmap.new(2, 2)
src_bitmap.fill_rect(Rect.new(0, 0, 1, 1), Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap.fill_rect(Rect.new(1, 0, 1, 1), Color.new(0xef, 0xef, 0xef, 0xff))
src_bitmap.fill_rect(Rect.new(0, 1, 1, 1), Color.new(0xfe, 0xfe, 0xfe, 0xff))
src_bitmap.fill_rect(Rect.new(1, 1, 1, 1), Color.new(0xee, 0xee, 0xee, 0xff))
x = 0
y = 0
opacity = 255
light_blending = 1
bind_status = Win32API.new('System/KGL2.klib', 'kglBindFramebuffer', 'i', 'i').call(dst_bitmap.object_id)
setting_status = Win32API.new('System/KGL2.klib', 'kglLightBlending', 'i', 'i').call(light_blending)
shader_status = Win32API.new('System/KGL2.klib', 'kglLightShader', 'iiii', 'i').call(src_bitmap.object_id, x, y, opacity)
expected_data = "\x00\x00\x00\xff" "\x10\x10\x10\xff" "\x01\x01\x01\xff" "\x11\x11\x11\xff"
if setting_status == 1 && bind_status == 1 && shader_status == 1 && dst_bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 8 ok'
else
  System.puts 'Test 8 failed'
  all_tests_passed = false
end

dst_bitmap = Bitmap.new(2, 2)
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap = Bitmap.new(2, 2)
src_bitmap.fill_rect(Rect.new(0, 0, 1, 1), Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap.fill_rect(Rect.new(1, 0, 1, 1), Color.new(0xef, 0xef, 0xef, 0xff))
src_bitmap.fill_rect(Rect.new(0, 1, 1, 1), Color.new(0xfe, 0xfe, 0xfe, 0xff))
src_bitmap.fill_rect(Rect.new(1, 1, 1, 1), Color.new(0xee, 0xee, 0xee, 0xff))
x = 1
y = 0
opacity = 255
light_blending = 1
bind_status = Win32API.new('System/KGL2.klib', 'kglBindFramebuffer', 'i', 'i').call(dst_bitmap.object_id)
setting_status = Win32API.new('System/KGL2.klib', 'kglLightBlending', 'i', 'i').call(light_blending)
shader_status = Win32API.new('System/KGL2.klib', 'kglLightShader', 'iiii', 'i').call(src_bitmap.object_id, x, y, opacity)
expected_data = "\xff\xff\xff\xff" "\x00\x00\x00\xff" "\xff\xff\xff\xff" "\x01\x01\x01\xff"
if setting_status == 1 && bind_status == 1 && shader_status == 1 && dst_bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 9 ok'
else
  System.puts 'Test 9 failed'
  all_tests_passed = false
end

dst_bitmap = Bitmap.new(2, 2)
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap = Bitmap.new(2, 2)
src_bitmap.fill_rect(Rect.new(0, 0, 1, 1), Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap.fill_rect(Rect.new(1, 0, 1, 1), Color.new(0xef, 0xef, 0xef, 0xff))
src_bitmap.fill_rect(Rect.new(0, 1, 1, 1), Color.new(0xfe, 0xfe, 0xfe, 0xff))
src_bitmap.fill_rect(Rect.new(1, 1, 1, 1), Color.new(0xee, 0xee, 0xee, 0xff))
x = 0
y = 1
opacity = 255
light_blending = 1
bind_status = Win32API.new('System/KGL2.klib', 'kglBindFramebuffer', 'i', 'i').call(dst_bitmap.object_id)
setting_status = Win32API.new('System/KGL2.klib', 'kglLightBlending', 'i', 'i').call(light_blending)
shader_status = Win32API.new('System/KGL2.klib', 'kglLightShader', 'iiii', 'i').call(src_bitmap.object_id, x, y, opacity)
expected_data = "\xff\xff\xff\xff" "\xff\xff\xff\xff" "\x00\x00\x00\xff" "\x10\x10\x10\xff"
if setting_status == 1 && bind_status == 1 && shader_status == 1 && dst_bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 10 ok'
else
  System.puts 'Test 10 failed'
  all_tests_passed = false
end

dst_bitmap = Bitmap.new(2, 2)
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap = Bitmap.new(2, 2)
src_bitmap.fill_rect(Rect.new(0, 0, 1, 1), Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap.fill_rect(Rect.new(1, 0, 1, 1), Color.new(0xef, 0xef, 0xef, 0xff))
src_bitmap.fill_rect(Rect.new(0, 1, 1, 1), Color.new(0xfe, 0xfe, 0xfe, 0xff))
src_bitmap.fill_rect(Rect.new(1, 1, 1, 1), Color.new(0xee, 0xee, 0xee, 0xff))
x = -1
y = 0
opacity = 255
light_blending = 1
bind_status = Win32API.new('System/KGL2.klib', 'kglBindFramebuffer', 'i', 'i').call(dst_bitmap.object_id)
setting_status = Win32API.new('System/KGL2.klib', 'kglLightBlending', 'i', 'i').call(light_blending)
shader_status = Win32API.new('System/KGL2.klib', 'kglLightShader', 'iiii', 'i').call(src_bitmap.object_id, x, y, opacity)
expected_data = "\x10\x10\x10\xff" "\xff\xff\xff\xff" "\x11\x11\x11\xff" "\xff\xff\xff\xff"
if setting_status == 1 && bind_status == 1 && shader_status == 1 && dst_bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 11 ok'
else
  System.puts 'Test 11 failed'
  all_tests_passed = false
end

dst_bitmap = Bitmap.new(2, 2)
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap = Bitmap.new(2, 2)
src_bitmap.fill_rect(Rect.new(0, 0, 1, 1), Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap.fill_rect(Rect.new(1, 0, 1, 1), Color.new(0xef, 0xef, 0xef, 0xff))
src_bitmap.fill_rect(Rect.new(0, 1, 1, 1), Color.new(0xfe, 0xfe, 0xfe, 0xff))
src_bitmap.fill_rect(Rect.new(1, 1, 1, 1), Color.new(0xee, 0xee, 0xee, 0xff))
x = 0
y = -1
opacity = 255
light_blending = 1
bind_status = Win32API.new('System/KGL2.klib', 'kglBindFramebuffer', 'i', 'i').call(dst_bitmap.object_id)
setting_status = Win32API.new('System/KGL2.klib', 'kglLightBlending', 'i', 'i').call(light_blending)
shader_status = Win32API.new('System/KGL2.klib', 'kglLightShader', 'iiii', 'i').call(src_bitmap.object_id, x, y, opacity)
expected_data = "\x01\x01\x01\xff" "\x11\x11\x11\xff" "\xff\xff\xff\xff" "\xff\xff\xff\xff"
if setting_status == 1 && bind_status == 1 && shader_status == 1 && dst_bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 12 ok'
else
  System.puts 'Test 12 failed'
  all_tests_passed = false
end

dst_bitmap = Bitmap.new(2, 2)
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap = Bitmap.new(2, 2)
src_bitmap.fill_rect(Rect.new(0, 0, 1, 1), Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap.fill_rect(Rect.new(1, 0, 1, 1), Color.new(0xef, 0xef, 0xef, 0xff))
src_bitmap.fill_rect(Rect.new(0, 1, 1, 1), Color.new(0xfe, 0xfe, 0xfe, 0xff))
src_bitmap.fill_rect(Rect.new(1, 1, 1, 1), Color.new(0xee, 0xee, 0xee, 0xff))
x = 2
y = 0
opacity = 255
light_blending = 1
bind_status = Win32API.new('System/KGL2.klib', 'kglBindFramebuffer', 'i', 'i').call(dst_bitmap.object_id)
setting_status = Win32API.new('System/KGL2.klib', 'kglLightBlending', 'i', 'i').call(light_blending)
shader_status = Win32API.new('System/KGL2.klib', 'kglLightShader', 'iiii', 'i').call(src_bitmap.object_id, x, y, opacity)
expected_data = "\xff\xff\xff\xff" "\xff\xff\xff\xff" "\xff\xff\xff\xff" "\xff\xff\xff\xff"
if setting_status == 1 && bind_status == 1 && shader_status == 1 && dst_bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 13 ok'
else
  System.puts 'Test 13 failed'
  all_tests_passed = false
end

dst_bitmap = Bitmap.new(2, 2)
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap = Bitmap.new(2, 2)
src_bitmap.fill_rect(Rect.new(0, 0, 1, 1), Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap.fill_rect(Rect.new(1, 0, 1, 1), Color.new(0xef, 0xef, 0xef, 0xff))
src_bitmap.fill_rect(Rect.new(0, 1, 1, 1), Color.new(0xfe, 0xfe, 0xfe, 0xff))
src_bitmap.fill_rect(Rect.new(1, 1, 1, 1), Color.new(0xee, 0xee, 0xee, 0xff))
x = 0
y = -2
opacity = 255
light_blending = 1
bind_status = Win32API.new('System/KGL2.klib', 'kglBindFramebuffer', 'i', 'i').call(dst_bitmap.object_id)
setting_status = Win32API.new('System/KGL2.klib', 'kglLightBlending', 'i', 'i').call(light_blending)
shader_status = Win32API.new('System/KGL2.klib', 'kglLightShader', 'iiii', 'i').call(src_bitmap.object_id, x, y, opacity)
expected_data = "\xff\xff\xff\xff" "\xff\xff\xff\xff" "\xff\xff\xff\xff" "\xff\xff\xff\xff"
if setting_status == 1 && bind_status == 1 && shader_status == 1 && dst_bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 14 ok'
else
  System.puts 'Test 14 failed'
  all_tests_passed = false
end

dst_bitmap = Bitmap.new(2, 2)
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap = Bitmap.new(2, 2)
src_bitmap.fill_rect(Rect.new(0, 0, 1, 1), Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap.fill_rect(Rect.new(1, 0, 1, 1), Color.new(0xef, 0xef, 0xef, 0xff))
src_bitmap.fill_rect(Rect.new(0, 1, 1, 1), Color.new(0xfe, 0xfe, 0xfe, 0xff))
src_bitmap.fill_rect(Rect.new(1, 1, 1, 1), Color.new(0xee, 0xee, 0xee, 0xff))
x = -3
y = 0
opacity = 255
light_blending = 1
bind_status = Win32API.new('System/KGL2.klib', 'kglBindFramebuffer', 'i', 'i').call(dst_bitmap.object_id)
setting_status = Win32API.new('System/KGL2.klib', 'kglLightBlending', 'i', 'i').call(light_blending)
shader_status = Win32API.new('System/KGL2.klib', 'kglLightShader', 'iiii', 'i').call(src_bitmap.object_id, x, y, opacity)
expected_data = "\xff\xff\xff\xff" "\xff\xff\xff\xff" "\xff\xff\xff\xff" "\xff\xff\xff\xff"
if setting_status == 1 && bind_status == 1 && shader_status == 111 && dst_bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 15 ok'
else
  System.puts 'Test 15 failed'
  all_tests_passed = false
end

dst_bitmap = Bitmap.new(2, 2)
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap = Bitmap.new(2, 2)
src_bitmap.fill_rect(Rect.new(0, 0, 1, 1), Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap.fill_rect(Rect.new(1, 0, 1, 1), Color.new(0xef, 0xef, 0xef, 0xff))
src_bitmap.fill_rect(Rect.new(0, 1, 1, 1), Color.new(0xfe, 0xfe, 0xfe, 0xff))
src_bitmap.fill_rect(Rect.new(1, 1, 1, 1), Color.new(0xee, 0xee, 0xee, 0xff))
x = 0
y = -3
opacity = 255
light_blending = 1
bind_status = Win32API.new('System/KGL2.klib', 'kglBindFramebuffer', 'i', 'i').call(dst_bitmap.object_id)
setting_status = Win32API.new('System/KGL2.klib', 'kglLightBlending', 'i', 'i').call(light_blending)
shader_status = Win32API.new('System/KGL2.klib', 'kglLightShader', 'iiii', 'i').call(src_bitmap.object_id, x, y, opacity)
expected_data = "\xff\xff\xff\xff" "\xff\xff\xff\xff" "\xff\xff\xff\xff" "\xff\xff\xff\xff"
if setting_status == 1 && bind_status == 1 && shader_status == 111 && dst_bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 16 ok'
else
  System.puts 'Test 16 failed'
  all_tests_passed = false
end

dst_bitmap = Bitmap.new(3, 2)
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap = Bitmap.new(2, 2)
src_bitmap.fill_rect(Rect.new(0, 0, 1, 1), Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap.fill_rect(Rect.new(1, 0, 1, 1), Color.new(0xef, 0xef, 0xef, 0xff))
src_bitmap.fill_rect(Rect.new(0, 1, 1, 1), Color.new(0xfe, 0xfe, 0xfe, 0xff))
src_bitmap.fill_rect(Rect.new(1, 1, 1, 1), Color.new(0xee, 0xee, 0xee, 0xff))
x = 0
y = 0
opacity = 255
light_blending = 1
bind_status = Win32API.new('System/KGL2.klib', 'kglBindFramebuffer', 'i', 'i').call(dst_bitmap.object_id)
setting_status = Win32API.new('System/KGL2.klib', 'kglLightBlending', 'i', 'i').call(light_blending)
shader_status = Win32API.new('System/KGL2.klib', 'kglLightShader', 'iiii', 'i').call(src_bitmap.object_id, x, y, opacity)
expected_data = "\x00\x00\x00\xff" "\x10\x10\x10\xff" "\xff\xff\xff\xff" "\x01\x01\x01\xff" "\x11\x11\x11\xff" "\xff\xff\xff\xff"
if setting_status == 1 && bind_status == 1 && shader_status == 1 && dst_bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 17 ok'
else
  System.puts 'Test 17 failed'
  all_tests_passed = false
end

dst_bitmap = Bitmap.new(2, 3)
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap = Bitmap.new(2, 2)
src_bitmap.fill_rect(Rect.new(0, 0, 1, 1), Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap.fill_rect(Rect.new(1, 0, 1, 1), Color.new(0xef, 0xef, 0xef, 0xff))
src_bitmap.fill_rect(Rect.new(0, 1, 1, 1), Color.new(0xfe, 0xfe, 0xfe, 0xff))
src_bitmap.fill_rect(Rect.new(1, 1, 1, 1), Color.new(0xee, 0xee, 0xee, 0xff))
x = 0
y = 0
opacity = 255
light_blending = 1
bind_status = Win32API.new('System/KGL2.klib', 'kglBindFramebuffer', 'i', 'i').call(dst_bitmap.object_id)
setting_status = Win32API.new('System/KGL2.klib', 'kglLightBlending', 'i', 'i').call(light_blending)
shader_status = Win32API.new('System/KGL2.klib', 'kglLightShader', 'iiii', 'i').call(src_bitmap.object_id, x, y, opacity)
expected_data = "\x00\x00\x00\xff" "\x10\x10\x10\xff" "\x01\x01\x01\xff" "\x11\x11\x11\xff" "\xff\xff\xff\xff" "\xff\xff\xff\xff"
if setting_status == 1 && bind_status == 1 && shader_status == 1 && dst_bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 18 ok'
else
  System.puts 'Test 18 failed'
  all_tests_passed = false
end

dst_bitmap = Bitmap.new(2, 2)
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap = Bitmap.new(3, 2)
src_bitmap.fill_rect(Rect.new(0, 0, 1, 1), Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap.fill_rect(Rect.new(1, 0, 1, 1), Color.new(0xef, 0xef, 0xef, 0xff))
src_bitmap.fill_rect(Rect.new(2, 0, 1, 1), Color.new(0xdf, 0xdf, 0xdf, 0xff))
src_bitmap.fill_rect(Rect.new(0, 1, 1, 1), Color.new(0xfe, 0xfe, 0xfe, 0xff))
src_bitmap.fill_rect(Rect.new(1, 1, 1, 1), Color.new(0xee, 0xee, 0xee, 0xff))
src_bitmap.fill_rect(Rect.new(2, 1, 1, 1), Color.new(0xde, 0xde, 0xde, 0xff))
x = 0
y = 0
opacity = 255
light_blending = 1
bind_status = Win32API.new('System/KGL2.klib', 'kglBindFramebuffer', 'i', 'i').call(dst_bitmap.object_id)
setting_status = Win32API.new('System/KGL2.klib', 'kglLightBlending', 'i', 'i').call(light_blending)
shader_status = Win32API.new('System/KGL2.klib', 'kglLightShader', 'iiii', 'i').call(src_bitmap.object_id, x, y, opacity)
expected_data = "\x00\x00\x00\xff" "\x10\x10\x10\xff" "\x01\x01\x01\xff" "\x11\x11\x11\xff"
if setting_status == 1 && bind_status == 1 && shader_status == 1 && dst_bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 19 ok'
else
  System.puts 'Test 19 failed'
  all_tests_passed = false
end

dst_bitmap = Bitmap.new(2, 2)
dst_bitmap.fill_rect(dst_bitmap.rect, Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap = Bitmap.new(2, 3)
src_bitmap.fill_rect(Rect.new(0, 0, 1, 1), Color.new(0xff, 0xff, 0xff, 0xff))
src_bitmap.fill_rect(Rect.new(1, 0, 1, 1), Color.new(0xef, 0xef, 0xef, 0xff))
src_bitmap.fill_rect(Rect.new(0, 1, 1, 1), Color.new(0xfe, 0xfe, 0xfe, 0xff))
src_bitmap.fill_rect(Rect.new(1, 1, 1, 1), Color.new(0xee, 0xee, 0xee, 0xff))
src_bitmap.fill_rect(Rect.new(0, 2, 1, 1), Color.new(0xfd, 0xfd, 0xfd, 0xff))
src_bitmap.fill_rect(Rect.new(1, 2, 1, 1), Color.new(0xed, 0xed, 0xed, 0xff))
x = 0
y = 0
opacity = 255
light_blending = 1
setting_status = Win32API.new('System/KGL2.klib', 'kglLightBlending', 'i', 'i').call(light_blending)
bind_status = Win32API.new('System/KGL2.klib', 'kglBindFramebuffer', 'i', 'i').call(dst_bitmap.object_id)
shader_status = Win32API.new('System/KGL2.klib', 'kglLightShader', 'iiii', 'i').call(src_bitmap.object_id, x, y, opacity)
expected_data = "\x00\x00\x00\xff" "\x10\x10\x10\xff" "\x01\x01\x01\xff" "\x11\x11\x11\xff"
if setting_status == 1 && bind_status == 1 && shader_status == 1 && dst_bitmap.raw_data == String.new(expected_data, encoding: 'ASCII-8BIT')
  System.puts 'Test 20 ok'
else
  System.puts 'Test 20 failed'
  all_tests_passed = false
end

exit(all_tests_passed ? 0 : 1)
