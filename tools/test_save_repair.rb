#!/usr/bin/env ruby
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# Tests for save_repair.rb: the repair of broken sound names before a
# save, and the report when a game loads a save.
# Run: ruby mkxp-z-apple-mobile/tools/test_save_repair.rb

require_relative 'save_fs_harness'

# rubocop:disable Style/FileOpen, Security/MarshalLoad

module RPG
  class AudioFile
    attr_accessor :name

    def initialize(name = '')
      @name = name
    end
  end

  class BGM < AudioFile; end
end

class Game_System # rubocop:disable Naming/ClassAndModuleCamelCase -- the RGSS name.
  attr_accessor :bgm, :bgs

  def initialize(bgm = nil, bgs = nil)
    @bgm = bgm
    @bgs = bgs
  end
end

REPORTS = []

# The engine's load_data reads the file and gives Marshal.load a String.
# Its save_data opens the file in C and calls Marshal.dump.
Kernel.module_eval do
  def load_data(path, *_args)
    Marshal.load(File._mkxp_orig_open(path, 'rb', &:read))
  end

  def save_data(obj, path, *_args)
    File._mkxp_orig_open(path, 'wb') { |file| Marshal.dump(obj, file) }
  end
  module_function :load_data, :save_data
end

# The engine loads save_repair.rb before windows_fs.rb.
load File.join(ROOT, 'scripts', 'preload', 'save_repair.rb')
reset_workspace!
Dir.chdir(GAME)

# load_windows_fs! makes a new System.
def load_windows_fs_with_reports!
  load_windows_fs!
  System.module_eval do
    module_function

    define_method(:report_save_load) { |name| REPORTS << [:load, name] }
    define_method(:report_save_repaired) { |detail| REPORTS << [:repaired, detail] }
  end
end
load_windows_fs_with_reports!

SAVE = File.join(GAME, 'Game_2.rxdata')

def broken(name, klass = RPG::AudioFile)
  sound = klass.new('Rain')
  sound.instance_variable_set(:@name, name)
  sound
end

def saved_value
  File._mkxp_orig_open(SAVE, 'rb') { |file| Marshal.load(file) }
end

def count_walks
  walks = 0
  original = ObjectSpace.method(:each_object)
  ObjectSpace.define_singleton_method(:each_object) do |*args, &block|
    walks += 1
    original.call(*args, &block)
  end
  yield
  walks
ensure
  ObjectSpace.define_singleton_method(:each_object, original)
end

# The getter of audio_file_name_repair.rb before any save load.
load File.join(ROOT, 'scripts', 'postload', 'audio_file_name_repair.rb')
REPORTS.clear
assert_eq([broken(0.5).name, broken(0.25).name], ['', ''], 'the getter repairs a Float')
assert_eq(REPORTS, [[:repaired, 'unknown file: the @name of an RPG::AudioFile was a Float when the game read it']],
          'one getter report for each kind of fault')

REPORTS.clear
system = Game_System.new(broken(1.5), broken([1, 2], RPG::BGM))
other_float = broken(1.25)
walks = count_walks do
  File.open(SAVE, 'wb') do |file|
    Marshal.dump('Gold', file)
    Marshal.dump(system, file)
    Marshal.dump(12, file)
  end
end
assert_eq(walks, 1, 'one walk for each file write')
loaded = File.open(SAVE, 'rb') { |file| [Marshal.load(file), Marshal.load(file), Marshal.load(file)] }
assert_eq(loaded[1].bgm.instance_variable_get(:@name), '', 'the save holds no sound for a Float')
assert_eq(loaded[1].bgs.instance_variable_get(:@name), '', 'the save holds no sound for an Array')
assert_eq(other_float.name, '', 'a second Float is repaired')
assert_eq(REPORTS.sort,
          [[:load, 'Game_2.rxdata'],
           [:repaired, 'Game_2.rxdata: the @name of an RPG::AudioFile was a Float'],
           [:repaired, 'Game_2.rxdata: the @name of an RPG::BGM was an Array']],
          'one report for each kind of fault, then one load')

REPORTS.clear
walks = count_walks do
  File.open(SAVE, 'wb') { |file| Marshal.dump(system, file) }
  File.open(SAVE, 'wb') { |file| Marshal.dump(system, file) }
