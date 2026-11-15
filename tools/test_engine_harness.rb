#!/usr/bin/env ruby
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# Tests for tests/engine/harness.rb, the reporting layer the
# in-engine suites use.
#
# A harness that reports the wrong outcome turns a broken engine into
# a green run. Two cases matter most. The engine's error classes
# (MKXPError and friends) descend from Exception, not from
# StandardError, so a rescue that names StandardError lets them past
# and kills the suite mid-run. And an operation the engine has not
# implemented must report PEND, not FAIL, or a port cannot tell a
# missing feature from a defect.
#
# The third case: a check that asserts nothing must not report ok,
# and PEND must stay behind a switch the suite turns on. Both keep a
# green run from meaning less than it reads.
#
# Run: ruby mkxp-z-apple-mobile/tools/test_engine_harness.rb

require 'fileutils'
require 'tmpdir'
require_relative 'assertion_count'

ROOT = File.expand_path('..', __dir__)

$failures = []

def assert(condition, label)
  asserted
  return if condition

  $failures << label
end

def assert_eq(actual, expected, label)
  asserted
  return if actual == expected

  $failures << "#{label}\n  expected: #{expected.inspect}\n  actual:   #{actual.inspect}"
end

# Stands in for the engine's System module. The harness prints
# through it, so collecting the lines here is the whole observation
# surface.
module System
  # rubocop:disable Style/MutableConstant -- the fake collects the
  # harness output as it runs, so this array has to stay writable.
  LINES = []
  # rubocop:enable Style/MutableConstant

  def self.puts(line)
    LINES << line
  end

  def self.reset
    LINES.clear
  end

  def self.ruby_version
    '1.8'
  end
end

# Stands in for the engine's Bitmap class. The harness reads the two
# size values at the start of a suite, and it creates and disposes
# bitmaps for the suites. Every instance registers itself, so a test
# below can check that nothing was left undisposed.
class Bitmap
  # rubocop:disable Style/MutableConstant -- the fake collects every
  # bitmap it makes, so this array has to stay writable.
  MADE = []
  # rubocop:enable Style/MutableConstant

  def self.max_size
    2048
  end

  def self.real_max_size
    16_384
  end

  def self.reset
    MADE.clear
  end

  attr_reader :width, :height, :fills

  def initialize(width, height)
    @width = width
    @height = height
    @fills = []
    @disposed = false
    MADE << self
  end

  def fill_rect(x, y, width, height, colour)
    @fills << [x, y, width, height, colour]
  end

  def dispose
    @disposed = true
  end

  def disposed?
    @disposed
  end
end

# MKXPError as the engine defines it: a direct child of Exception.
class MKXPError < Exception # rubocop:disable Lint/InheritException
end

load File.join(ROOT, 'tests', 'engine', 'harness.rb')

def lines_for
  System.reset
  yield
  System::LINES.map { |line| line.sub('[TEST] ', '') }
end

# --- A passing check prints one ok line ---
lines = lines_for { EngineTest.test('a check') { EngineTest.assert(true, 'never seen') } }
assert_eq(lines, ['ok a check'], 'a passing check prints ok')

# --- A failed assertion prints FAIL with the reason ---
lines = lines_for { EngineTest.test('a check') { EngineTest.assert(false, 'the reason') } }
assert_eq(lines, ['FAIL a check -- the reason'], 'a failed assertion prints FAIL')

# --- An unsupported operation prints PEND where the suite allows it ---
EngineTest.pending_allowed = true
lines = lines_for do
  EngineTest.test('a check') do
    raise MKXPError, 'Operation not supported for mega surfaces'
  end
end
assert_eq(lines, ['PEND a check -- Operation not supported for mega surfaces'],
          'an unsupported operation prints PEND')

# --- ...and FAIL everywhere else ---
EngineTest.pending_allowed = false
lines = lines_for do
  EngineTest.test('a check') do
    raise MKXPError, 'Operation not supported for mega surfaces'
  end
end
assert_eq(lines,
          ['FAIL a check -- MKXPError: Operation not supported for mega surfaces'],
          'an unsupported operation is a failure where the suite expects the work')

# --- A check that asserts nothing is not a pass ---
lines = lines_for { EngineTest.test('a check') { nil } }
assert_eq(lines, ['FAIL a check -- the check asserted nothing'],
          'a check with no assertion fails')

