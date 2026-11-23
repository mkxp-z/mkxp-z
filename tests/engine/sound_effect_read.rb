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

# Ruby cannot count the reads of the process. The kernel time of the
# process is the nearest value. With one read for each byte of the
# 70 KB file, the play takes 46 ms of it on an M2 Pro. With reads in
# blocks, it takes 6 ms on the CI runner.
KERNEL_LIMIT_MS = 20

EngineTest.test('the first play of a sound effect reads its file in blocks') do
  before = Process.times.stime
  Audio.se_play('Audio/SE/read_noise', 0)
  kernel_ms = (Process.times.stime - before) * 1000
  Audio.se_stop
  EngineTest.info('kernel_ms', format('%.1f', kernel_ms))
  EngineTest.assert(kernel_ms < KERNEL_LIMIT_MS, format('%.1f ms of kernel time', kernel_ms))
end

EngineTest.finish
exit
