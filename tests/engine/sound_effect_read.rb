# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# The first play of a sound effect loads its file on the game thread,
# so the frame waits for it. The Ogg Vorbis decoder asks for one byte
# at a time. The engine must not pass each of these to the kernel as
# one read.
#
#   tools/run-engine-tests.sh --suite sound_effect_read.rb

unless defined?(EngineTest)
  harness_path = File.join(File.dirname(__FILE__), 'harness.rb')
  eval(File.read(harness_path), TOPLEVEL_BINDING, harness_path) # rubocop:disable Security/Eval
end

EngineTest.suite('sound-effect-read', 1)

def kernel_ms
  before = Process.times.stime
  yield
  (Process.times.stime - before) * 1000
end

# Ruby cannot count the reads of the process. The kernel time of the
# process is the nearest value, but it is not the same on each
# computer: the same play took 4 ms and 25 ms on two CI runners. So
# the test compares it with the kernel time of one read for each byte
# of the same file. On an M2 Pro, the play takes 0.02 times that with
# reads in blocks, and 1.5 times that with one read for each byte.
EngineTest.test('the first play of a sound effect reads its file in blocks') do
  each_byte_ms = kernel_ms do
    File.open('Audio/SE/read_noise.ogg', 'rb') { |file| file.stat.size.times { file.sysread(1) } }
  end
  play_ms = kernel_ms { Audio.se_play('Audio/SE/read_noise', 0) }
  Audio.se_stop
  play = format('%.1f', play_ms)
  each_byte = format('%.1f', each_byte_ms)
  EngineTest.info('each_byte_ms', each_byte)
  EngineTest.info('play_ms', play)
  EngineTest.assert(play_ms < each_byte_ms / 2,
                    "#{play} ms of kernel time, and #{each_byte} ms for one read for each byte")
end

EngineTest.finish
exit
