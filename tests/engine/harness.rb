# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# Assertion and reporting harness for the engine test suites in this
# directory. Each suite loads it on its first lines, because the
# engine skips "preloadScript" for games that boot from a
# "customScript".
#
# Every line starts with "[TEST] ", which is how a runner tells suite
# output apart from the rest of the engine log:
#
#   [TEST] SUITE <name>
#   [TEST] PLAN <n>
#   [TEST] INFO <key>=<value>
#   [TEST] ok <name>
#   [TEST] FAIL <name> -- <reason>
#   [TEST] PEND <name> -- <reason>
#   [TEST] DONE passed=<n> failed=<n> pending=<n>
#
# PLAN states how many checks the suite is about to run. A runner
# compares it against the DONE totals, so a suite that dies halfway
# through cannot report a clean run.
#
# PEND means the engine refused the operation with a known "not
# supported" error. A check that the engine cannot do yet reports
# PEND, not FAIL, so the same suite runs before and after a port.
# When the engine gains the operation, the line turns into ok with
# no edit to the suite.
#
# PEND is off by default. A suite turns it on only where the gap is
# expected, with EngineTest.pending_allowed = true. Everywhere else
# an unimplemented operation is a defect, so it reports FAIL.
#
# This file must parse on Ruby 1.8, 1.9, and 3.x. The engine picks
# the interpreter per game.
module EngineTest
  PREFIX = '[TEST] '.freeze

  # Errors the engine raises for an operation it does not implement
  # for that kind of bitmap, from the guard macros at the top of
  # src/display/bitmap.cpp. A test that hits one reports PEND.
  #
  # The third guard there, "not supported for static bitmaps", is
  # missing on purpose. That one fires when a test calls an
  # animation-only method on an ordinary bitmap, which is a defect in
  # the test, not a gap in the engine.
  UNSUPPORTED = [
    'Operation not supported for mega surfaces',
    'Operation not supported for animated bitmaps'
  ].freeze

  class Failure < StandardError
  end

  # The counters start here as well as in reset, so a suite that runs
  # a check before it calls EngineTest.suite still reports the result
  # instead of dying on a nil counter.
  @passed = 0
  @failed = 0
  @pending = 0
  @planned = 0
  @asserted = 0
  @pending_allowed = false

  def self.reset
    @passed = 0
    @failed = 0
    @pending = 0
    @planned = 0
    @asserted = 0
    @pending_allowed = false
  end

  # Lets the checks that follow report PEND for an operation the
  # engine does not implement. Leave it off wherever the engine is
  # expected to do the work, so a gap there reads as the defect it is.
  def self.pending_allowed=(allowed)
    @pending_allowed = allowed
  end

  # Counts one assertion against the running check. Every assertion
  # below calls it. A check that never does asserts nothing, and the
  # runner fails it rather than call it a pass.
  def self.asserted
    @asserted += 1
  end

  def self.emit(line)
    System.puts(PREFIX + line)
  end

  def self.info(key, value)
    emit("INFO #{key}=#{value}")
  end

  # `planned` is how many checks the suite will run. finish compares
  # it against what actually ran.
  def self.suite(name, planned)
    reset
    @planned = planned
    emit("SUITE #{name}")
    emit("PLAN #{planned}")
    info('ruby', RUBY_VERSION)
    info('interpreter', System.ruby_version)
    info('game', ENV.fetch('MKXP_TEST_GAME', nil))
    info('max_size', Bitmap.max_size)
    info('real_max_size', Bitmap.real_max_size)
  end

  # Runs one check. Every outcome prints exactly one line.
  def self.test(name)
    @asserted = 0
    yield
    if @asserted.zero?
      record_failure(name, 'the check asserted nothing')
    else
      @passed += 1
      emit("ok #{name}")
    end
  rescue Failure => e
    record_failure(name, e.message)
  # The engine's error classes descend from Exception, not from
  # StandardError, so a narrower rescue would let MKXPError through
  # and abort the whole suite.
  rescue Exception => e # rubocop:disable Lint/RescueException
    if unsupported?(e) && @pending_allowed
      @pending += 1
      emit("PEND #{name} -- #{e.message}")
    else
      record_failure(name, "#{e.class}: #{e.message}")
    end
  end

  def self.record_failure(name, reason)
    @failed += 1
    emit("FAIL #{name} -- #{reason}")
  end

  def self.unsupported?(error)
    message = error.message.to_s
    UNSUPPORTED.any? { |known| message.include?(known) }
  end

  def self.assert(condition, message)
    asserted
    raise Failure, message unless condition
  end

  # Yields an engine object and disposes it afterwards, whatever the
  # check did. A failed check must not leak a bitmap, or a later
  # check in the same run fails for want of memory instead of for
  # its own reason.
  def self.owning(resource)
    yield resource
  ensure
    resource.dispose unless resource.disposed?
  end

  # A game that boots from "customScript" gets no engine preloads, so
  # a suite loads the ones it needs from the host's asset bundle, in
  # the order of loadEnginePreloads(). The host runs the suite from a
  # copy in Documents, and the only app bundle path Ruby can see is
  # the stdlib folder on $LOAD_PATH.
  def self.load_preloads(names)
    app = $LOAD_PATH.map { |dir| dir[/\A.*\.app/] }.compact.first
    names.each do |name|
      path = File.join(app, 'Assets.bundle', 'Preload', "#{name}.rb")
      eval(File.read(path), TOPLEVEL_BINDING, name) # rubocop:disable Security/Eval
    end
  end

  # The common case: a bitmap of a given size, filled or left clear.
  def self.bitmap(width, height, colour = nil)
    owning(Bitmap.new(width, height)) do |bitmap|
      bitmap.fill_rect(0, 0, width, height, colour) if colour
      yield bitmap
    end
  end

  def self.assert_equal(expected, actual, label)
    asserted
    return if expected == actual

    raise Failure, "#{label}: expected #{expected.inspect}, got #{actual.inspect}"
  end

  def self.refute_equal(unexpected, actual, label)
    asserted
    return unless unexpected == actual

    raise Failure, "#{label}: expected a value other than #{unexpected.inspect}"
  end

  # Colour channels come back as floats. Round them so a comparison
  # against literal 0..255 values means what it reads like.
  def self.pixel(bitmap, point)
    colour = bitmap.get_pixel(point[0], point[1])
    [colour.red.round, colour.green.round, colour.blue.round, colour.alpha.round]
  end

  def self.assert_pixel(bitmap, point, expected, label)
    assert_equal(expected, pixel(bitmap, point), "#{label} at #{point.inspect}")
  end

  # Channel-wise comparison with a tolerance, for operations whose
  # exact output the API does not fix (gradients, blur, hue).
  def self.assert_pixel_near(bitmap, point, expected, tolerance, label)
    asserted
    actual = pixel(bitmap, point)
    off = (0..3).select { |i| (actual[i] - expected[i]).abs > tolerance }
    return if off.empty?

    raise Failure, "#{label} at #{point.inspect}: expected #{expected.inspect} " \
                   "+/- #{tolerance}, got #{actual.inspect}"
  end

  def self.assert_raises(fragment, label)
    asserted
    yield
    raise Failure, "#{label}: expected an error matching #{fragment.inspect}, none raised"
  rescue Failure
    raise
  rescue Exception => e # rubocop:disable Lint/RescueException -- see self.test
    return if e.message.to_s.include?(fragment)

    raise Failure, "#{label}: expected an error matching #{fragment.inspect}, " \
                   "got #{e.class}: #{e.message}"
  end

  # Prints the summary line and returns the failure count, so a suite
  # can decide what to do with it.
  #
  # A run that does not match its own PLAN counts as a failure. That
  # covers a suite whose checks stopped registering and a suite that
  # lost checks along the way.
  def self.finish
    ran = @passed + @failed + @pending
    record_failure('<plan>', "planned #{@planned} checks, ran #{ran}") if ran != @planned

    emit("DONE passed=#{@passed} failed=#{@failed} pending=#{@pending}")
    @failed
  end
end
