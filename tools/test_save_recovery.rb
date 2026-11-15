#!/usr/bin/env ruby
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# Unit tests for the alias-era save recovery guards in
# windows_fs.rb: the cwd gate, the symlink-proof identity
# guard, artifact exclusion, and unique collision backups.
#
# The defect these tests pin: on iOS the host config spells the
# data dir /var/... while getcwd resolves the symlink to
# /private/var/..., so a string-comparison guard let the recovery
# run while the game had chdir'd INTO the data dir. It then
# renamed every save onto a ".pre-literal.bak" chain. The
# workspace here reproduces that shape with a real symlink, so the
# tests run on any host, case-folding or not.
# Run: ruby mkxp-z-apple-mobile/tools/test_save_recovery.rb

require_relative 'assertion_count'

require 'fileutils'

ROOT = File.expand_path('..', __dir__)
WORK = File.expand_path('test_save_recovery_workspace', __dir__)
REAL_HOME = File.join(WORK, 'private-home')
USERDATA_REAL = File.join(REAL_HOME, 'UserData')
# The alias mirrors iOS's /var -> /private/var indirection: the
# spelling handed to the engine goes through the symlink, getcwd
# resolves to the real path.
ALIAS_HOME = File.join(WORK, 'home')
USERDATA_ALIAS = File.join(ALIAS_HOME, 'UserData')
GAME = File.join(REAL_HOME, 'Game')

def reset_workspace!
  FileUtils.rm_rf(WORK)
  FileUtils.mkdir_p(USERDATA_REAL)
  FileUtils.mkdir_p(GAME)
  File.symlink(REAL_HOME, ALIAS_HOME)
end

def ensure_ruby31_compat_aliases!
  class << File
    alias_method :exists?, :exist? unless method_defined?(:exists?)
  end
  FileTest.singleton_class.class_eval do
    alias_method :exists?, :exist? unless method_defined?(:exists?)
  end
  class << Dir
    alias_method :exists?, :exist? unless method_defined?(:exists?)
  end
end

# Loads (or reloads) windows_fs.rb with the data dir set to
# `data_dir`. Loading must happen with the cwd at GAME: the file
# captures the game root from the cwd, exactly like the engine
# boot does.
def load_windows_fs!(data_dir)
  ensure_ruby31_compat_aliases!
  Object.send(:remove_const, :System) if defined?(System)
  Object.const_set(:System, Module.new do
    module_function

    define_method(:data_directory) { data_dir }
    define_method(:puts) { |*args| Kernel.puts(*args) }
    define_method(:joiplay_compat?) { false }
  end)
  unless Kernel.private_method_defined?(:load_data)
    Kernel.module_eval do
      def load_data(_path, *_args); end
      def save_data(_obj, _path, *_args); end
      module_function :load_data, :save_data
    end
  end
  prev_verbose = $VERBOSE
  $VERBOSE = nil
  begin
    Dir.chdir(GAME) do
      load File.join(ROOT, 'scripts', 'preload', 'windows_fs.rb')
    end
  ensure
    $VERBOSE = prev_verbose
  end
end

def assert_eq(actual, expected, label)
  asserted
  return if actual == expected

  warn "FAIL: #{label}\n  expected: #{expected.inspect}\n  actual:   #{actual.inspect}"
  exit 1
end

def assert_true(value, label)
  assert_eq(value, true, label)
end

def assert_false(value, label)
  assert_eq(value, false, label)
end

def raw_entries(dir)
  Dir._mkxp_orig_entries(dir).reject { |name| ['.', '..'].include?(name) }.sort
end

def set_mtime(path, time)
  File.utime(time, time, path)
end

# --- The device shape: data dir spelled through a symlink ---
# Essentials v19+ enumerates saves with Dir.chdir(data_dir) +
# Dir.glob("*"). With the symlink spelling, the old guard saw two
# different strings for one directory and self-migrated every
# save. The fixed guards must leave the files alone.
reset_workspace!
File.write(File.join(USERDATA_REAL, 'Game.rxdata'), 'latest save')
File.write(File.join(USERDATA_REAL, 'Game.rxdata.bak'), 'backup save')
load_windows_fs!("#{USERDATA_ALIAS}/")

