#!/usr/bin/env ruby
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# Tests for script_folder_split.rb. The device suite in
# tests/script-folder runs the split inside the engine.
# Run: ruby mkxp-z-apple-mobile/tools/test_script_folder.rb

require 'fileutils'
require 'tmpdir'
require_relative 'assertion_count'
require_relative '../scripts/preload/script_folder_split'

def assert_eq(actual, expected, label)
  asserted
  return if actual == expected

  warn "FAIL: #{label}\n  expected: #{expected.inspect}\n  actual:   #{actual.inspect}"
  exit 1
end

LOADER = "def load_scripts_from_folder(path)\n  # eval each file\nend\n".freeze

def write(path, code)
  FileUtils.mkdir_p(File.dirname(path))
  File.binwrite(path, code)
end

def section(name, code)
  [0, name, '', code]
end

def assert_order_and_shape
  write('Data/Scripts/002_b.rb', 'b')
  write('Data/Scripts/001_a.rb', 'a')
  write('Data/Scripts/001_folder/001_late.rb', 'late')
  write('Data/Scripts/999_main/999_main.rb', 'main')

  main = section('Main', "#{LOADER}load_scripts_from_folder('Data/Scripts')\n")
  scripts = [section('Before', 'x'), main, section('After', 'y')]
  out = MKXPScriptFolder.expand(scripts, 1)

  assert_eq(out.map { |s| s[1] }, %w[Before Main 001_a.rb 002_b.rb 001_late.rb 999_main.rb After],
            'files by name, then subfolders by name, between the neighbours')
  assert_eq(out.map { |s| s[3] }.values_at(2, 3, 4, 5), %w[a b late main], 'each section holds its file')
  assert_eq(out[1][3], "#{LOADER}\n", 'the Main section keeps the definitions and loses the call')
  assert_eq(main[3], "#{LOADER}load_scripts_from_folder('Data/Scripts')\n",
            'the input section is not changed')
  assert_eq(scripts.length, 3, 'the input array is not changed')
end

def assert_only_ruby_files
  FileUtils.rm_rf('Data/Scripts')
  write('Data/Scripts/.hidden.rb', 'hidden file')
  write('Data/Scripts/001_a.rb', 'a')
  write('Data/Scripts/002_upper.RB', 'upper')
  write('Data/Scripts/001_plugin/meta.txt', "Name       = Tip Cards\r\nVersion    = 1.2\r\n")
  write('Data/Scripts/001_plugin/001_scene.rb', 'scene')
  write('Data/Scripts/.git/001_hidden.rb', 'hidden')

  main = section('Main', "#{LOADER}load_scripts_from_folder('Data/Scripts')\n")
  out = MKXPScriptFolder.expand([main], 0)
  assert_eq(out.map { |s| s[1] }, %w[Main .hidden.rb 001_a.rb 002_upper.RB 001_scene.rb],
            'only .rb files of any case, and no folder whose name starts with a dot')
end

def assert_extractor_shape
  code = "#{LOADER}load_scripts_from_folder(\"Data/Scripts\")".gsub("\n", "\r\n")
  out = MKXPScriptFolder.expand([section('Main', code)], 0)
  assert_eq(out && out.length, 5, 'CRLF, double quotes and no final newline still split')
  assert_eq(out[0][3], LOADER.gsub("\n", "\r\n"), 'the CRLF call line is removed')
end

def assert_no_split
  call = "load_scripts_from_folder('Data/Scripts')"
  assert_eq(MKXPScriptFolder.expand([section('Main', call)], 0), nil, 'a call without the definition')
  assert_eq(MKXPScriptFolder.expand([section('Main', LOADER)], 0), nil, 'the definition without the call')
  assert_eq(MKXPScriptFolder.expand([section('Main', "#{LOADER}#{call} # go")], 0), nil,
            'a call with more on its line')
  assert_eq(MKXPScriptFolder.expand([section('Main', "#{LOADER}load_scripts_from_folder 'Data/Scripts'")], 0),
            nil, 'a call without brackets')
  assert_eq(MKXPScriptFolder.expand([section('Main', "#{LOADER}load_scripts_from_folder('Data/Other')")], 0),
            nil, 'a call on another folder')
  assert_eq(MKXPScriptFolder.expand(['not a section'], 0), nil, 'a section that is not an array')
  assert_eq(MKXPScriptFolder.expand([[0, 'Main', '', nil]], 0), nil, 'a section without code')
end

Dir.mktmpdir do |dir|
  Dir.chdir(dir) do
    assert_order_and_shape
    assert_extractor_shape
    assert_no_split
    assert_only_ruby_files
  end
end

test_passed('script_folder', 15)
