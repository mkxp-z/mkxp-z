# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# MKXPSaveRepair on the interpreters that the engine ships. save_data
# and load_data are the engine's own, and tools/test_save_repair.rb
# runs only on the host Ruby.
#
#   tools/run-engine-tests.sh --suite save_repair.rb --ruby 18
#
# This file must parse on all three.

unless defined?(EngineTest)
  harness_path = File.join(File.dirname(__FILE__), 'harness.rb')
  eval(File.read(harness_path), TOPLEVEL_BINDING, harness_path) # rubocop:disable Security/Eval
end

EngineTest.suite('save-repair', 11)

EngineTest.load_preloads(%w[save_repair windows_fs platform_compat])

unless defined?(RPG::AudioFile)
  module RPG
    class AudioFile
      attr_accessor :name

      def initialize(name = '', volume = 100, pitch = 100)
        @name = name
        @volume = volume
        @pitch = pitch
      end
    end
  end
end

class Game_System
  attr_accessor :bgs

  def initialize(bgs = nil)
    @bgs = bgs
  end
end

REPAIR_REPORTS = [] # rubocop:disable Style/MutableConstant -- the System stand-ins add to it.

class << System
  [:report_save_load, :report_save_repaired].each do |name| # rubocop:disable Style/SymbolArray -- %i is not on Ruby 1.8.
    next unless System.respond_to?(name)

    alias_method("_repair_suite_#{name}", name)
    define_method(name) do |detail|
      REPAIR_REPORTS.push([name, detail])
      send("_repair_suite_#{name}", detail)
    end
  end
end

# rubocop:disable Security/MarshalLoad -- the suite loads only its own files.
REPAIR_FLOAT = 'the @name of an RPG::AudioFile was a Float'.freeze
REPAIR_ARRAY = 'the @name of an RPG::AudioFile was an Array'.freeze
REPAIR_DIR = File.join(Dir.pwd, 'save_repair_suite')
REPAIR_GAME = File.join(REPAIR_DIR, 'Game_2.rxdata')
Dir.mkdir(REPAIR_DIR) unless File.directory?(REPAIR_DIR)

def repair_saved_name(path)
  File.open(path, 'rb') { |file| Marshal.load(file) }.bgs.instance_variable_get(:@name)
end

def repair_broken_system(name)
  system = Game_System.new(RPG::AudioFile.new('Rain'))
  system.bgs.instance_variable_set(:@name, name)
  system
end

EngineTest.test('the engine gives the save reports to the host') do
  EngineTest.assert(System.respond_to?(:_repair_suite_report_save_load), 'System.report_save_load')
  EngineTest.assert(System.respond_to?(:_repair_suite_report_save_repaired), 'System.report_save_repaired')
end

EngineTest.test('a save with broken sound names saves no sound and tells the host once') do
  REPAIR_REPORTS.clear
  system = repair_broken_system(1.5)
  other = repair_broken_system(2.5)
  File.open(REPAIR_GAME, 'wb') do |file|
    Marshal.dump('Gold', file)
    Marshal.dump(system, file)
  end
  saved = File.open(REPAIR_GAME, 'rb') { |file| [Marshal.load(file), Marshal.load(file)] }
  EngineTest.assert_equal('', saved[1].bgs.instance_variable_get(:@name), 'the saved name')
  EngineTest.assert_equal('', other.bgs.instance_variable_get(:@name), 'the second broken name')
  EngineTest.assert_equal([[:report_save_repaired, "Game_2.rxdata: #{REPAIR_FLOAT}"],
                           [:report_save_load, 'Game_2.rxdata']], REPAIR_REPORTS, 'reports')
end

EngineTest.test('save_data repairs the save too') do
  REPAIR_REPORTS.clear
  save_data(repair_broken_system([1, 2]), REPAIR_GAME)
  EngineTest.assert_equal('', repair_saved_name(REPAIR_GAME), 'the saved name')
  EngineTest.assert_equal([:report_save_repaired, "Game_2.rxdata: #{REPAIR_ARRAY}"],
                          REPAIR_REPORTS[0], 'report')