lines = lines_for { EngineTest.test('a check') { 2 + 2 } }
assert_eq(lines, ['FAIL a check -- the check asserted nothing'],
          'work with no assertion is still no assertion')

# --- Any other engine error is a failure, and does not escape ---
lines = lines_for do
  EngineTest.test('a check') { raise MKXPError, 'disposed bitmap' }
end
assert_eq(lines, ['FAIL a check -- MKXPError: disposed bitmap'],
          'an engine error that is not a missing feature prints FAIL')

# --- The summary counts each outcome once ---
System.reset
EngineTest.suite('demo', 3)
EngineTest.pending_allowed = true
EngineTest.test('one') { EngineTest.assert(true, 'yes') }
EngineTest.test('two') { EngineTest.assert(false, 'no') }
EngineTest.test('three') { raise MKXPError, 'Operation not supported for mega surfaces' }
failures = EngineTest.finish
summary = System::LINES.last
assert_eq(summary, '[TEST] DONE passed=1 failed=1 pending=1', 'the summary counts every outcome')
assert_eq(failures, 1, 'finish returns the failure count')

# --- A suite states its plan up front ---
System.reset
EngineTest.suite('demo', 7)
assert(System::LINES.include?('[TEST] PLAN 7'), 'suite prints the plan')

# --- A suite prints the game folder that the host opened ---
ENV['MKXP_TEST_GAME'] = 'legacy-methods'
System.reset
EngineTest.suite('demo', 1)
assert(System::LINES.include?('[TEST] INFO game=legacy-methods'), 'suite prints the game folder')
ENV.delete('MKXP_TEST_GAME')

# --- A suite prints the interpreter, which RUBY_VERSION does not name ---
System.reset
EngineTest.suite('demo', 1)
assert(System::LINES.include?('[TEST] INFO interpreter=1.8'), 'suite prints the interpreter')

# --- Running fewer checks than planned is a failure ---
System.reset
EngineTest.suite('demo', 3)
EngineTest.test('only one') { EngineTest.assert(true, 'yes') }
failures = EngineTest.finish
assert_eq(failures, 1, 'a short run fails')
assert(System::LINES.include?('[TEST] FAIL <plan> -- planned 3 checks, ran 1'),
       'the short run says how many checks it lost')
assert_eq(System::LINES.last, '[TEST] DONE passed=1 failed=1 pending=0',
          'the plan failure lands in the summary')

# --- A suite that registered nothing cannot report a pass ---
System.reset
EngineTest.suite('demo', 0)
failures = EngineTest.finish
assert_eq(failures, 0, 'an empty plan matches an empty run')
assert_eq(System::LINES.last, '[TEST] DONE passed=0 failed=0 pending=0',
          'an empty suite reports nothing passed')

# --- suite resets the pending switch, so it cannot leak between suites ---
EngineTest.pending_allowed = true
EngineTest.suite('demo', 1)
lines = lines_for do
  EngineTest.test('a check') { raise MKXPError, 'Operation not supported for mega surfaces' }
end
assert_eq(lines.first[0, 4], 'FAIL', 'a new suite starts with PEND turned off')

# --- assert_equal and refute_equal report the values they saw ---
lines = lines_for { EngineTest.test('a check') { EngineTest.assert_equal(1, 2, 'count') } }
assert_eq(lines, ['FAIL a check -- count: expected 1, got 2'], 'assert_equal reports both values')

lines = lines_for { EngineTest.test('a check') { EngineTest.refute_equal(1, 1, 'count') } }
assert_eq(lines, ['FAIL a check -- count: expected a value other than 1'],
          'refute_equal reports the value it rejected')

# --- Pixel helpers round the float channels the engine returns ---
colour = Struct.new(:red, :green, :blue, :alpha)
bitmap = Object.new
bitmap.define_singleton_method(:get_pixel) { |_x, _y| colour.new(254.6, 0.4, 0.0, 255.0) }
assert_eq(EngineTest.pixel(bitmap, [0, 0]), [255, 0, 0, 255], 'pixel rounds every channel')

lines = lines_for do
  EngineTest.test('a check') { EngineTest.assert_pixel(bitmap, [3, 4], [0, 0, 0, 0], 'corner') }
end
assert_eq(lines, ['FAIL a check -- corner at [3, 4]: expected [0, 0, 0, 0], got [255, 0, 0, 255]'],
          'assert_pixel names the point it read')

