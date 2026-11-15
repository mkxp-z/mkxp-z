#!/usr/bin/env ruby
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# Unit tests for the Pokemon Essentials cheat menu in cheat_pe19.rb:
# Get Items and Heal Party before the game makes the player, and
# with the player globals of Essentials v17 ($Trainer) and v21 ($player).
# Run: ruby tools/test_cheat_pe19.rb

require_relative 'assertion_count'

SCRIPT = File.expand_path('../scripts/postload/cheat_pe19.rb', __dir__)

def assert_eq(actual, expected, label)
  asserted
  return if actual == expected

  warn "FAIL: #{label}\n  expected: #{expected.inspect}\n  actual:   #{actual.inspect}"
  exit 1
end

module MKXP
  def self.puts(_text); end
end

module Input
  B = 13
  C = 14
  HOME = 99

  def self.trigger?(button)
    button == C
  end
end

module PBItems
  def self.maxValue
    1
  end

  def self.getName(item)
    item == 1 ? 'Potion' : ''
  end
end

# Essentials v17 reads the money of $Trainer for the money window of
# the buy screen (PScreen_Mart pbRefresh).
class PokemonMartAdapter
  def getMoney
    $Trainer.money
  end
end

class PokemonMartScene
  def pbStartBuyScene(_stock, adapter)
    adapter.getMoney
  end
end

$marts = []

class PokemonMartScreen
  def pbBuyScreen
    @scene.pbStartBuyScene(@stock, @adapter)
    $marts << @stock
  end
end

def pbScrollMap(*_args); end

class Scene_Map
  def updateMaps; end
end

class Game_Player
  def passable?(_x, _y, _d, _strict = false)
    true
  end
end

Window = Struct.new(:index, :active, :visible) do
  def update; end
end

Trainer = Struct.new(:money, :party)

class Pokemon
  attr_reader :healed

  def heal
    @healed = true
  end
end

load SCRIPT

def choose(index)
  scene = Scene_Cheat.new
  scene.instance_variable_set(:@cheat_window, Window.new(index, true, true))
  scene.update_cheat
end

# --- Before the player exists (the intro map of a v17 game) ---
$Trainer = nil
assert_eq(choose(0), -1, 'Get Items with no player closes the menu')
assert_eq($marts, [], 'Get Items with no player opens no mart')
assert_eq(choose(1), -1, 'Heal Party with no player closes the menu')

# --- Essentials v17 to v20: $Trainer ---
pokemon = Pokemon.new
$Trainer = Trainer.new(0, [pokemon])
assert_eq(choose(0), -1, 'Get Items with a player closes the menu after the mart')
assert_eq($marts, [[1]], 'Get Items with a player opens the mart with every named item')
choose(1)
assert_eq(pokemon.healed, true, 'Heal Party heals the party of $Trainer')

# --- Essentials v21: $player, no $Trainer ---
# Essentials v21 reads the money of $player (UI_PokeMart getMoney).
class PokemonMartAdapter
  def getMoney
    $player.money
  end
end

$Trainer = nil
$marts = []
pokemon = Pokemon.new
$player = Trainer.new(0, [pokemon])
assert_eq(choose(0), -1, 'Get Items with $player closes the menu after the mart')
assert_eq($marts, [[1]], 'Get Items with $player opens the mart with every named item')
choose(1)
assert_eq(pokemon.healed, true, 'Heal Party heals the party of $player')

test_passed('test_cheat_pe19', 9)