end
assert_eq(walks, 2, 'one walk for each of two saves')
assert_eq(REPORTS, [], 'a repaired sound gives no second report')

REPORTS.clear
kept = [RPG::AudioFile.new(nil), RPG::AudioFile.new('Route 30')]
frozen = broken(2.5).freeze
after_frozen = broken(3.5)
File.open(SAVE, 'wb') { |file| Marshal.dump([kept, frozen, after_frozen], file) }
assert_eq(kept.map(&:name), [nil, 'Route 30'], 'nil and String names stay')
assert_eq(frozen.instance_variable_get(:@name), 2.5, 'a frozen sound stays')
assert_eq(after_frozen.instance_variable_get(:@name), '', 'the walk goes on after a frozen sound')
assert_eq(REPORTS, [[:repaired, 'Game_2.rxdata: the @name of an RPG::AudioFile was a Float']],
          'one report for the sound after the frozen one')

REPORTS.clear
odd = Array.new(3) { broken(BasicObject.new) } + Array.new(3) { broken(3.75) }
File.open(SAVE, 'wb') { |file| Marshal.dump(odd, file) }
assert_eq(odd.map { |sound| sound.instance_variable_get(:@name) }, [''] * 6, 'a BasicObject name stops no repair')
assert_eq(REPORTS, [[:repaired, 'Game_2.rxdata: the @name of an RPG::AudioFile was a Float']],
          'the report of the Floats next to BasicObject names')

REPORTS.clear
save_data(Game_System.new(broken(4.5)), SAVE)
assert_eq(REPORTS, [[:repaired, 'Game_2.rxdata: the @name of an RPG::AudioFile was a Float']], 'save_data report')
assert_eq(saved_value.bgm.instance_variable_get(:@name), '', 'save_data saves no sound')

REPORTS.clear
in_memory = broken(5.5)
Marshal.dump(Game_System.new(in_memory))
assert_eq(REPORTS, [], 'a dump to a String repairs nothing')
in_memory.name = ''

# Loads
REPORTS.clear
options = File.join(GAME, 'Options.dt')
File.open(options, 'wb') { |file| Marshal.dump({ :volume => 80 }, file) }
File.open(options, 'rb') { |file| Marshal.load(file) }
load_data(options)
assert_eq(REPORTS, [], 'an option file is not a save')

[Game_System.new, { :system => Game_System.new }, [1, Game_System.new]].each do |value|
  REPORTS.clear
  File.open(SAVE, 'wb') { |file| Marshal.dump(value, file) }
  File.open(SAVE, 'rb') { |file| Marshal.load(file) }
  assert_eq(REPORTS, [[:load, 'Game_2.rxdata']], "a #{value.class} with a Game_System is a save")
end

REPORTS.clear
File.open(SAVE, 'wb') do |file|
  Marshal.dump(Game_System.new, file)
  Marshal.dump({ :system => Game_System.new }, file)
end
File.open(SAVE, 'rb') { |file| [Marshal.load(file), Marshal.load(file)] }
assert_eq(REPORTS, [[:load, 'Game_2.rxdata']], 'one report for each open file')

REPORTS.clear
Marshal.load(File._mkxp_orig_open(SAVE, 'rb', &:read))
assert_eq(REPORTS, [], 'a String is not a save file')
load_data(SAVE)
assert_eq(REPORTS, [[:load, 'Game_2.rxdata']], 'load_data of a save')

REPORTS.clear
assert_eq(broken(6.5).name, '', 'the getter repairs a Float after a load')
assert_eq(REPORTS, [[:repaired, 'Game_2.rxdata: the @name of an RPG::AudioFile was a Float when the game read it']],
          'the getter report names the last save')

# Pokemon Essentials reads the first parts of a save at start and on the
# title screen.
REPORTS.clear
File.open(SAVE, 'wb') { |file| ['Trainer', 12, Game_System.new, 'options', 'map'].each { |part| Marshal.dump(part, file) } }
File.open(SAVE, 'rb') { |file| Array.new(4) { Marshal.load(file) } }
assert_eq(REPORTS, [], 'a read of the first parts is not a save load')
File.open(SAVE, 'rb') { |file| Array.new(5) { Marshal.load(file) } }
assert_eq(REPORTS, [[:load, 'Game_2.rxdata']], 'a read to the end is a save load')

