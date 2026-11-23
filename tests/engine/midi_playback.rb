# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# A MIDI track plays through FluidSynth. When the engine cannot read
# MIDI, it logs "Sound format unsupported" and the track position
# stays at zero.
#
#   tools/run-engine-tests.sh --suite midi_playback.rb

unless defined?(EngineTest)
  harness_path = File.join(File.dirname(__FILE__), 'harness.rb')
  eval(File.read(harness_path), TOPLEVEL_BINDING, harness_path) # rubocop:disable Security/Eval
end

EngineTest.suite('midi-playback', 1)

# One track at 120 beats a minute: a piano note for four beats.
def midi_track(events)
  events += [0x00, 0xFF, 0x2F, 0x00]
  "MTrk#{[events.size].pack('N')}#{events.pack('C*')}"
end

events = [0x00, 0xC0, 0x00, 0x00, 0x90, 0x3C, 0x64, 0x8F, 0x00, 0x80, 0x3C, 0x40]
midi = "MThd#{[6, 0, 1, 480].pack('Nnnn')}#{midi_track(events)}"
File.binwrite('midi_tone.mid', midi)

# The sound device of a CI runner can stall and then catch up: the
# position after 1.5 seconds was between 0 and 5.2 seconds. The engine
# sends no event when the track starts, so the test reads the position
# at each frame.
LIMIT = 30

EngineTest.test('a MIDI track plays') do
  Audio.bgm_play('midi_tone', 100, 100)
  start = Time.now
  position = 0.0
  while position <= 0.5 && Time.now - start < LIMIT
    Graphics.update
    position = Audio.bgm_pos
  end
  Audio.bgm_stop
  EngineTest.info('bgm_pos', position)
  EngineTest.info('waited', format('%.1f', Time.now - start))
  EngineTest.assert(position > 0.5, "bgm_pos #{position} after #{LIMIT} seconds")
end

EngineTest.finish
exit
