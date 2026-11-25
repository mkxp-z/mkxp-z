# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# A game that drops a large bitmap on each frame does not fill the GPU
# memory. Pokemon Xenoverse clones a 1024x1024 background on each frame
# of a trade. The pixels are not on the Ruby heap, so the interpreter
# alone collects too late, and iOS stops the app.
#
#   tools/run-engine-tests.sh --suite gc_bitmap.rb --ruby 18
#
# This file must parse on all three interpreters.

unless defined?(EngineTest)
  harness_path = File.join(File.dirname(__FILE__), 'harness.rb')
  eval(File.read(harness_path), TOPLEVEL_BINDING, harness_path) # rubocop:disable Security/Eval
end

EngineTest.suite('gc-bitmap', 3)

def live_bitmaps
  count = 0
  ObjectSpace.each_object(Bitmap) { |b| count += 1 unless b.disposed? }
  count
end

# Keeps the last bitmap that the block gives, as a game shows it, and
# drops the one before.
def most_live_bitmaps(drops)
  GC.start
  shown = nil
  most = 0
  drops.times do |i|
    shown = yield
    most = [most, live_bitmaps].max if (i % 20).zero?
  end
  most
end

# 400 MB of 1024x1024 bitmaps.
EngineTest.test('dropped bitmaps are collected before they fill the memory') do
  source = Bitmap.new(1024, 1024)
  most = most_live_bitmaps(600) { source.clone }
  EngineTest.assert(most <= 100, "#{most} bitmaps were alive at one time")
end

# Each clone of an animated bitmap makes a texture for each frame.
EngineTest.test('dropped animated clones are collected before they fill the memory') do
  source = Bitmap.new(256, 256)
  frame = Bitmap.new(256, 256)
  9.times { source.add_frame(frame) }
  EngineTest.assert(source.frame_count == 10, "the source has #{source.frame_count} frames")
  most = most_live_bitmaps(400) { source.clone }
  EngineTest.assert(most <= 150, "#{most} bitmaps were alive at one time")
end

EngineTest.test('dropped bitmaps with added frames are collected before they fill the memory') do
  frame = Bitmap.new(256, 256)
  most = most_live_bitmaps(400) do
    animated = Bitmap.new(256, 256)
    9.times { animated.add_frame(frame) }
    animated
  end
  EngineTest.assert(most <= 150, "#{most} bitmaps were alive at one time")
end

EngineTest.finish
exit