globbed = Dir.chdir("#{USERDATA_ALIAS}/") { Dir.glob('*').sort }
assert_eq(globbed, ['Game.rxdata', 'Game.rxdata.bak'], 'glob inside symlink-spelled data dir lists saves')
assert_eq(
  raw_entries(USERDATA_REAL),
  ['Game.rxdata', 'Game.rxdata.bak'],
  'saves keep their names after enumeration'
)
assert_eq(File.read(File.join(USERDATA_REAL, 'Game.rxdata')), 'latest save', 'save content untouched')

# Repeated enumeration must stay stable too - the field defect
# chained one ".pre-literal.bak" per glob call.
3.times { Dir.chdir("#{USERDATA_ALIAS}/") { Dir.glob('*') } }
assert_eq(
  raw_entries(USERDATA_REAL),
  ['Game.rxdata', 'Game.rxdata.bak'],
  'saves survive repeated enumeration'
)

# --- The full save loop stays stable through the alias ---
# Save (absolute path through the symlink), enumerate, save again,
# enumerate again: names and bytes must hold through every step.
reset_workspace!
load_windows_fs!("#{USERDATA_ALIAS}/")

File.binwrite("#{USERDATA_ALIAS}/Game.rxdata", 'first save')
Dir.chdir("#{USERDATA_ALIAS}/") { Dir.glob('*') }
File.binwrite("#{USERDATA_ALIAS}/Game.rxdata", 'second save')
listed = Dir.chdir("#{USERDATA_ALIAS}/") { Dir.glob('*') }
assert_eq(listed, ['Game.rxdata'], 'loop: enumeration sees the save')
assert_eq(
  File.read(File.join(USERDATA_REAL, 'Game.rxdata')),
  'second save',
  'loop: the second save survives enumeration'
)

# --- The cwd gate holds without any symlink ---
# A game that chdir'd into a random subfolder and runs a bare
# save-shaped glob must not have root saves moved into that
# folder.
reset_workspace!
File.write(File.join(USERDATA_REAL, 'Game.rxdata'), 'root save')
elsewhere = File.join(GAME, 'Mods')
FileUtils.mkdir_p(elsewhere)
load_windows_fs!("#{USERDATA_REAL}/")

Dir.chdir(elsewhere) { Dir.glob('Game.rxdata') }
assert_eq(raw_entries(elsewhere), [], 'no save teleported into a non-root cwd')
assert_eq(raw_entries(USERDATA_REAL), ['Game.rxdata'], 'root save stays put')

# --- Legitimate recovery still works from the game root ---
reset_workspace!
File.write(File.join(USERDATA_REAL, 'Save12.rxdata'), 'stranded slot')
File.write(File.join(USERDATA_REAL, 'Keep.rxdata'), 'not matched')
load_windows_fs!("#{USERDATA_REAL}/")

# A non-matching pattern must move nothing.
Dir.chdir(GAME) { Dir.glob('Other*.rxdata') }
assert_eq(
  raw_entries(USERDATA_REAL),
  ['Keep.rxdata', 'Save12.rxdata'],
  'non-matching pattern leaves the data dir alone'
)

Dir.chdir(GAME) { Dir.glob('Save*.rxdata') }
assert_true(
  File._mkxp_orig_file(File.join(GAME, 'Save12.rxdata')),
  'stranded root save recovered into the game folder'
)
assert_eq(
  File.read(File.join(GAME, 'Save12.rxdata')),
  'stranded slot',
  'recovered save keeps its bytes'
)
assert_eq(
  raw_entries(USERDATA_REAL),
  ['Keep.rxdata'],
  'only the matched save left the data dir'
)

# Dir[] shares the glob recovery. Ancient slot pickers use both.
reset_workspace!
File.write(File.join(USERDATA_REAL, 'Save03.rxdata'), 'bracket slot')
load_windows_fs!("#{USERDATA_REAL}/")

