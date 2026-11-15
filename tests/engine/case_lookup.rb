# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# Windows finds a file in any case, and iOS does not. The Essentials
# plugin manager calls File.mtime on
# "Plugins/Redistribuidor EVS/meta.txt", and the file is "Meta.txt".
# The engine's own lookup cannot see a file that the game made after
# launch, so the suite makes its files first.
#
# CI runs it with and without --config '{"pathCache":false}'.
#
#   tools/run-engine-tests.sh --suite case_lookup.rb --ruby 18
#
# This file must parse on Ruby 1.8, 1.9 and 3.1.

unless defined?(EngineTest)
  harness_path = File.join(File.dirname(__FILE__), 'harness.rb')
  eval(File.read(harness_path), TOPLEVEL_BINDING, harness_path) # rubocop:disable Security/Eval
end

EngineTest.suite('case-lookup', 9)

EngineTest.load_preloads(%w[windows_fs])

# The suite calls IO and Kernel#open on purpose, because games do.
# rubocop:disable Security/IoMethods, Security/Open
# rubocop:disable Style/FileWrite, Style/NumericLiteralPrefix -- File.binwrite
# and the 0o prefix are not on Ruby 1.8.
LOOKUP_DIR = 'case_lookup/Redistribuidor EVS'.freeze
LOOKUP_FILE = "#{LOOKUP_DIR}/Meta.txt".freeze
LOOKUP_DATA = "#{LOOKUP_DIR}/Data.rxdata".freeze
LOOKUP_VARIANT = "#{LOOKUP_DIR}/meta.txt".freeze
LOOKUP_DIR_VARIANT = 'case_lookup/redistribuidor evs'.freeze
LOOKUP_TEXT = "a\nb\n".freeze

def lookup_reset
  Dir.mkdir('case_lookup') unless File.directory?('case_lookup')
  Dir.mkdir(LOOKUP_DIR) unless File.directory?(LOOKUP_DIR)
  File.open(LOOKUP_FILE, 'wb') { |file| file.write(LOOKUP_TEXT) }
  File.open(LOOKUP_DATA, 'wb') { |file| file.write(Marshal.dump([1, 2])) }
end

# The names on disk, in the case that the disk keeps.
def lookup_spellings
  Dir._mkxp_orig_entries(LOOKUP_DIR).select { |name| name.downcase == 'meta.txt' }
end

def lookup_lines(receiver)
  lines = []
  receiver.foreach(LOOKUP_VARIANT) { |line| lines << line }
  lines
end

lookup_reset

EngineTest.test('File.mtime finds a file in another case') do
  EngineTest.assert_equal(File.mtime(LOOKUP_FILE), File.mtime(LOOKUP_VARIANT), 'mtime')
  EngineTest.assert_equal(File.atime(LOOKUP_FILE), File.atime(LOOKUP_VARIANT), 'atime')
  EngineTest.assert_equal(File.ctime(LOOKUP_FILE), File.ctime(LOOKUP_VARIANT), 'ctime')
  folder_variant = "#{LOOKUP_DIR_VARIANT}/Meta.txt"
  EngineTest.assert_equal(File.mtime(LOOKUP_FILE), File.mtime(folder_variant), 'folder case')
end

EngineTest.test('the stat calls find a file in another case') do
  size = LOOKUP_TEXT.size
  EngineTest.assert_equal(size, File.stat(LOOKUP_VARIANT).size, 'File.stat')
  EngineTest.assert_equal(size, File.lstat(LOOKUP_VARIANT).size, 'File.lstat')
  EngineTest.assert_equal('file', File.ftype(LOOKUP_VARIANT), 'File.ftype')
  EngineTest.assert_equal(size, File.size(LOOKUP_VARIANT), 'File.size')
  EngineTest.assert_equal(size, File.size?(LOOKUP_VARIANT), 'File.size?')
  EngineTest.assert_equal(false, File.zero?(LOOKUP_VARIANT), 'File.zero?')
  EngineTest.assert_equal(true, File.readable?(LOOKUP_VARIANT), 'File.readable?')
  EngineTest.assert_equal(true, File.writable?(LOOKUP_VARIANT), 'File.writable?')
  EngineTest.assert_equal(size, FileTest.size(LOOKUP_VARIANT), 'FileTest.size')
  EngineTest.assert_equal(false, File.empty?(LOOKUP_VARIANT), 'File.empty?') if File.respond_to?(:empty?)
  if File.respond_to?(:realpath)
    real = File.realpath(LOOKUP_FILE)
    EngineTest.assert_equal(real, File.realpath('meta.txt', LOOKUP_DIR_VARIANT), 'realpath in a folder')
  end
end

