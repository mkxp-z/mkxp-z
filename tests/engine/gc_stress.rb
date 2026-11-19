# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# Code like a game keeps its objects whole while the garbage collector
# runs at each allocation in the steps of a battle. The check compares
# the result with the same rounds run with the collector off.
#
#   tools/run-engine-tests.sh --suite gc_stress.rb --ruby 18
#
# This file must parse on all three interpreters.

unless defined?(EngineTest)
  harness_path = File.join(File.dirname(__FILE__), 'harness.rb')
  eval(File.read(harness_path), TOPLEVEL_BINDING, harness_path) # rubocop:disable Security/Eval
end

EngineTest.suite('gc-stress', 1)

STRESS_ROUNDS = 3
STRESS_SOUND_EFFECTS = 16
STRESS_FOES = 3
STRESS_TURNS = 2
STRESS_LOG_SIZE = 40
STRESS_FRAME = 20

def stressed
  GC.stress = true
  yield
ensure
  GC.stress = false
end

module Nicknamed
  def nickname
    "#{name} the brave"
  end
end

class Battler
  attr_reader :name, :hp, :moves, :stats, :tone

  def initialize(name, level)
    @name = name
    @hp = level * 3
    @moves = %w[Tackle Growl Ember Surf].map { |move| "#{move} #{level}" }
    @stats = { :attack => level * 2, :speed => level / 3.0, :types => %w[fire water] }
    @tone = Tone.new(level % 255, 0, 0, 0)
  end

  def speed
    @stats[:speed]
  end

  def hit(damage)
    @hp -= damage
    @moves = @moves.reverse
    @stats[:speed] += 0.5
    @stats[:types] = @stats[:types].sort.uniq
  end
end

def sound(name, volume)
  RPG::AudioFile.new(name, volume, 100)
end

def new_save
  map = Table.new(16, 8, 2)
  16.times { |x| 8.times { |y| map[x, y, 0] = (x * 8) + y } }
  party = Array.new(6) { |i| Battler.new("Mon #{i}", i + 5) }
  # A clone copies the singleton class. The broken lazy sweep of #55 crashed only here.
  party.each_with_index { |battler, i| battler.extend(Nicknamed) if i.even? }
  save = {
    'system' => {},
    'party' => party,
    'lead' => party.first,
    'switches' => Array.new(100) { |i| (i % 3).zero? },
    'variables' => Array.new(100) { |i| i * 7 },
    'map' => map,
    'flashes' => [Color.new(255, 0, 0, 128)],
    'log' => [],
    'played' => []
  }
  save['system']['save'] = save
  save
end

def play_map_sounds(system, round)
  system['bgm'] = sound("Town #{round}", 100)
  system['bgs'] = sound("Rain #{round}", 80)
  system['se'] = Array.new(STRESS_SOUND_EFFECTS) { |i| sound("Step #{round} #{i}", 60) }
end

def pause_sounds(system)
  memorized = ([system['bgm'], system['bgs']] + system['se']).map(&:clone)
  system['bgm'] = system['bgs'] = nil
  system['se'] = []
  memorized
end

def fight_turn(fighters, log, round, turn)
  fighters.each_with_index do |attacker, i|
    target = fighters[(i + turn + 1) % fighters.size]
    damage = (((round * 7) + (turn * 3) + i) % 13) + 1
    target.hit(damage)
    log << "#{attacker.name} hit #{target.name} for #{damage}"
  end
end

def fight(save, party, round)
  foes = Array.new(STRESS_FOES) { |i| Battler.new("Foe #{round} #{i}", 5 + ((round + i) % 50)) }
  log = save['log']
  STRESS_TURNS.times do |turn|
    fighters = (party + foes).sort_by { |battler| [-battler.speed, battler.name] }
    fight_turn(fighters, log, round, turn)
    save['map'][turn, round % 8, 1] = log.size
  end
  save['flashes'] << Color.new(round * 10, (round * 37) % 256, 0, 255)
  log.shift while log.size > STRESS_LOG_SIZE
end

