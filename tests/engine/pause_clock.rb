# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# The clocks after the engine was away: a pause, a game dialog that
# stays open, and a pause that the launcher cancels before the engine
# stops. tests/host/host.m reads each dialog text as a command. The
# away time must not count as run time. A game camera that divides by
# Graphics.average_frame_rate otherwise moves too far and stops the
# game with a RangeError.
#
#   tools/run-engine-tests.sh --suite pause_clock.rb

unless defined?(EngineTest)
  harness_path = File.join(File.dirname(__FILE__), 'harness.rb')
  eval(File.read(harness_path), TOPLEVEL_BINDING, harness_path) # rubocop:disable Security/Eval
end

EngineTest.suite('pause-clock', 8)

SPAN = 10.0

# Graphics.average_frame_rate averages the last 41 frame times, so 60
# frames fill it before the engine goes away. Only 5 frames follow, so
# the away time is still in the average if the engine counts it.
def measure_away
  times = []
  60.times do
    Graphics.update
    times << Time.now
  end
  drawn = (times.size - 1) / (times.last - times.first)

  wall_start = Time.now
  clock_start = System.uptime
  yield
  longest = 0
  5.times do
    Graphics.update
    longest = [longest, Graphics.delta / 1_000_000.0].max
  end
  {
    :drawn => drawn,
    :average => Graphics.average_frame_rate,
    :away => (Time.now - wall_start) - (System.uptime - clock_start),
    :longest => longest
  }
end

def check_clock(name, result, longest_frame)
  EngineTest.test("#{name}: System.uptime leaves out the away time") do
    message = format('the clock left out %<away>.1f s of %<span>.1f s',
                     :away => result[:away], :span => SPAN)
    EngineTest.assert(result[:away] > SPAN * 0.8, message)
  end

  EngineTest.test("#{name}: Graphics.delta leaves out the away time") do
    message = format('the longest frame took %<longest>.1f s', :longest => result[:longest])
    EngineTest.assert(result[:longest] < longest_frame, message)
  end
end

def check_frame_rate(name, result)
  EngineTest.test("#{name}: Graphics.average_frame_rate leaves out the away time") do
    message = format('average %<average>.1f, drawn %<drawn>.1f',
                     :average => result[:average], :drawn => result[:drawn])
    EngineTest.assert(result[:average] > result[:drawn] * 0.5, message)
  end
end

paused = measure_away { msgbox("pause #{SPAN}") }
check_clock('a pause', paused, 1.0)
check_frame_rate('a pause', paused)

held = measure_away { msgbox("hold #{SPAN}") }
check_clock('a game dialog', held, 1.0)
check_frame_rate('a game dialog', held)

# The suite sleeps for 0.6 s more than the pause, and that time counts.
cancelled = measure_away do
  msgbox("pause-cancel 0.2 #{SPAN}")
  sleep(SPAN + 0.6)
end
check_clock('a cancelled pause', cancelled, 2.0)

EngineTest.finish
exit