EngineTest.test('the read calls find a file in another case') do
  EngineTest.assert_equal(LOOKUP_TEXT, File.read(LOOKUP_VARIANT), 'File.read')
  EngineTest.assert_equal(%W[a\n b\n], File.readlines(LOOKUP_VARIANT), 'File.readlines')
  EngineTest.assert_equal(%W[a\n b\n], lookup_lines(File), 'File.foreach')
  EngineTest.assert_equal(LOOKUP_TEXT, IO.read(LOOKUP_VARIANT), 'IO.read')
  EngineTest.assert_equal(%W[a\n b\n], IO.readlines(LOOKUP_VARIANT), 'IO.readlines')
  EngineTest.assert_equal(%W[a\n b\n], lookup_lines(IO), 'IO.foreach')
  EngineTest.assert_equal(LOOKUP_TEXT, open(LOOKUP_VARIANT, 'rb', &:read), 'Kernel#open')
  if File.respond_to?(:binread)
    EngineTest.assert_equal(LOOKUP_TEXT, File.binread(LOOKUP_VARIANT), 'File.binread')
    EngineTest.assert_equal(LOOKUP_TEXT, IO.binread(LOOKUP_VARIANT), 'IO.binread')
  end
end

EngineTest.test('load_data finds a file in another case') do
  EngineTest.assert_equal([1, 2], load_data("#{LOOKUP_DIR_VARIANT}/data.rxdata"), 'loaded value')
end

EngineTest.test('chmod, utime and truncate change the file in another case') do
  begin
    stamp = Time.at(1_000_000_000)
    EngineTest.assert_equal(1, File.chmod(0600, LOOKUP_VARIANT), 'File.chmod count')
    EngineTest.assert_equal(0600, File.stat(LOOKUP_FILE).mode & 0777, 'mode')
    EngineTest.assert_equal(1, File.utime(stamp, stamp, LOOKUP_VARIANT), 'File.utime count')
    EngineTest.assert_equal(stamp, File.mtime(LOOKUP_FILE), 'mtime after utime')
    File.truncate(LOOKUP_VARIANT, 2)
    EngineTest.assert_equal("a\n", File.read(LOOKUP_FILE), 'text after truncate')
  ensure
    File.chmod(0644, LOOKUP_FILE)
    lookup_reset
  end
end

EngineTest.test('a write in another case replaces the file') do
  begin
    open(LOOKUP_VARIANT, 'wb') { |file| file.write('open') }
    EngineTest.assert_equal('open', File.read(LOOKUP_FILE), 'Kernel#open')
    if IO.respond_to?(:write)
      IO.write(LOOKUP_VARIANT, 'io')
      EngineTest.assert_equal('io', File.read(LOOKUP_FILE), 'IO.write')
    end
    save_data([3], "#{LOOKUP_DIR_VARIANT}/data.rxdata")
    EngineTest.assert_equal([3], load_data(LOOKUP_DATA), 'save_data')
    EngineTest.assert_equal(['Meta.txt'], lookup_spellings, 'names on disk')
  ensure
    lookup_reset
  end
end

EngineTest.test('the folder calls find a folder in another case') do
  names = Dir._mkxp_orig_entries(LOOKUP_DIR).sort
  EngineTest.assert_equal(names, Dir.entries(LOOKUP_DIR_VARIANT).sort, 'Dir.entries')
  listed = []
  Dir.foreach(LOOKUP_DIR_VARIANT) { |name| listed << name }
  EngineTest.assert_equal(names, listed.sort, 'Dir.foreach')
  dir = Dir.new(LOOKUP_DIR_VARIANT)
  begin
    EngineTest.assert_equal(names, dir.entries.sort, 'Dir.new')
  ensure
    dir.close
  end
  EngineTest.assert_equal(names, Dir.open(LOOKUP_DIR_VARIANT) { |opened| opened.entries.sort }, 'Dir.open')
  pwd = Dir.chdir(LOOKUP_DIR_VARIANT) { Dir.pwd }
  EngineTest.assert_equal('Redistribuidor EVS', File.basename(pwd), 'Dir.chdir')
  children = names - %w[. ..]
  if Dir.respond_to?(:children)
    EngineTest.assert_equal(children, Dir.children(LOOKUP_DIR_VARIANT).sort, 'Dir.children')
  end
  EngineTest.assert_equal(false, Dir.empty?(LOOKUP_DIR_VARIANT), 'Dir.empty?') if Dir.respond_to?(:empty?)
end

# Graphics/Lookup/late.txt is in the game folder at launch.
EngineTest.test('a file from launch is found in another case') do
  real = 'Graphics/Lookup/late.txt'
  variant = 'graphics/LOOKUP/Late.txt'
  EngineTest.assert_equal(File.mtime(real), File.mtime(variant), 'mtime')
  EngineTest.assert_equal(File.read(real), File.read(variant), 'text')
end

EngineTest.test('a missing file still raises ENOENT') do
  EngineTest.assert_raises('No such file', 'File.mtime') { File.mtime("#{LOOKUP_DIR_VARIANT}/missing.txt") }
  EngineTest.assert_raises('No such file', 'File.read') { File.read("#{LOOKUP_DIR_VARIANT}/missing.txt") }
  EngineTest.assert_equal(nil, File.size?("#{LOOKUP_DIR_VARIANT}/missing.txt"), 'File.size?')
end
# rubocop:enable Style/FileWrite, Style/NumericLiteralPrefix
# rubocop:enable Security/IoMethods, Security/Open

EngineTest.finish
exit
