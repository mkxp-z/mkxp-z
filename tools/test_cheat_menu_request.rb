#!/usr/bin/env ruby
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# Unit tests for the HOME key check of the four cheat menus, with a
# game script that replaces Input.trigger? in the shape of Yanfly's
# "YEA - Debug Extension". That script sends
# each key it does not know to `key.between?(130, 158)`, so a Symbol
# such as :HOME raises ArgumentError, and an Integer such as 43 reads
# a Windows key that is never down.
# Run: ruby tools/test_cheat_menu_request.rb

require 'rbconfig'
require_relative 'assertion_count'

SCRIPTS = File.expand_path('../scripts', __dir__)
CHEAT_SCRIPTS = %w[cheat_rpgmxp cheat_rpgmvx cheat_rpgmvxace cheat_pe19].freeze

# Each cheat script defines its own Scene_Cheat, so each one runs in
# its own process.
unless ARGV.first
  CHEAT_SCRIPTS.each do |name|
    asserted
    next if system(RbConfig.ruby, __FILE__, name)

    warn "FAIL: #{name}"
    exit 1
  end
  test_passed('test_cheat_menu_request', CHEAT_SCRIPTS.size)
  exit
end

NAME = ARGV.first
RGSS3 = NAME == 'cheat_rpgmvxace'

def assert_eq(actual, expected, label)
  asserted
  return if actual == expected

  warn "FAIL: #{NAME}: #{label}\n  expected: #{expected.inspect}\n  actual:   #{actual.inspect}"
  exit 1
end

module System
  @cheats = false

  def self.puts(_text); end

  def self.cheats=(enabled)
    @cheats = enabled
  end

  def self.cheats_enabled?
    @cheats
  end
end

# The engine's Input module. In RGSS3 the button constants are
# Symbols, and the native methods take either form.
module Input
  CODES = { :C => 13, :HOME => 43 }.freeze
  C = RGSS3 ? :C : 13
  HOME = RGSS3 ? :HOME : 43

  @down = []
  @before = []

  def self.code(button)
    button.is_a?(Symbol) ? CODES.fetch(button) : button
  end

  def self.hold(*buttons)
    @before = @down
    @down = buttons.map { |button| code(button) }
  end

  def self.jpress?(button)
    @down.include?(code(button))
  end

  def self.jtrigger?(button)
    jpress?(button) && !@before.include?(code(button))
  end

  class << self
    alias_method :press?, :jpress?
    alias_method :trigger?, :jtrigger?
  end
end

# The game script.
module Input
  DEFAULT = [:DOWN, :LEFT, :RIGHT, :UP, :A, :B, :C, :X, :Y, :Z, :L, :R,
             :SHIFT, :CTRL, :ALT, :F5, :F6, :F7, :F8, :F9].freeze

  class << self
    alias_method :game_original_trigger?, :trigger?
    alias_method :game_original_press?, :press?
  end

  def self.default_key?(key)
    return true if key.is_a?(Integer) && key < 30

    DEFAULT.include?(key)
  end

  def self.adjust_key(key)
    key -= 130 if key.between?(130, 158)
    key
  end

  def self.trigger?(key)
    return game_original_trigger?(key) if default_key?(key)

    adjust_key(key)
    false
  end

  def self.press?(key)
    return game_original_press?(key) if default_key?(key)

    adjust_key(key)
    false
  end
end

class Window_Base; end
class Window_Selectable < Window_Base; end
class Window_Command < Window_Selectable; end
class Scene_MenuBase; end
class Scene_Map
  def updateMaps; end
end
module PBItems; end
class PokemonMartAdapter; end
class PokemonMartScene; end
class PokemonMartScreen; end

class Game_Player
  def update; end

  def straighten; end

  def passable?(_x, _y, _d, _strict = false)
    true
  end
end

module Sound
  def self.play_ok; end
end

module SceneManager
  def self.call(scene)
    $scene = scene
  end
end

load File.join(SCRIPTS, 'preload', 'mkxp_wrap.rb')
load File.join(SCRIPTS, 'postload', "#{NAME}.rb")

# The real menus draw windows. Only the call matters here. The
# Essentials menu runs pbUpdateSceneMap on each of its frames, which
# runs Scene_Map#updateMaps again.
$menus_opened = 0
$tap_in_menu = false
MAP = Scene_Map.new

class Scene_Cheat
  def main
    $menus_opened += 1
    return unless $tap_in_menu

    $tap_in_menu = false
    Input.hold
    MAP.updateMaps
    Input.hold(Input::HOME)
    MAP.updateMaps
  end
end

# The XP, VX and VX Ace menus open as the next scene.
def map_frame
  before = $menus_opened
  $scene = nil
  if NAME == 'cheat_pe19'
    MAP.updateMaps
  else
    Game_Player.new.update
  end
  $menus_opened += 1 if $scene == Scene_Cheat || $scene.is_a?(Scene_Cheat)
  $menus_opened > before
end

System.cheats = false
Input.hold
map_frame
Input.hold(Input::HOME)
assert_eq(map_frame, false, 'HOME with cheats off opens no menu')

# The app turns cheats on and taps HOME at almost the same time.
System.cheats = true
Input.hold(Input::HOME)
assert_eq(map_frame, true, 'cheats on while HOME is held opens the menu')

Input.hold
map_frame
Input.hold(Input::HOME)
assert_eq(map_frame, true, 'HOME with cheats on opens the menu')
assert_eq(map_frame, false, 'a held HOME opens the menu once')

# Pokemon Essentials fan games call Input.update in the middle of a
# frame (see pokemon_input.rb), so the native trigger edge can be
# gone before the map reads it. The press must still open the menu.
Input.hold
map_frame
Input.hold(Input::HOME)
Input.hold(Input::HOME)
assert_eq(map_frame, true, 'HOME after a second Input.update opens the menu')

Input.hold(Input::C)
assert_eq(map_frame, false, 'another key opens no menu')

Input.hold
map_frame
$tap_in_menu = true
Input.hold(Input::HOME)
map_frame
assert_eq($menus_opened, 4, 'HOME in the open menu opens no second menu')

test_passed(NAME, 7)
