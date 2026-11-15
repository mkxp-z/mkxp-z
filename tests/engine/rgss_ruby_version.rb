# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# Ruby 1.8 and Ruby 1.9 report the Ruby version of RPG Maker: 1.8.1 for
# XP and VX, and 1.9.2 for VX Ace.
#
#   tools/run-engine-tests.sh --suite rgss_ruby_version.rb --ruby 18
#   tools/run-engine-tests.sh --suite rgss_ruby_version.rb --ruby 19
#
# This file must parse on Ruby 1.8.

unless defined?(EngineTest)
  harness_path = File.join(File.dirname(__FILE__), 'harness.rb')
  eval(File.read(harness_path), TOPLEVEL_BINDING, harness_path) # rubocop:disable Security/Eval
end

EngineTest.suite('rgss-ruby-version', 2)

EngineTest.test('RUBY_VERSION is the Ruby of RPG Maker') do
  expected = { '1.8' => '1.8.1', '1.9' => '1.9.2' }[System.ruby_version]
  EngineTest.assert_equal(expected, RUBY_VERSION, 'RUBY_VERSION')
end

# Ruby 1.9 removed VERSION.
EngineTest.test('VERSION is RUBY_VERSION on Ruby 1.8 only') do
  expected = System.ruby_version == '1.8' ? RUBY_VERSION : nil
  EngineTest.assert_equal(expected, defined?(VERSION) ? VERSION : nil, 'VERSION')
end

EngineTest.finish
exit
