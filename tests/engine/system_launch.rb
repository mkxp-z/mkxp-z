# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# System.launch with a web link and with a program name.
#
#   tools/run-engine-tests.sh --suite system_launch.rb
#
# This file must parse on Ruby 1.8, 1.9, and 3.x.

unless defined?(EngineTest)
  harness_path = File.join(File.dirname(__FILE__), 'harness.rb')
  eval(File.read(harness_path), TOPLEVEL_BINDING, harness_path) # rubocop:disable Security/Eval
end

EngineTest.suite('system-launch', 3)

EngineTest.test('System.launch with a program name raises MKXPError') do
  EngineTest.assert_raises('opens only http and https links', 'System.launch("notepad.exe")') do
    System.launch('notepad.exe')
  end
end

EngineTest.test('System.launch with a link that is not UTF-8 returns nil') do
  EngineTest.assert_equal(nil, System.launch("https://example.com/\xff"), 'System.launch(byte 0xff)')
end

EngineTest.test('System.launch opens an https link and the game goes on') do
  EngineTest.assert_equal(nil, System.launch('https://example.com/?team=AQAI'), 'System.launch(https link)')
end

EngineTest.finish
exit