# Pokemon Essentials keeps a clone of the playing sounds through a
# battle (getPlayingBGS, bgs_pause, the battle, bgs_resume). After the
# pause only the clones hold the names.
def battle(save, round)
  system = save['system']
  play_map_sounds(system, round)
  stressed { Array.new(STRESS_FRAME) { 'x' * 16 } }
  memorized = stressed { pause_sounds(system) }
  stressed do
    party = save['party'].map(&:clone)
    fight(save, party, round)
    save['party'] = party
    save['lead'] = party.first
    system['bgm'] = memorized[0]
    system['bgs'] = memorized[1]
    system['se'] = memorized[2..-1]
    save['played'].concat(memorized)
  end
end

def play
  save = new_save
  STRESS_ROUNDS.times { |round| battle(save, round) }
  save
end

def unstressed_save
  GC.start
  GC.disable
  play
ensure
  GC.enable
end

def immediate?(value)
  value.nil? || value.equal?(true) || value.equal?(false) || value.is_a?(Symbol) || value.is_a?(Numeric)
end

# The live object at an object id, or nil when its slot was freed. A
# method call on a freed slot can crash the interpreter, so the check
# reads each object by its id first.
def alive_at(id)
  ObjectSpace._id2ref(id)
rescue RangeError
  nil
end

def alive?(value)
  immediate?(value) || alive_at(value.object_id).equal?(value)
end

def array_difference(expected, actual, path, pairs)
  return "#{path} has #{actual.size} items, not #{expected.size}" unless expected.size == actual.size

  expected.each_index do |i|
    found = difference(expected[i], actual[i], "#{path}[#{i}]", pairs)
    return found if found
  end
  nil
end

def hash_difference(expected, actual, path, pairs)
  return "#{path} has #{actual.size} keys, not #{expected.size}" unless expected.size == actual.size

  expected.each_pair do |key, value|
    return "#{path} has no key #{key.inspect}" unless actual.key?(key)

    found = difference(value, actual[key], "#{path}[#{key.inspect}]", pairs)
    return found if found
  end
  nil
end

def object_difference(expected, actual, path, pairs)
  methods = actual.singleton_methods.sort
  return "#{path} has the methods #{methods.inspect}" unless expected.singleton_methods.sort == methods

  names = expected.instance_variables.sort
  return "#{path} has other instance variables" unless names == actual.instance_variables.sort

  names.each do |name|
    found = difference(expected.instance_variable_get(name), actual.instance_variable_get(name),
                       "#{path}.#{name}", pairs)
    return found if found
  end
  nil
end

def value_difference(expected, actual, path)
  # Color#== is always false on Ruby 1.8.
  same = if immediate?(expected) || expected.is_a?(String)
           expected == actual
         else
           Marshal.dump(expected) == Marshal.dump(actual)
         end
  same ? nil : "#{path} is #{actual.inspect}, not #{expected.inspect}"
end

# The first path where `actual` differs from `expected`, or nil.
# `pairs` maps each object to its partner in the other tree, in both
# directions, so an object is shared in `actual` only where it is
# shared in `expected`.
def difference(expected, actual, path, pairs = {})
  return "#{path} is a freed object" unless alive?(actual)
  return "#{path} is a #{actual.class}, not a #{expected.class}" unless actual.instance_of?(expected.class)
  return value_difference(expected, actual, path) if immediate?(expected)

  partner = pairs[[:expected, expected.object_id]]
  return partner.equal?(actual) ? nil : "#{path} is not shared as in the original" if partner
  return "#{path} is shared, but not in the original" if pairs.key?([:actual, actual.object_id])

  pairs[[:expected, expected.object_id]] = actual
  pairs[[:actual, actual.object_id]] = expected
  case expected
  when String, Color, Tone, Rect, Table then value_difference(expected, actual, path)
  when Array then array_difference(expected, actual, path, pairs)
  when Hash then hash_difference(expected, actual, path, pairs)
  else object_difference(expected, actual, path, pairs)
  end
end

EngineTest.test('battles in stress mode keep the party, the log and the memorized sounds') do
  stressed_save = play
  found = difference(unstressed_save, stressed_save, 'save')
  EngineTest.assert(found.nil?, found)
end

EngineTest.finish
