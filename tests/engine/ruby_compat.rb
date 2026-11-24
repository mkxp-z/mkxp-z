# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# Interpreter compatibility suite.
#
# The engine ships Ruby 1.8, 1.9 and 3.1, and a launcher picks one
# per game. A game that runs on one must run on the others, so every
# check here states a behaviour that has to be the same on all three.
#
# Run it on each interpreter:
#
#   tools/run-engine-tests.sh --suite ruby_compat.rb --ruby 18
#   tools/run-engine-tests.sh --suite ruby_compat.rb --ruby 19
#   tools/run-engine-tests.sh --suite ruby_compat.rb --ruby 31
#
# This file must parse on all three.

unless defined?(EngineTest)
  harness_path = File.join(File.dirname(__FILE__), 'harness.rb')
  eval(File.read(harness_path), TOPLEVEL_BINDING, harness_path) # rubocop:disable Security/Eval
end

EngineTest.suite('ruby-compat', 12)

# A path with no per-cent sign in it, and a spare argument after it.
# Pokemon Essentials writes many calls in this shape.
SPARE = 'Graphics/Pictures/Logros/img/3c.png'.freeze

def with_debug
  $DEBUG = true
  yield
ensure
  $DEBUG = false
end

EngineTest.test('$DEBUG keeps what a game writes') do
  with_debug do
    EngineTest.assert_equal(true, $DEBUG, '$DEBUG after a game turns it on')
  end
  EngineTest.assert_equal(false, $DEBUG, '$DEBUG after a game turns it off')
end

EngineTest.test('$-d follows $DEBUG') do
  with_debug do
    EngineTest.assert_equal(true, $-d, '$-d')
  end
end

EngineTest.test('sprintf drops a spare argument while $DEBUG is on') do
  with_debug do
    # rubocop:disable Style/FormatString -- the three entry points into
    # rb_str_format are the subject, so each check names its own.
    EngineTest.assert_equal(SPARE, sprintf(SPARE, 3), 'sprintf')
    # rubocop:enable Style/FormatString
  end
end

EngineTest.test('format drops a spare argument while $DEBUG is on') do
  with_debug do
    EngineTest.assert_equal(SPARE, format(SPARE, 3), 'format')
  end
end

EngineTest.test('String#% drops a spare argument while $DEBUG is on') do
  with_debug do
    # rubocop:disable Style/FormatString -- see the sprintf check above.
    EngineTest.assert_equal(SPARE, SPARE % [3], 'String#%')
    # rubocop:enable Style/FormatString
  end
end

EngineTest.test('sprintf still fills the directives it has') do
  with_debug do
    # rubocop:disable Style/FormatString, Style/RedundantFormat, Style/FormatStringToken
    # The literal result is the point: the call must still read its
    # directives, and plain tokens are what a game writes.
    EngineTest.assert_equal('a 2', sprintf('%s %d', 'a', 2), 'sprintf with directives')
    # rubocop:enable Style/FormatString, Style/RedundantFormat, Style/FormatStringToken
  end
end

# The engine loads RPG Maker's built-in classes from
# scripts/preload/rpg_*.rb. A file that fails to load only logs.
EngineTest.test('the RPG module of the game loads') do
  EngineTest.assert_equal(true, RPG.const_defined?(:Map), 'RPG::Map')
end

# tests/host/host.m names the launcher and passes no version.
EngineTest.test('$userAgent is the name of the launcher') do
  EngineTest.assert_equal('mkxp-z engine tests', $userAgent, '$userAgent')
end

EngineTest.test('$userAgentVersion is a frozen "unknown" when the launcher passes no version') do
  EngineTest.assert_equal('unknown', $userAgentVersion, '$userAgentVersion')
  EngineTest.assert_equal(true, $userAgentVersion.frozen?, '$userAgentVersion.frozen?')
end

EngineTest.load_preloads(%w[windows_fs platform_compat])

# LonaRPG's light script runs @sprite.visible = @sprite, and Galv's
# Event Pop Ups in Hello Charlotte EP1 runs font.shadow = 20. The
# test host runs as RGSS1, where Font has no shadow, so bold stands in.
EngineTest.test('a flag setter takes any value as true, and nil as false') do
  sprite = Sprite.new
  begin
    sprite.visible = sprite
    EngineTest.assert_equal(true, sprite.visible, 'visible after a Sprite')
    sprite.visible = nil
    EngineTest.assert_equal(false, sprite.visible, 'visible after nil')
  ensure
    sprite.dispose
  end
  font = Font.new
  font.bold = 20
  EngineTest.assert_equal(true, font.bold, 'bold after 20')
end

# Essentials writes $game_variables with Marshal.dump. A missing
# constant turns into IOS::NullStub, and a variable can hold it.
EngineTest.test('a save that holds the null stub dumps and loads') do
  loaded = Marshal.load(Marshal.dump([1, IOS::NullStub]))
  EngineTest.assert_equal([1, IOS::NullStub], loaded, 'loaded values')
end

EngineTest.test('a failed Marshal.dump still raises and logs the value') do
  $marshal_trace_lines = []
  class << MKXPMarshalTrace
    alias_method :engine_test_log, :log
    alias_method :engine_test_logging?, :logging?
    def log(line)
      $marshal_trace_lines.push(line)
    end

    def logging?
      true
    end
  end
  begin
    raised = begin
      Marshal.dump([1, proc {}])
      nil
    rescue TypeError => e
      e
    end
  ensure
    class << MKXPMarshalTrace
      alias_method :log, :engine_test_log
      alias_method :logging?, :engine_test_logging?
    end
  end
  EngineTest.assert_equal(TypeError, raised.class, 'error class')
  EngineTest.assert_equal(true, $marshal_trace_lines[0].to_s.include?('Array[1] = a Proc'),
                          "log line: #{$marshal_trace_lines[0]}")
  EngineTest.assert_equal(true, [true, false].include?(System.debug_log?), 'System.debug_log? is a boolean')
end

EngineTest.finish
exit