end

EngineTest.test('a correct save keeps its names and sends no repair') do
  REPAIR_REPORTS.clear
  File.open(REPAIR_GAME, 'wb') { |file| Marshal.dump(Game_System.new(RPG::AudioFile.new('Route 30')), file) }
  EngineTest.assert_equal('Route 30', repair_saved_name(REPAIR_GAME), 'the saved name')
  EngineTest.assert_equal([[:report_save_load, 'Game_2.rxdata']], REPAIR_REPORTS, 'reports')
end

# Bones' Tales saves symbols and strings in Shift_JIS.
EngineTest.test('a save with Shift_JIS symbols and strings goes through') do
  REPAIR_REPORTS.clear
  text = "\x83\x7B\x81\x5B\x83\x93\x83\x59"
  text.force_encoding('Shift_JIS') if text.respond_to?(:force_encoding)
  value = { text.to_sym => text, :system => Game_System.new(RPG::AudioFile.new(text)) }
  File.open(REPAIR_GAME, 'wb') { |file| Marshal.dump(value, file) }
  saved = File.open(REPAIR_GAME, 'rb') { |file| Marshal.load(file) }
  EngineTest.assert_equal(text, saved[text.to_sym], 'the string')
  EngineTest.assert_equal(text, saved[:system].bgs.name, 'the sound name')
  EngineTest.assert_equal([[:report_save_load, 'Game_2.rxdata']], REPAIR_REPORTS, 'reports')
end

EngineTest.test('only a file that holds a Game_System counts as a save load') do
  REPAIR_REPORTS.clear
  options = File.join(REPAIR_DIR, 'Options.dt')
  File.open(options, 'wb') { |file| Marshal.dump({ :volume => 80 }, file) }
  File.open(options, 'rb') { |file| Marshal.load(file) }
  load_data(options)
  Marshal.load(File.open(REPAIR_GAME, 'rb', &:read)) # rubocop:disable Style/FileRead -- File.binread is not on Ruby 1.8.
  EngineTest.assert_equal([], REPAIR_REPORTS, 'reports of other files')
  save_data([Game_System.new], REPAIR_GAME)
  load_data(REPAIR_GAME)
  EngineTest.assert_equal([[:report_save_load, 'Game_2.rxdata']], REPAIR_REPORTS, 'report of load_data')
end

# Pokemon Essentials reads the first parts of a save at start and on the
# title screen.
EngineTest.test('only a read to the end of a save counts as a save load') do
  REPAIR_REPORTS.clear
  save = File.join(REPAIR_DIR, 'Game_3.rxdata')
  File.open(save, 'wb') do |file|
    ['Trainer', 12, Game_System.new, 'options', 'map'].each { |part| Marshal.dump(part, file) }
  end
  File.open(save, 'rb') { |file| Array.new(4) { Marshal.load(file) } }
  EngineTest.assert_equal([], REPAIR_REPORTS, 'reports of the first parts')
  File.open(save, 'rb') { |file| Array.new(5) { Marshal.load(file) } }
  EngineTest.assert_equal([[:report_save_load, 'Game_3.rxdata']], REPAIR_REPORTS, 'report of the whole save')
end

EngineTest.test('a broken name that the game reads tells the host') do
  REPAIR_REPORTS.clear
  postload = $LOAD_PATH.map { |dir| dir[/\A.*\.app/] }.compact.first
  path = File.join(postload, 'Assets.bundle', 'Postload', 'audio_file_name_repair.rb')
  eval(File.read(path), TOPLEVEL_BINDING, path) # rubocop:disable Security/Eval
  load_data(REPAIR_GAME)
  sound = RPG::AudioFile.new
  sound.instance_variable_set(:@name, 0.5)
  EngineTest.assert_equal(['', ''], [sound.name, sound.name], 'the names that the game reads')
  EngineTest.assert_equal(
    [[:report_save_load, 'Game_2.rxdata'],
     [:report_save_repaired, "Game_2.rxdata: #{REPAIR_FLOAT} when the game read it"]],
    REPAIR_REPORTS, 'reports'
  )
