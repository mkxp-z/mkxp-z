# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# With enableHires, get_pixel on a bitmap reads its high-res copy. It
# must give the values that RPG Maker gives: a fully clear pixel keeps
# its color, and a pixel outside the bitmap is 0, 0, 0, 0. Run it with
# high-res on, at both texture scales:
#
#   tools/run-engine-tests.sh --suite hires_pixel.rb \
#       --config '{"enableHires":true,"framebufferScalingFactor":2}'
#   tools/run-engine-tests.sh --suite hires_pixel.rb \
#       --config '{"enableHires":true,"textureScalingFactor":2,"framebufferScalingFactor":2}'
#
# This file must parse on Ruby 1.8, 1.9, and 3.x.

unless defined?(EngineTest)
  harness_path = File.join(File.dirname(__FILE__), 'harness.rb')
  eval(File.read(harness_path), TOPLEVEL_BINDING, harness_path) # rubocop:disable Security/Eval
end

EngineTest.suite('hires-pixel', 5)

SIZE = 32

EngineTest.test('the bitmap has a high-res copy') do
  EngineTest.bitmap(SIZE, SIZE) do |bitmap|
    EngineTest.refute_equal(nil, bitmap.hires, 'Bitmap#hires')
    EngineTest.info('hires_size', "#{bitmap.hires.width}x#{bitmap.hires.height}")
  end
end

EngineTest.test('a new bitmap reads 0, 0, 0, 0') do
  EngineTest.bitmap(SIZE, SIZE) do |bitmap|
    EngineTest.assert_equal([0, 0, 0, 0], EngineTest.pixel(bitmap, [0, 0]), 'pixel 0, 0')
  end
end

EngineTest.test('a fully clear pixel keeps its color') do
  EngineTest.bitmap(SIZE, SIZE, Color.new(255, 128, 0, 0)) do |bitmap|
    EngineTest.assert_equal([255, 128, 0, 0], EngineTest.pixel(bitmap, [3, 3]), 'pixel 3, 3')
  end
end

EngineTest.test('a pixel outside the bitmap reads 0, 0, 0, 0') do
  EngineTest.bitmap(SIZE, SIZE, Color.new(10, 20, 30, 255)) do |bitmap|
    [[SIZE, 0], [0, SIZE], [-1, 0], [0, -1]].each do |point|
      EngineTest.assert_equal([0, 0, 0, 0], EngineTest.pixel(bitmap, point), "pixel #{point.inspect}")
    end
  end
end

# One opaque pixel in the high-res block gives its color, and the
# alpha is the average of the block.
EngineTest.test('a block takes the color of its opaque pixels') do
  EngineTest.bitmap(SIZE, SIZE) do |bitmap|
    block = (bitmap.hires.width / SIZE) * (bitmap.hires.height / SIZE)
    bitmap.hires.set_pixel(0, 0, Color.new(200, 100, 50, 255))
    expected = [200, 100, 50, (255.0 / block).round]
    EngineTest.assert_equal(expected, EngineTest.pixel(bitmap, [0, 0]), 'pixel 0, 0')
  end
end

EngineTest.finish
exit