# --- A tolerance covers a near miss but not a real difference ---
lines = lines_for do
  EngineTest.test('a check') { EngineTest.assert_pixel_near(bitmap, [0, 0], [250, 4, 0, 255], 8, 'end') }
end
assert_eq(lines, ['ok a check'], 'assert_pixel_near accepts a difference within the tolerance')

lines = lines_for do
  EngineTest.test('a check') { EngineTest.assert_pixel_near(bitmap, [0, 0], [200, 0, 0, 255], 8, 'end') }
end
assert_eq(lines.first[0, 4], 'FAIL', 'assert_pixel_near rejects a difference past the tolerance')

# --- assert_raises wants the error it names ---
lines = lines_for do
  EngineTest.test('a check') do
    EngineTest.assert_raises('disposed', 'use after dispose') { raise MKXPError, 'disposed bitmap' }
  end
end
assert_eq(lines, ['ok a check'], 'assert_raises accepts a matching error')

lines = lines_for do
  EngineTest.test('a check') { EngineTest.assert_raises('disposed', 'use after dispose') { nil } }
end
assert_eq(lines, ['FAIL a check -- use after dispose: expected an error matching "disposed", none raised'],
          'assert_raises fails when nothing is raised')

# --- owning disposes the resource, whatever the block did ---
resource = Bitmap.new(4, 4)
EngineTest.owning(resource) { nil }
assert(resource.disposed?, 'owning disposes after a block that returns')

resource = Bitmap.new(4, 4)
begin
  EngineTest.owning(resource) { raise MKXPError, 'a broken check' }
rescue MKXPError
  nil
end
assert(resource.disposed?, 'owning disposes after a block that raises')

# --- owning leaves an already disposed resource alone ---
resource = Bitmap.new(4, 4)
EngineTest.owning(resource, &:dispose)
assert(resource.disposed?, 'owning accepts a resource the block disposed')

# --- bitmap makes the size it is asked for, and fills it on request ---
Bitmap.reset
EngineTest.bitmap(8, 4) { |made| assert_eq([made.width, made.height], [8, 4], 'bitmap size') }
assert_eq(Bitmap::MADE.size, 1, 'bitmap makes one bitmap')
assert_eq(Bitmap::MADE.first.fills, [], 'bitmap leaves an uncoloured bitmap clear')
assert(Bitmap::MADE.first.disposed?, 'bitmap disposes what it made')

Bitmap.reset
EngineTest.bitmap(8, 4, :red) { nil }
assert_eq(Bitmap::MADE.first.fills, [[0, 0, 8, 4, :red]], 'bitmap fills the whole bitmap')

# --- run_suites.rb runs every suite, also after one that raises ---
Dir.mktmpdir do |dir|
  File.write(File.join(dir, 'first.rb'), <<~RUBY)
    EngineTest.suite('first', 1)
    EngineTest.test('a') { EngineTest.assert(true, 'a') }
    EngineTest.finish
    exit
  RUBY
  File.write(File.join(dir, 'broken.rb'), <<~RUBY)
    EngineTest.suite('broken', 1)
    raise MKXPError, 'boom'
  RUBY
  File.write(File.join(dir, 'last.rb'), <<~RUBY)
    EngineTest.suite('last', 1)
    EngineTest.test('b') { EngineTest.assert(true, 'b') }
    exit(EngineTest.finish.zero? ? 0 : 1)
  RUBY
  FileUtils.cp(File.join(ROOT, 'tests', 'engine', 'run_suites.rb'), dir)
  ENV['MKXP_TEST_SUITES'] = 'first.rb broken.rb last.rb'
  System.reset
  begin
    load File.join(dir, 'run_suites.rb')
  rescue SystemExit
    nil
  end
end
lines = System::LINES.map { |line| line.sub('[TEST] ', '') }.grep(/\A(SUITE|ok|FAIL|DONE)/)
assert_eq(lines, ['SUITE first', 'ok a', 'DONE passed=1 failed=0 pending=0',
                  'SUITE broken', 'FAIL broken.rb -- MKXPError: boom', 'DONE passed=0 failed=1 pending=0',
                  'SUITE last', 'ok b', 'DONE passed=1 failed=0 pending=0'],
          'run_suites runs each suite and reports the one that raised')

unless $failures.empty?
  warn "test_engine_harness: #{$failures.size} failure(s)"
  $failures.each { |failure| warn "FAIL: #{failure}" }
  exit 1
end

test_passed('test_engine_harness', 33)
