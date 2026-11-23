# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# The frame rate of a frozen screen. A game calls Graphics.update on a
# frozen screen to wait, for example between the event commands
# "Prepare for Transition" and "Execute Transition". Each call must
# take one frame and count one frame, as on a screen that is not
# frozen. This is also true under fast-forward and after a slow step
# of the game.
#
#   tools/run-engine-tests.sh --suite frozen_screen_rate.rb
#   tools/run-engine-tests.sh --suite frozen_screen_rate.rb --fast-forward 4

unless defined?(EngineTest)
  harness_path = File.join(File.dirname(__FILE__), 'harness.rb')
  eval(File.read(harness_path), TOPLEVEL_BINDING, harness_path) # rubocop:disable Security/Eval
end

EngineTest.suite('frozen-screen-rate', 3)

MULTIPLIER = [(ENV['MKXP_TEST_FAST_FORWARD'] || '1').to_i, 1].max
EngineTest.info('multiplier', MULTIPLIER)

UPDATES = 80 * MULTIPLIER

def timed_updates
  count = Graphics.frame_count
  start = Time.now
  UPDATES.times { Graphics.update }
  [Time.now - start, Graphics.frame_count - count]
end

normal_seconds, = timed_updates
Graphics.freeze
frozen_seconds, frozen_count = timed_updates
sleep(1)
late_seconds, = timed_updates
Graphics.transition(0)

EngineTest.test('Graphics.update on a frozen screen takes one frame') do
  message = format('%<frozen>.2f s frozen, %<normal>.2f s not frozen',
                   :frozen => frozen_seconds, :normal => normal_seconds)
  EngineTest.assert(frozen_seconds < normal_seconds * 1.5, message)
end

EngineTest.test('Graphics.update on a frozen screen counts one frame') do
  EngineTest.assert_equal(UPDATES, frozen_count, 'frame count')
end

EngineTest.test('a frozen screen does not run faster to catch up on a slow step') do
  message = format('%<late>.2f s after the slow step, %<normal>.2f s not frozen',
                   :late => late_seconds, :normal => normal_seconds)
  EngineTest.assert(late_seconds > normal_seconds * 0.75, message)
end

EngineTest.finish
exit
