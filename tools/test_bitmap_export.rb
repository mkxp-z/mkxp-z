#!/usr/bin/env ruby
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

require_relative 'assertion_count'

SCRIPT = File.expand_path('../scripts/postload/pokemon_graphics_compat.rb', __dir__)

def assert_eq(actual, expected, label)
  asserted
  return if actual == expected

  warn "FAIL: #{label}\n  expected: #{expected.inspect}\n  actual:   #{actual.inspect}"
  exit 1
end

class Bitmap
  attr_reader :written

  def raw_data; end

  def to_file(filename)
    (@written ||= []) << filename
    nil
  end

  def last_row_address
    0
  end
end

prev_verbose = $VERBOSE
$VERBOSE = nil
begin
  load SCRIPT
ensure
  $VERBOSE = prev_verbose
end

# Ecchi Version calls `save_to_file(path)` when `$MKXP` is set, and
# `export(path)` with no `.png` on Windows, which adds `.png`.
[
  ['Graphics/Characters/player_walk.png', 'Graphics/Characters/player_walk.png'],
  ['Graphics/Transitions/vsPlayer.png', 'Graphics/Transitions/vsPlayer.png'],
  ['Saves/Photo - 1794778193', 'Saves/Photo - 1794778193.png']
].each do |path, windows_file|
  bitmap = Bitmap.new
  bitmap.save_to_file(path)
  assert_eq(bitmap.written, [windows_file], "save_to_file(#{path.inspect}) writes the Windows file")
end

test_passed('test_bitmap_export', 3)