Dir.chdir(GAME) { Dir['Save*.rxdata'] }
assert_true(
  File._mkxp_orig_file(File.join(GAME, 'Save03.rxdata')),
  'Dir[] recovers a stranded save too'
)

# --- The single-file open recovery obeys the cwd gate ---
reset_workspace!
File.write(File.join(USERDATA_REAL, 'Game.rxdata'), 'stranded root save')
elsewhere2 = File.join(GAME, 'SubShop')
FileUtils.mkdir_p(elsewhere2)
load_windows_fs!("#{USERDATA_REAL}/")

Dir.chdir(elsewhere2) do
  begin
    File.binread('Game.rxdata')
  rescue StandardError
    nil
  end
end
assert_eq(
  raw_entries(USERDATA_REAL),
  ['Game.rxdata'],
  'bare open away from the game root recovers nothing'
)

Dir.chdir(GAME) do
  content = File.binread('Game.rxdata')
  assert_eq(content, 'stranded root save', 'bare open at the game root recovers and reads')
end
assert_false(
  raw_entries(USERDATA_REAL).include?('Game.rxdata'),
  'single-file recovery moved the save out of the data dir'
)

# --- Recovery artifacts are never candidates again ---
reset_workspace!
File.write(File.join(USERDATA_REAL, 'Game.rxdata.pre-literal.bak'), 'chained victim')
File.write(File.join(USERDATA_REAL, 'Game.rxdata.pre-literal.bak.pre-literal.bak'), 'chained twice')
load_windows_fs!("#{USERDATA_REAL}/")

assert_false(MKXPSaveFS.save_filename?('Game.rxdata.pre-literal.bak'), 'chained name is not save-shaped')
assert_false(MKXPSaveFS.save_filename?('Game.rxdata.pre-literal-2.bak'), 'numbered backup is not save-shaped')
Dir.chdir(GAME) { Dir.glob('*') }
assert_eq(
  raw_entries(USERDATA_REAL),
  ['Game.rxdata.pre-literal.bak', 'Game.rxdata.pre-literal.bak.pre-literal.bak'],
  'chained files gain no further suffix'
)

# --- Self-identity bail in the migration itself ---
reset_workspace!
File.write(File.join(USERDATA_REAL, 'Game.rxdata'), 'the one save')
load_windows_fs!("#{USERDATA_ALIAS}/")

MKXPSaveFS.migrate_save_file(
  File.join(USERDATA_ALIAS, 'Game.rxdata'),
  File.join(USERDATA_REAL, 'Game.rxdata')
)
assert_eq(raw_entries(USERDATA_REAL), ['Game.rxdata'], 'self-migration is a no-op')
assert_eq(
  File.read(File.join(USERDATA_REAL, 'Game.rxdata')),
  'the one save',
  'content survives self-migration'
)

# --- Collision backups get unique names ---
reset_workspace!
old = File.join(USERDATA_REAL, 'old1.rxdata')
mid = File.join(USERDATA_REAL, 'old2.rxdata')
dst = File.join(GAME, 'Game.rxdata')
File.write(old, 'oldest')
File.write(mid, 'middle')
File.write(dst, 'newest')
now = Time.now
set_mtime(old, now - 300)
set_mtime(mid, now - 200)
set_mtime(dst, now - 100)
load_windows_fs!("#{USERDATA_REAL}/")

MKXPSaveFS.migrate_save_file(old, dst)
MKXPSaveFS.migrate_save_file(mid, dst)
assert_eq(File.read(dst), 'newest', 'newest keeps the canonical name')
game_entries = raw_entries(GAME)
assert_true(game_entries.include?('Game.rxdata.pre-literal.bak'), 'first backup name used')
assert_true(game_entries.include?('Game.rxdata.pre-literal-2.bak'), 'second backup gets a unique name')
backups = [
  File.read(File.join(GAME, 'Game.rxdata.pre-literal.bak')),
  File.read(File.join(GAME, 'Game.rxdata.pre-literal-2.bak'))
].sort
assert_eq(backups, %w[middle oldest], 'no backup clobbered another')

FileUtils.rm_rf(WORK)
test_passed('test_save_recovery', 25)