# Bones' Tales keeps Shift_JIS symbols and strings in its save.
REPORTS.clear
text = "\x83\x7B\x81\x5B\x83\x93\x83\x59".dup.force_encoding('Shift_JIS')
File.open(SAVE, 'wb') { |file| Marshal.dump({ text.to_sym => text, :system => Game_System.new(RPG::AudioFile.new(text)) }, file) }
value = File.open(SAVE, 'rb') { |file| Marshal.load(file) }
assert_eq([value[text.to_sym], value[:system].bgm.name], [text, text], 'Shift_JIS values load again')
assert_eq(REPORTS, [[:load, 'Game_2.rxdata']], 'a Shift_JIS save gives no repair')

# The next game loads the preloads again.
dump = Marshal.method(:dump).unbind
load File.join(ROOT, 'scripts', 'preload', 'save_repair.rb')
assert_eq(Marshal.method(:dump).unbind, dump, 'a second load keeps Marshal.dump')
load_windows_fs_with_reports!
REPORTS.clear
load_data(SAVE)
assert_eq(REPORTS, [[:load, 'Game_2.rxdata']], 'load_data reports after a second load of windows_fs.rb')

# Pokemon Essentials 19 and later reads the whole save at start and when
# the load screen opens. Continue gives that data to Game.load.
module SaveData; end

module Game
  def self.load(save_data)
    REPORTS << [:game_load, save_data.keys]
    raise 'a broken save' if save_data[:broken]
  end
end

REPORTS.clear
File.open(SAVE, 'wb') { |file| Marshal.dump({ :game_system => Game_System.new }, file) }
boot = File.open(SAVE, 'rb') { |file| Marshal.load(file) }
File.open(SAVE, 'rb') { |file| Marshal.load(file) }
assert_eq(REPORTS, [], 'Essentials 19: a read of the save is not a save load')
Game.load(boot)
assert_eq(REPORTS, [[:load, 'Game_2.rxdata'], [:game_load, [:game_system]]],
          'Essentials 19: Game.load is the save load, and the report comes first')
REPORTS.clear
File.open(SAVE, 'rb') { |file| Marshal.load(file) }
raised = begin
  Game.load(boot.merge(:broken => true))
rescue RuntimeError => e
  e.message
end
assert_eq(raised, 'a broken save', 'the error of Game.load goes on to the game')
assert_eq(REPORTS, [[:load, 'Game_2.rxdata'], [:game_load, [:game_system, :broken]]],
          'Essentials 19: one wrapper after a second read')

# F12 runs the game's scripts again.
$VERBOSE, verbose = nil, $VERBOSE
module Game
  def self.load(save_data)
    REPORTS << [:game_load_after_reset, save_data.size]
  end
end
$VERBOSE = verbose
REPORTS.clear
load_data(SAVE)
assert_eq(REPORTS, [], 'Essentials 19: load_data of the save is not a save load')
Game.load(boot)
assert_eq(REPORTS, [[:load, 'Game_2.rxdata'], [:game_load_after_reset, 1]],
          'Essentials 19: the wrapper goes on again after F12')

# A script wraps Game.load after our wrapper.
class << Game
  alias_method :plugin_load, :load

  def load(save_data)
    REPORTS << [:plugin_load]
    plugin_load(save_data)
  end
end
REPORTS.clear
load_data(SAVE)
looped = begin
  Game.load(boot)
  false
rescue SystemStackError
  true
end
assert_eq(looped, false, 'Essentials 19: a wrapper that a script adds after ours does not loop')
assert_eq(REPORTS, [[:load, 'Game_2.rxdata'], [:plugin_load], [:game_load_after_reset, 1]],
          'Essentials 19: each wrapper calls its own original, and only the outer one reports')

# A Game.load that takes a block and a keyword argument.
$VERBOSE, verbose = nil, $VERBOSE
module Game
  def self.load(save_data, safe: false)
    REPORTS << [:game_load_with, save_data.size, safe, yield]
  end
end
$VERBOSE = verbose
REPORTS.clear
load_data(SAVE)
Game.load(boot, safe: true) { :block }
assert_eq(REPORTS, [[:load, 'Game_2.rxdata'], [:game_load_with, 1, true, :block]],
          'Essentials 19: the wrapper passes on a block and a keyword argument')

# rubocop:enable Style/FileOpen, Security/MarshalLoad

test_passed('save_repair', 42)
