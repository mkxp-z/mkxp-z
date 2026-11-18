#!/usr/bin/env ruby
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# Unit tests for audio_file_name_repair.rb: a sound from a save with a
# number, a list or another object as its name.
# Run: ruby tools/test_audio_file_name_repair.rb

require_relative 'assertion_count'

ROOT = File.expand_path('..', __dir__)
SCRIPT = File.join(ROOT, 'scripts', 'postload', 'audio_file_name_repair.rb')

def assert_eq(actual, expected, label)
  asserted
  return if actual == expected

  warn "FAIL: #{label}\n  expected: #{expected.inspect}\n  actual:   #{actual.inspect}"
  exit 1
end

Sprite = Class.new

class Table
  def initialize(*); end
end

Color = Struct.new(:red, :green, :blue)

load File.join(ROOT, 'scripts', 'preload', 'rpg_xp.rb')

# The game's own definition, as in GS Vitro.
def load_game_scripts
  RPG::AudioFile.class_eval { attr_accessor :name }
end

load_game_scripts
load SCRIPT

# A save from the old engine: the name slot held another object.
def from_save(name)
  sound = RPG::AudioFile.new
  sound.instance_variable_set(:@name, name)
  Marshal.load(Marshal.dump(sound))
end

[0.0123456789, [1, 2], Color.new(1, 2, 3)].each do |bad|
  sound = from_save(bad)
  assert_eq(sound.name, '', "a #{bad.class} name reads as no sound")
  assert_eq(Marshal.load(Marshal.dump(sound)).instance_variable_get(:@name), '',
            "the next save keeps the repaired #{bad.class} name")
end

assert_eq(from_save('Rain').name, 'Rain', 'a good name stays')
assert_eq(from_save(nil).name, nil, 'a nil name stays')
assert_eq(File.basename(from_save(1.5).name, '.*'), '', 'the GS Vitro weather call works')

# F12 runs the game's scripts again, then the postloads.
load_game_scripts
load SCRIPT
assert_eq(from_save(1.5).name, '', 'a Float name reads as no sound after F12')

test_passed('audio_file_name_repair', 10)