end

# Pokemon Essentials 19 and later reads the whole save at start, and
# Continue gives that data to Game.load.
EngineTest.test('in Essentials 19 and later, Game.load is the save load') do
  REPAIR_REPORTS.clear
  begin
    Object.const_set(:SaveData, Module.new)
    game = Module.new
    game.instance_eval do
      def load(save_data)
        REPAIR_REPORTS.push([:game_load, save_data.size])
      end
    end
    Object.const_set(:Game, game)
    save = File.join(REPAIR_DIR, 'Game_4.rxdata')
    File.open(save, 'wb') { |file| Marshal.dump({ :game_system => Game_System.new }, file) }
    data = File.open(save, 'rb') { |file| Marshal.load(file) }
    File.open(save, 'rb') { |file| Marshal.load(file) }
    EngineTest.assert_equal([], REPAIR_REPORTS, 'reports of the reads at start and on the load screen')
    Game.load(data)
    EngineTest.assert_equal([[:report_save_load, 'Game_4.rxdata'], [:game_load, 1]], REPAIR_REPORTS,
                            'reports of Game.load')
    # F12 runs the game's scripts again.
    game.instance_eval do
      def load(save_data)
        REPAIR_REPORTS.push([:game_load_after_reset, save_data.size])
      end
    end
    REPAIR_REPORTS.clear
    load_data(save)
    EngineTest.assert_equal([], REPAIR_REPORTS, 'reports of load_data')
    Game.load(data)
    EngineTest.assert_equal([[:report_save_load, 'Game_4.rxdata'], [:game_load_after_reset, 1]],
                            REPAIR_REPORTS, 'reports of Game.load after F12')
  ensure
    Object.send(:remove_const, :SaveData)
    Object.send(:remove_const, :Game)
  end
end

# A script can wrap Game.load after our wrapper.
EngineTest.test('a wrapper that a script adds over ours does not loop, and a block goes through') do
  REPAIR_REPORTS.clear
  begin
    Object.const_set(:SaveData, Module.new)
    game = Module.new
    game.instance_eval do
      def load(save_data)
        REPAIR_REPORTS.push([:game_load, save_data.size, yield])
      end
    end
    Object.const_set(:Game, game)
    save = File.join(REPAIR_DIR, 'Game_4.rxdata')
    File.open(save, 'wb') { |file| Marshal.dump({ :game_system => Game_System.new }, file) }
    data = load_data(save)
    (class << game; self; end).send(:alias_method, :plugin_load, :load)
    game.instance_eval do
      def load(save_data, &block)
        REPAIR_REPORTS.push([:plugin_load])
        plugin_load(save_data, &block)
      end
    end
    load_data(save)
    looped = begin
      Game.load(data) { :block }
      false
    rescue SystemStackError
      true
    end
    EngineTest.assert_equal(false, looped, 'a loop')
    EngineTest.assert_equal(
      [[:report_save_load, 'Game_4.rxdata'], [:plugin_load], [:game_load, 1, :block]],
      REPAIR_REPORTS, 'reports of Game.load'
    )
  ensure
    Object.send(:remove_const, :SaveData)
    Object.send(:remove_const, :Game)
  end
end

# The VM outlives a game, and the next game loads the preloads again.
EngineTest.test('a second load keeps the Marshal.dump of platform_compat.rb') do
  EngineTest.load_preloads(%w[save_repair windows_fs platform_compat])
  probe = Class.new
  probe.send(:define_method, :_dump) do |_limit|
    $repair_suite_traced = Thread.current[:mkxp_marshal_dump]
    ''
  end
  Object.const_set(:RepairSuiteProbe, probe) unless defined?(RepairSuiteProbe)
  Marshal.dump(RepairSuiteProbe.new)
  EngineTest.assert_equal(true, $repair_suite_traced, 'the dump goes through MKXPMarshalTrace')
end
# rubocop:enable Security/MarshalLoad

EngineTest.finish
exit
