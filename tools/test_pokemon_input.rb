#!/usr/bin/env ruby
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# Unit tests for pokemon_input.rb: the Pokemon Essentials marker, and
# the name screen with the "In-game keyboard" setting on.
# Run: ruby tools/test_pokemon_input.rb

require 'tmpdir'
require_relative 'assertion_count'

SCRIPT = File.expand_path('../scripts/postload/pokemon_input.rb', __dir__)

def assert_eq(actual, expected, label)
  asserted
  return if actual == expected

  warn "FAIL: #{label}\n  expected: #{expected.inspect}\n  actual:   #{actual.inspect}"
  exit 1
end

$PokemonSystem = Object.new

$managed_dir = nil
$mkxp_log = []

module MKXP
  def self.use_in_game_keyboard?
    true
  end

  def self.managed_config_dir
    $managed_dir
  end

  def self.puts(message)
    $mkxp_log << message
  end
end

module Input
  def self.update; end
end

class Viewport
  def self.all
    @all ||= []
  end

  attr_reader :disposed

  def initialize
    Viewport.all << self
  end

  def dispose
    @disposed = true
  end
end

class Grid
  def update; end
end

$fades = []

def pbFadeInAndShow(sprites)
  $fades << sprites.keys
end

def pbDisposeSpriteHash(sprites)
  sprites.clear
end

# The stock Essentials scene: it builds the letter grid when
# USEKEYBOARD is off.
STOCK_SCENE = <<-RUBY.freeze
  class PokemonEntryScene
    USEKEYBOARD = true

    def pbStartScene(_helptext, _minlength, _maxlength, _initial, _subject = 0, _pokemon = nil)
      @sprites = {}
      @viewport = Viewport.new
      @sprites['entry'] = USEKEYBOARD ? :keyboard_window : :plain_window
      @sprites['entry2'] = Grid.new unless USEKEYBOARD
      pbFadeInAndShow(@sprites)
    end

    def pbEntry
      return :typed if USEKEYBOARD

      @sprites['entry2'].update
      :grid
    end
  end
RUBY

# Pokemon Xenoverse's 225_NewNameEntry.rb: the same scene, but its
# pbStartScene never builds the letter grid.
XENOVERSE_SCENE = STOCK_SCENE.sub(/^.*Grid\.new.*\n/, '')

def load_game(scene_source)
  Object.send(:remove_const, :PokemonEntryScene) if defined?(PokemonEntryScene)
  Object.class_eval(scene_source)
  load SCRIPT
  $fades = []
  PokemonEntryScene.new
end

scene = load_game(XENOVERSE_SCENE)
scene.pbStartScene('Your name?', 1, 7, '', 1)
assert_eq(scene.pbEntry, :typed, 'a scene without a letter grid takes typed text')
assert_eq($fades, [%w[entry]], 'the screen fades in once')
assert_eq(scene.instance_variable_get(:@sprites)['entry'], :keyboard_window,
          'the screen is built again with the typing window')
assert_eq(Viewport.all.map(&:disposed), [true, nil], 'the first viewport is disposed')
assert_eq(PokemonEntryScene::USEKEYBOARD, true, 'typing mode stays for the session')

$fades = []
scene = PokemonEntryScene.new
scene.pbStartScene('Your name?', 1, 7, '', 1)
assert_eq(scene.pbEntry, :typed, 'the next name screen also takes typed text')
assert_eq($fades.length, 1, 'the next name screen fades in once')

scene = load_game(STOCK_SCENE)
scene.pbStartScene('Your name?', 1, 7, '', 1)
assert_eq(scene.pbEntry, :grid, 'a stock scene keeps the letter grid')
assert_eq($fades, [%w[entry entry2]], 'a stock scene fades in once, with the grid')
assert_eq(PokemonEntryScene::USEKEYBOARD, false, 'a stock scene stays in grid mode')

# F12 runs the game scripts again, which puts the game's pbStartScene
# back, then runs this script again.
Object.class_eval(XENOVERSE_SCENE)
load SCRIPT
$fades = []
scene = PokemonEntryScene.new
scene.pbStartScene('Your name?', 1, 7, '', 1)
assert_eq(scene.pbEntry, :typed, 'after a reset, a scene without a letter grid takes typed text')
assert_eq($fades.length, 1, 'after a reset, the screen fades in once')

load SCRIPT
$fades = []
scene = PokemonEntryScene.new
scene.pbStartScene('Your name?', 1, 7, '', 1)
assert_eq($fades.length, 1, 'a second load of the script does not wrap the scene twice')

$fades = []
scene.pbFadeInAndShow({})
assert_eq($fades.length, 1, 'a fade outside the name screen goes through')

scene = load_game(XENOVERSE_SCENE.sub(/^.*Viewport\.new.*\n/, ''))
scene.pbStartScene('Your name?', 1, 7, '', 1)
assert_eq(scene.pbEntry, :typed, 'a scene with no viewport takes typed text')

scene = load_game(XENOVERSE_SCENE.sub(/^.*USEKEYBOARD = true.*\n/, ''))
scene.pbStartScene('Your name?', 1, 7, '', 1)
assert_eq(scene.pbEntry, :typed, 'a scene with no USEKEYBOARD of its own takes typed text')
assert_eq(USEKEYBOARDTEXTENTRY, true, 'the top-level setting also goes to typing mode')

# Ruby 1.8 has no File.write.
def without_file_write
  IO.singleton_class.send(:alias_method, :mkxp_test_write, :write)
  IO.singleton_class.send(:undef_method, :write)
  yield
ensure
  IO.singleton_class.send(:alias_method, :write, :mkxp_test_write)
end

def check_marker(ruby)
  Dir.mktmpdir do |dir|
    $managed_dir = dir
    $mkxp_log = []
    load SCRIPT
    assert_eq($mkxp_log, [], "#{ruby}: the script logs no error")
    assert_eq(File.exist?("#{dir}/.pokemon_essentials_detected"), true, "#{ruby}: the script writes the marker")
  end
ensure
  $managed_dir = nil
end

without_file_write { check_marker('Ruby 1.8') }

test_passed('test_pokemon_input', 19)
