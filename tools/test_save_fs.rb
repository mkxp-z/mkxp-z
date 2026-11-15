#!/usr/bin/env ruby
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# Unit tests for MKXPSaveFS save-path handling in windows_fs.rb:
# bare-filename remap into UserData, literal portable "Save Data",
# UserData-root protections, and the alias->literal save migration.
# Case-semantics tests live in test_save_fs_case.rb.
# Run: ruby mkxp-z-apple-mobile/tools/test_save_fs.rb

require_relative 'save_fs_harness'

# The launcher identity marks host files (MKXPSaveFS.host_artifact?).
$userAgent = 'host'

reset_workspace!
load_windows_fs!

base = MKXPSaveFS.root
assert_eq(base, USERDATA, 'root')

# --- Every path resolves literally ---
[
  'Save01.rvdata2', 'Save/Save01.rvdata2', 'Save Data/Save01.rvdata2',
  'Save', 'Data/Map001.rvdata2', '/etc/passwd'
].each do |path|
  assert_eq(MKXPSaveFS.path_for(path), path, "literal path_for: #{path}")
end

# --- UserData root listings hide engine-internal entries ---
File.write(File.join(USERDATA, 'keybindings.mkxp3'), '')
File.write(File.join(USERDATA, 'Save03.rvdata2'), 'x')
entries = Dir.entries(USERDATA)
assert_false(entries.include?('keybindings.mkxp3'), 'engine file filtered')
assert_true(entries.include?('Save03.rvdata2'), 'save file kept')
root_list = Dir.entries("#{USERDATA}/")
assert_false(root_list.include?('keybindings.mkxp3'), 'data_directory listing filters engine file')
assert_true(root_list.include?('Save03.rvdata2'), 'data_directory listing keeps saves')

# --- UserData root is host-owned: mkdir/rmdir no-op, never destroy ---
assert_eq(Dir.mkdir(USERDATA), 0, 'mkdir of root no-op')
assert_eq(Dir.mkdir("#{USERDATA}/"), 0, 'mkdir of root (trailing sep) no-op')
assert_eq(Dir.rmdir(USERDATA), 0, 'rmdir of root no-op')
assert_eq(Dir.delete("#{USERDATA}/"), 0, 'Dir.delete of root no-op')
assert_true(raw_dir?(USERDATA), 'UserData survives root rmdir')

# --- Portable mode is literal: the Rejuvenation PBDebug flow ---
#   getSaveFolder: Dir.mkdir("Save Data/") unless File.exists?("Save Data/")
#   PBDebug:       Dir.mkdir("Save Data/" + "Battle Debug Logs/") unless ...
# Both must really create directories in the game folder, like on
# Windows/JoiPlay.
Dir.chdir(GAME) do
  # rubocop:disable Lint/DeprecatedClassMethods -- the deprecated
  # aliases are exactly what Reborn-lineage getSaveFolder/PBDebug
  # call. Exercise their wrappers.
  assert_false(File.exists?('Save Data/'), 'portable dir absent before mkdir')
  assert_false(Dir.exist?('Save Data/'), 'Dir.exist? portable dir absent')
  Dir.mkdir('Save Data/')
  assert_true(raw_dir?(File.join(GAME, 'Save Data')), 'portable dir created in game folder')
  assert_true(Dir.exist?('Save Data'), 'Dir.exist? sees literal portable dir')
  assert_true(Dir.exists?('Save Data'), 'Dir.exists? sees literal portable dir')

  logdir = 'Save Data/Battle Debug Logs/'
  assert_false(File.exists?(logdir), 'log subdir absent before mkdir')
  # rubocop:enable Lint/DeprecatedClassMethods
  Dir.mkdir(logdir)
  assert_true(
    raw_dir?(File.join(GAME, 'Save Data', 'Battle Debug Logs')),
    'log subdir created in game folder'
  )

  logfile = "#{logdir}battlelog - test.txt"
  File.open(logfile, 'a+b') { |f| f.write("x\r\n") }
  assert_true(
    File.exist?(File.join(GAME, 'Save Data', 'Battle Debug Logs', 'battlelog - test.txt')),
    'log file written in game folder'
  )
  assert_false(
    raw_entries(USERDATA).include?('Battle Debug Logs'),
    'UserData untouched by portable logging'
  )

  assert_true(
    Dir.entries('Save Data/Battle Debug Logs').include?('battlelog - test.txt'),
    'entries lists literal log subdir'
  )
  assert_true(
    Dir.children('Save Data').include?('Battle Debug Logs'),
    'children lists literal portable dir'
  )
  ngplus = []
  Dir.each_child('Save Data/') { |entry| ngplus << entry }
  assert_true(ngplus.include?('Battle Debug Logs'), 'each_child lists literal portable dir')

  globbed = Dir.glob('Save Data/Battle Debug Logs/battlelog - *.txt')
  assert_eq(globbed.length, 1, 'glob finds literal log file')
  File.delete(globbed[0])
  Dir.rmdir(logdir)
  Dir.rmdir('Save Data')
  assert_false(raw_entries(GAME).include?('Save Data'), 'game can remove its portable dir')

  plain = File.join(GAME, 'PlainDir')
  Dir.mkdir(plain)
  File.write(File.join(plain, 'a.txt'), '')
  assert_true(Dir.children('PlainDir').include?('a.txt'), 'children passthrough')
  passthrough = []
  Dir.each_child('PlainDir') { |entry| passthrough << entry }
  assert_true(passthrough.include?('a.txt'), 'each_child passthrough')
end

# --- Portable-save sweep: a file access under "Save Data/" that
# misses literally while the flattened counterpart sits at the root
# recovers everything. Rejuvenation goes portable on the launcher
# identity alone (RTP.isPortable -> mobile? -> $host) - no marker,
# no $joiplay - so no boot-time gate could see it. ---
reset_workspace!
File.write(File.join(USERDATA, 'Game.rxdata'), 'active root save')
File.write(File.join(USERDATA, 'Save 1 - Aevis - 5h 3m - 2 badges.rxdata'), 'slot')
File.write(File.join(USERDATA, 'Game.rxdata.bak'), 'root backup')
File.write(File.join(USERDATA, 'Settings.dat'), 'options')
File.write(File.join(USERDATA, 'updater.log'), 'patch history')
File.write(File.join(USERDATA, '.portable'), '')
File.write(File.join(USERDATA, 'keybindings.mkxp3'), '')
File.write(File.join(USERDATA, 'Game.rxdata.host-displaced.bak'), 'host artifact')
File.write(File.join(USERDATA, '.host-origin.json'), '{}')
FileUtils.mkdir_p(File.join(USERDATA, 'Battle Logs'))
File.write(File.join(USERDATA, 'Battle Logs', 'log1.txt'), 'battle log')
Dir.chdir(GAME) { load_windows_fs!(false) }
Dir.chdir(GAME) do
  assert_true(File.exist?('Save Data/Game.rxdata'), 'qualifying access recovers the stranded save')
end

migrated = File.join(GAME, 'Save Data')
assert_true(raw_dir?(migrated), 'sweep created the portable dir')
assert_eq(File.read(File.join(migrated, 'Game.rxdata')), 'active root save', 'save migrated')
assert_true(
  raw_entries(migrated).include?('Save 1 - Aevis - 5h 3m - 2 badges.rxdata'),
  'slot save migrated'
)
assert_true(raw_entries(migrated).include?('Game.rxdata.bak'), 'backup migrated')
assert_true(raw_entries(migrated).include?('Settings.dat'), 'settings migrated')
assert_true(raw_entries(migrated).include?('updater.log'), 'log migrated')
assert_true(raw_entries(migrated).include?('.portable'), 'stranded marker migrated')
assert_eq(
  File.read(File.join(migrated, 'Battle Logs', 'log1.txt')),
  'battle log',
  'subdirectory content migrated'
)
leftover = raw_entries(USERDATA)
assert_false(leftover.include?('Game.rxdata'), 'root save gone after the sweep')
assert_false(leftover.include?('Battle Logs'), 'root subdirectory gone after the sweep')
assert_true(leftover.include?('keybindings.mkxp3'), 'engine file stays at the root')
assert_true(leftover.include?('Game.rxdata.host-displaced.bak'), 'host backup stays at the root')
assert_true(leftover.include?('.host-origin.json'), 'host marker stays at the root')

# One sweep per boot. The next boot converges later stragglers.
File.write(File.join(USERDATA, 'Save03.rvdata2'), 'later straggler')
Dir.chdir(GAME) do
  assert_false(File.exist?('Save Data/Save03.rvdata2'), 'no second sweep in one boot')
end
Dir.chdir(GAME) { load_windows_fs!(false) }
Dir.chdir(GAME) do
  assert_true(File.exist?('Save Data/Save03.rvdata2'), 'next boot sweeps stragglers')
end

# --- Portable-save sweep: enumerating the portable dir triggers it
# too - Rejuvenation's load screen lists Dir.new(getSaveFolder) ---
reset_workspace!
FileUtils.mkdir_p(File.join(GAME, 'Save Data'))
File.write(File.join(USERDATA, 'Game.rxdata'), 'listed save')
Dir.chdir(GAME) { load_windows_fs!(false) }
Dir.chdir(GAME) do
  lister = Dir.new('Save Data')
  begin
    names = []
    lister.each { |entry| names << entry }
    assert_true(names.include?('Game.rxdata'), 'Dir.new listing sees the recovered save')
  ensure
    lister.close
  end
end

# The glob spelling of the same enumeration.
reset_workspace!
FileUtils.mkdir_p(File.join(GAME, 'Save Data'))
File.write(File.join(USERDATA, 'Game.rxdata'), 'globbed save')
Dir.chdir(GAME) { load_windows_fs!(false) }
Dir.chdir(GAME) do
  assert_true(
    Dir.glob('Save Data/*.rxdata').include?('Save Data/Game.rxdata'),
    'portable glob sees the recovered save'
  )
end

# --- Sweep safety: a probe without stranded evidence never sweeps.
# A non-portable game that only checks for the marker keeps reading
# its saves at the UserData root. ---
reset_workspace!
File.write(File.join(USERDATA, 'Game.rxdata'), 'non-portable save')
Dir.chdir(GAME) { load_windows_fs!(false) }
Dir.chdir(GAME) do
  assert_false(File.exist?('Save Data/.portable'), 'marker probe misses')
  assert_true(raw_entries(USERDATA).include?('Game.rxdata'), 'root save untouched by the probe')
  assert_false(raw_entries(GAME).include?('Save Data'), 'no portable dir appears')
end

# JoiPlay compat alone is not evidence either: a game with the
# toggle on that never reads "Save Data" keeps its root saves.
reset_workspace!
File.write(File.join(USERDATA, 'Game.rxdata'), 'joiplay root save')
Dir.chdir(GAME) { load_windows_fs!(true) }
Dir.chdir(GAME) do
  assert_true(File.exist?(File.join(USERDATA, 'Game.rxdata')), 'root save stays readable')
end
assert_true(raw_entries(USERDATA).include?('Game.rxdata'), 'joiplay boot leaves root saves alone')
assert_false(raw_entries(GAME).include?('Save Data'), 'joiplay boot creates no portable dir')

# --- Sweep collision, alias-era shape: the root copy is the newer
# alias-active save, it wins. The stale literal copy is kept as
# *.pre-literal.bak. Trigger: Dir.entries listing. ---
reset_workspace!
FileUtils.mkdir_p(File.join(GAME, 'Save Data'))
File.write(File.join(GAME, 'Save Data', 'Game.rxdata'), 'old literal save')
set_mtime(File.join(GAME, 'Save Data', 'Game.rxdata'), Time.now - 86_400)
File.write(File.join(USERDATA, 'Game.rxdata'), 'root active save')
Dir.chdir(GAME) { load_windows_fs!(false) }
Dir.chdir(GAME) do
  assert_true(Dir.entries('Save Data').include?('Game.rxdata'), 'listing triggers the sweep')
end
assert_eq(
  File.read(File.join(GAME, 'Save Data', 'Game.rxdata')),
  'root active save',
  'collision: newer root copy wins'
)
assert_eq(
  File.read(File.join(GAME, 'Save Data', 'Game.rxdata.pre-literal.bak')),
  'old literal save',
  'collision: stale literal copy kept as backup'
)

# --- Sweep collision, post-literal shape: a stale straggler at the
# root must NOT displace the newer save the player has since written
# into the literal folder. Trigger: Dir.children listing. ---
reset_workspace!
FileUtils.mkdir_p(File.join(GAME, 'Save Data'))
File.write(File.join(GAME, 'Save Data', 'Game.rxdata'), 'current literal save')
File.write(File.join(USERDATA, 'Game.rxdata'), 'stale root save')
set_mtime(File.join(USERDATA, 'Game.rxdata'), Time.now - 86_400)
Dir.chdir(GAME) { load_windows_fs!(false) }
Dir.chdir(GAME) do
  assert_true(Dir.children('Save Data').include?('Game.rxdata'), 'children triggers the sweep')
end
assert_eq(
  File.read(File.join(GAME, 'Save Data', 'Game.rxdata')),
  'current literal save',
  'collision: newer literal save keeps canonical name'
)
assert_eq(
  File.read(File.join(GAME, 'Save Data', 'Game.rxdata.pre-literal.bak')),
  'stale root save',
  'collision: stale root copy archived'
)
assert_false(
  raw_entries(USERDATA).include?('Game.rxdata'),
  'collision: root straggler removed either way'
)

# --- Sweep merge into an existing subdirectory: stranded and
# literal files coexist. Per-file collisions follow the mtime rule.
# Trigger: Dir.foreach of the subdirectory. ---
reset_workspace!
FileUtils.mkdir_p(File.join(GAME, 'Save Data', 'Battle Logs'))
File.write(File.join(GAME, 'Save Data', 'Battle Logs', 'new.txt'), 'stale literal log')
set_mtime(File.join(GAME, 'Save Data', 'Battle Logs', 'new.txt'), Time.now - 86_400)
FileUtils.mkdir_p(File.join(USERDATA, 'Battle Logs'))
File.write(File.join(USERDATA, 'Battle Logs', 'old.txt'), 'stranded log')
File.write(File.join(USERDATA, 'Battle Logs', 'new.txt'), 'stranded newer log')
Dir.chdir(GAME) { load_windows_fs!(false) }
Dir.chdir(GAME) do
  seen = []
  Dir.foreach('Save Data/Battle Logs') { |entry| seen << entry }
  assert_true(seen.include?('old.txt'), 'foreach triggers the sweep')
end
logs = File.join(GAME, 'Save Data', 'Battle Logs')
assert_eq(File.read(File.join(logs, 'old.txt')), 'stranded log', 'merge keeps stranded file')
assert_eq(File.read(File.join(logs, 'new.txt')), 'stranded newer log', 'merge: newer root copy wins')
assert_eq(
  File.read(File.join(logs, 'new.txt.pre-literal.bak')),
  'stale literal log',
  'merge: stale literal copy kept as backup'
)
assert_false(raw_entries(USERDATA).include?('Battle Logs'), 'merged root subdirectory removed')

# A stranded name that collides with a literal directory of another
# type stays at the root instead of clobbering it.
reset_workspace!
FileUtils.mkdir_p(File.join(GAME, 'Save Data'))
File.write(File.join(GAME, 'Save Data', 'Exports'), 'literal file named like a dir')
FileUtils.mkdir_p(File.join(USERDATA, 'Exports'))
File.write(File.join(USERDATA, 'Exports', 'mon.pk'), 'stranded export')
File.write(File.join(USERDATA, 'Game.rxdata'), 'save beside the clash')
Dir.chdir(GAME) { load_windows_fs!(false) }
Dir.chdir(GAME) do
  assert_true(File.exist?('Save Data/Game.rxdata'), 'sweep still runs around the clash')
end
assert_eq(
  File.read(File.join(GAME, 'Save Data', 'Exports')),
  'literal file named like a dir',
  'type clash: literal file untouched'
)
assert_eq(
  File.read(File.join(USERDATA, 'Exports', 'mon.pk')),
  'stranded export',
  'type clash: stranded tree left in place'
)

# --- Shipped marker: the game reads the portable dir, so listings
# recover stranded saves without $joiplay ---
reset_workspace!
FileUtils.mkdir_p(File.join(GAME, 'Save Data'))
File.write(File.join(GAME, 'Save Data', '.portable'), '')
File.write(File.join(USERDATA, 'Game.rxdata'), 'marker save')
Dir.chdir(GAME) { load_windows_fs!(false) }
Dir.chdir(GAME) do
  ngplus = []
  Dir.each_child('Save Data') { |entry| ngplus << entry }
  assert_true(ngplus.include?('Game.rxdata'), 'each_child listing sees the recovered save')
end
assert_eq(
  File.read(File.join(GAME, 'Save Data', 'Game.rxdata')),
  'marker save',
  'marker install recovered through the listing'
)

# --- Legacy-save recovery ---
# Earlier builds redirected bare working-directory save names into
# UserData, so upgraded installs have saves stranded at the UserData
# root. The first bare-name access (probe, read, glob, load_data)
# moves the stranded copy into the game folder. After that every
# operation is literal. Modern games address UserData by absolute
# path and never trigger it.
reset_workspace!
Dir.chdir(GAME) { load_windows_fs!(false) }
File.write(File.join(USERDATA, 'Game.rxdata'), 'stranded save')
File.write(File.join(USERDATA, 'keybindings.mkxp3'), '')
Dir.chdir(GAME) do
  assert_true(File.exist?('Game.rxdata'), 'probe recovers the stranded save')
  assert_true(raw_entries(GAME).include?('Game.rxdata'), 'save now lives in the game folder')
  assert_false(raw_entries(USERDATA).include?('Game.rxdata'), 'root copy gone')
  assert_eq(File.read('Game.rxdata'), 'stranded save', 'recovered content intact')

  # New writes are literal from the first byte.
  File.write('Save05.rxdata', 'fresh')
  assert_true(raw_entries(GAME).include?('Save05.rxdata'), 'new save lands in the game folder')
  assert_false(raw_entries(USERDATA).include?('Save05.rxdata'), 'UserData untouched by new writes')
  assert_eq(File.binread('Save05.rxdata'), 'fresh', 'binread agrees with the write')
end

# Recovery through glob: a bare save pattern sweeps matching stranded
# saves in before the literal glob, so slot enumeration and the open
# that follows agree. Engine-internal files never move.
File.write(File.join(USERDATA, 'Save06.rxdata'), 'stranded slot')
Dir.chdir(GAME) do
  listed = Dir.glob('*.rxdata')
  assert_true(listed.include?('Save06.rxdata'), 'glob lists the recovered slot')
  assert_true(raw_entries(GAME).include?('Save06.rxdata'), 'glob recovery moved the file')
  assert_eq(File.read('Save06.rxdata'), 'stranded slot', 'glob-recovered content intact')

  Dir.glob('*')
  assert_true(
    raw_entries(USERDATA).include?('keybindings.mkxp3'),
    'bare * glob cannot drag engine files into the game folder'
  )
end

# Recovery through load_data (how ancient games load saves).
File.write(File.join(USERDATA, 'Save07.rxdata'), 'loadable')
Dir.chdir(GAME) do
  load_data('Save07.rxdata')
  assert_true(raw_entries(GAME).include?('Save07.rxdata'), 'load_data recovers the stranded save')
end

# Collision: the newer copy keeps the canonical name. The loser is
# kept as *.pre-literal.bak. Shipped starter save vs real progress:
reset_workspace!
Dir.chdir(GAME) { load_windows_fs!(false) }
File.write(File.join(GAME, 'Game.rxdata'), 'shipped starter')
set_mtime(File.join(GAME, 'Game.rxdata'), Time.now - 86_400)
File.write(File.join(USERDATA, 'Game.rxdata'), 'real progress')
Dir.chdir(GAME) do
  assert_eq(File.read('Game.rxdata'), 'real progress', 'newer stranded save wins the name')
  assert_eq(
    File._mkxp_orig_open('Game.rxdata.pre-literal.bak', 'rb', &:read),
    'shipped starter',
    'starter save kept as backup'
  )
end

# Reverse collision: a stale root straggler must not displace the
# save the player has since written into the game folder.
reset_workspace!
Dir.chdir(GAME) { load_windows_fs!(false) }
File.write(File.join(GAME, 'Game.rxdata'), 'current save')
File.write(File.join(USERDATA, 'Game.rxdata'), 'stale straggler')
set_mtime(File.join(USERDATA, 'Game.rxdata'), Time.now - 86_400)
Dir.chdir(GAME) do
  assert_eq(File.read('Game.rxdata'), 'current save', 'current save keeps the name')
  assert_true(raw_entries(GAME).include?('Game.rxdata.pre-literal.bak'), 'straggler archived')
  assert_false(raw_entries(USERDATA).include?('Game.rxdata'), 'root straggler removed')
end

# Modern games: absolute UserData paths never trigger recovery.
reset_workspace!
Dir.chdir(GAME) { load_windows_fs!(false) }
modern = File.join(USERDATA, 'Game.rvdata2')
File.write(modern, 'modern save')
Dir.chdir(GAME) do
  assert_true(File.exist?(modern), 'absolute UserData probe works')
  assert_eq(File.read(modern), 'modern save', 'absolute UserData read works')
  assert_true(raw_entries(USERDATA).include?('Game.rvdata2'), 'modern save stays in UserData')
  assert_false(raw_entries(GAME).include?('Game.rvdata2'), 'modern save never moves')

  # Non-save globs stay fully literal.
  File.write(File.join(GAME, 'readme.txt'), '')
  assert_eq(Dir.glob('*.txt'), ['readme.txt'], 'non-save glob passthrough')
end

# --- Cross-API consistency matrix ---
# The property every savefs bug in this file's history violated:
# every wrapped API must agree about where a given path points. The
# matrix deliberately does NOT assert where anything lands on disk -
# the targeted tests above own that - only that create/probe/write/
# read/enumerate/delete see one coherent world through every spelling
# a game might use. New wrappers must be added to these API lists.
# rubocop:disable Lint/DeprecatedClassMethods -- the deprecated
# spellings are part of the API surface games call. The matrix must
# hold for them too.
FILE_PROBES = {
  'File.exist?' => lambda { |p| File.exist?(p) },
  'File.exists?' => lambda { |p| File.exists?(p) },
  'FileTest.exist?' => lambda { |p| FileTest.exist?(p) },
  'FileTest.exists?' => lambda { |p| FileTest.exists?(p) },
  'File.file?' => lambda { |p| File.file?(p) }
}.freeze
DIR_PROBES = {
  'Dir.exist?' => lambda { |p| Dir.exist?(p) },
  'Dir.exists?' => lambda { |p| Dir.exists?(p) },
  'File.directory?' => lambda { |p| File.directory?(p) },
  'FileTest.directory?' => lambda { |p| FileTest.directory?(p) }
}.freeze
# rubocop:enable Lint/DeprecatedClassMethods
FILE_READERS = {
  'File.read' => lambda { |p| File.read(p) },
  'File.binread' => lambda { |p| File.binread(p) },
  # rubocop:disable Style/FileRead -- File.open IS the API under comparison.
  'File.open' => lambda { |p| File.open(p, 'rb', &:read) },
  # rubocop:enable Style/FileRead
  'File.readlines' => lambda { |p| File.readlines(p).join }
}.freeze
DIR_LISTERS = {
  'Dir.entries' => lambda { |d| Dir.entries(d) - ['.', '..'] },
  'Dir.children' => lambda { |d| Dir.children(d) },
  'Dir.each_child(block)' => lambda { |d|
    acc = []
    Dir.each_child(d) { |e| acc << e }
    acc
  },
  'Dir.foreach(block)' => lambda { |d|
    acc = []
    Dir.foreach(d) { |e| acc << e }
    acc - ['.', '..']
  }
}.freeze

def assert_probes(probes, path, expected, label)
  probes.each do |name, probe|
    assert_eq(probe.call(path), expected, "#{label}: #{name}(#{path.inspect}) == #{expected}")
  end
end

def assert_lifecycle_consistent(file_path, label)
  dir_path = File.dirname(file_path)
  basename = File.basename(file_path)
  content = "payload for #{label}"

  assert_probes(FILE_PROBES, file_path, false, "#{label} before write")

  File.write(file_path, content)
  assert_probes(FILE_PROBES, file_path, true, "#{label} after write")
  assert_eq(File.size(file_path), content.length, "#{label}: File.size sees the write")
  FILE_READERS.each do |name, reader|
    assert_eq(reader.call(file_path), content, "#{label}: #{name} reads the write")
  end
  File.mtime(file_path)

  assert_listings(dir_path, basename, label)

  File.delete(file_path)
  assert_probes(FILE_PROBES, file_path, false, "#{label} after delete")
  DIR_LISTERS.each do |name, lister|
    assert_false(lister.call(dir_path).include?(basename), "#{label}: #{name} forgets the delete")
  end
end

def assert_listings(dir_path, basename, label)
  DIR_LISTERS.each do |name, lister|
    assert_true(
      lister.call(dir_path).include?(basename),
      "#{label}: #{name} lists the write"
    )
  end
  globbed = Dir.glob(File.join(dir_path, '*'))
  assert_true(
    globbed.any? { |entry| File.basename(entry) == basename },
    "#{label}: glob array form lists the write"
  )
  block_glob = []
  Dir.glob(File.join(dir_path, '*')) { |entry| block_glob << entry }
  assert_eq(block_glob, globbed, "#{label}: glob block form matches array form")
end

def assert_dir_lifecycle_consistent(dir_path, label)
  assert_probes(DIR_PROBES, dir_path, false, "#{label} before mkdir")
  Dir.mkdir(dir_path)
  assert_probes(DIR_PROBES, dir_path, true, "#{label} after mkdir")

  assert_lifecycle_consistent(File.join(dir_path, 'inner.rxdata'), "#{label}/inner")

  Dir.rmdir(dir_path)
  assert_probes(DIR_PROBES, dir_path, false, "#{label} after rmdir")
end

reset_workspace!
Dir.chdir(GAME) { load_windows_fs!(false) }
Dir.chdir(GAME) do
  # Bare save filename - fully literal, no asymmetry left.
  assert_lifecycle_consistent('Matrix.rxdata', 'bare save name')

  # The portable dir, a subdirectory of it, and files inside both.
  assert_dir_lifecycle_consistent('Save Data', 'portable dir')
  Dir.mkdir('Save Data')
  assert_dir_lifecycle_consistent('Save Data/Battle Debug Logs', 'portable subdir')
  assert_lifecycle_consistent('Save Data/Slot 1.rxdata', 'portable save file')
  Dir.rmdir('Save Data')

  # Absolute spelling inside UserData.
  assert_lifecycle_consistent(File.join(USERDATA, 'Abs.rxdata'), 'absolute UserData file')

  # Plain non-save relative paths.
  assert_dir_lifecycle_consistent('PlainMatrix', 'plain dir')
end

# Windows refuses to open a folder. Games rescue EACCES, not EISDIR.
Dir.chdir(GAME) do
  Dir.mkdir('Characters')
  [['File.open', -> { File.open('Characters/', 'rb') { |f| f.read(1) } }],
   ['File.new', -> { File.new('Characters', 'rb') }],
   ['File.open with a stray CR', -> { File.open("Characters/\r", 'rb') { |f| f.read(1) } }],
   ['File.new with a stray CR', -> { File.new("Characters\r", 'rb').read(1) }]].each do |name, call|
    raised = begin
      call.call
      nil
    rescue SystemCallError => e
      e.class
    end
    assert_eq(raised, Errno::EACCES, "#{name} on a folder raises EACCES")
  end
  Dir.rmdir('Characters')
end

def as_ruby_version(version)
  real = RUBY_VERSION
  prev_verbose = $VERBOSE
  $VERBOSE = nil
  Object.send(:remove_const, :RUBY_VERSION)
  Object.const_set(:RUBY_VERSION, version)
  yield
ensure
  Object.send(:remove_const, :RUBY_VERSION)
  Object.const_set(:RUBY_VERSION, real)
  $VERBOSE = prev_verbose
end

def open_error
  yield
  nil
rescue StandardError => e
  e.class
end

# Ruby 1.8.1 to 1.8.6 of RGSS1 and RGSS2 give the path to the C library,
# so the path ends at the first NUL. GS Vitro's safeExists? rescues only
# Errno.
Dir.chdir(GAME) do
  File.open('Battler.png', 'wb') { |f| f.write('x') }
  Dir.mkdir('Characters')
  File.open('Characters/hero.png', 'wb') { |f| f.write('h') }
  nul_calls = {
    'File.open' => -> { File.open("Battler.png\0junk", 'rb', &:read) },
    'File.new' => -> { File.new("Battler.png\0junk", 'rb').tap(&:close) && 'x' },
    'File.read' => -> { File.read("Battler.png\0junk") },
    'File.readlines' => -> { File.readlines("Battler.png\0junk").join },
    'File.foreach' => -> { File.foreach("Battler.png\0junk").to_a.join },
    'IO.read' => -> { IO.read("Battler.png\0junk") },
    'IO.readlines' => -> { IO.readlines("Battler.png\0junk").join },
    'IO.foreach' => -> { IO.foreach("Battler.png\0junk").to_a.join },
    'Kernel#open' => -> { open("Battler.png\0junk", 'rb', &:read) },
    'Dir.chdir' => -> { Dir.chdir("Characters\0junk") { File.basename(Dir.pwd) } },
    'Dir.entries' => -> { Dir.entries("Characters\0junk").sort },
    'Dir.foreach' => -> { Dir.foreach("Characters\0junk").to_a.sort },
    'Dir.new' => -> { Dir.new("Characters\0junk").tap(&:close).path },
    'Dir.open' => -> { Dir.open("Characters\0junk") { |d| d.path } },
    'Dir.mkdir' => -> { Dir.mkdir("Made\0junk") && File.directory?('Made') },
    'Dir.rmdir' => -> { Dir.rmdir("Made\0junk") && File.exist?('Made') }
  }
  cut_results = {
    'File.open' => 'x', 'File.new' => 'x', 'File.read' => 'x', 'File.readlines' => 'x',
    'File.foreach' => 'x', 'IO.read' => 'x', 'IO.readlines' => 'x', 'IO.foreach' => 'x',
    'Kernel#open' => 'x', 'Dir.chdir' => 'Characters', 'Dir.entries' => %w[. .. hero.png],
    'Dir.foreach' => %w[. .. hero.png], 'Dir.new' => 'Characters', 'Dir.open' => 'Characters',
    'Dir.mkdir' => true, 'Dir.rmdir' => false
  }
  checked_calls = {
    'File.exist?' => -> { File.exist?("Battler.png\0junk") },
    'File.directory?' => -> { File.directory?("Characters\0junk") },
    'File.file?' => -> { File.file?("Battler.png\0junk") },
    'File.size' => -> { File.size("Battler.png\0junk") },
    'File.mtime' => -> { File.mtime("Battler.png\0junk") },
    'File.delete' => -> { File.delete("Battler.png\0junk") },
    'File.rename' => -> { File.rename("Battler.png\0junk", 'Other.png') }
  }
  assert_eq(IO.respond_to?(:_mkxp_orig_io_read), false, 'Ruby 1.9 and later keep the original IO.read')
  assert_eq(Kernel.private_method_defined?(:_mkxp_orig_kernel_open), false, 'Ruby 1.9 and later keep the original Kernel#open')
  as_ruby_version('1.8.1') do
    load_windows_fs!(false)
    nul_calls.each do |name, call|
      assert_eq(call.call, cut_results[name], "#{name} on Ruby 1.8 uses the path before the NUL")
    end
    checked_calls.each do |name, call|
      assert_eq(open_error(&call), ArgumentError, "#{name} on Ruby 1.8 refuses a NUL, like Ruby 1.8.1")
    end
    File.open("Saved.txt\0junk", 'wb') { |f| f.write('y') }
    assert_eq(File._mkxp_orig_read('Saved.txt'), 'y', "File.open 'wb' on Ruby 1.8 writes the path before the NUL")
    open("Opened.txt\0junk", 'wb') { |f| f.write('z') }
    assert_eq(File._mkxp_orig_read('Opened.txt'), 'z', "Kernel#open 'wb' on Ruby 1.8 writes the path before the NUL")
    assert_eq(open_error { File.open("\0junk", 'wb') { nil } }, Errno::ENOENT,
              "File.open 'wb' on Ruby 1.8 with a NUL first raises ENOENT")
    assert_eq(open_error { File.open("Missing.png\0junk", 'rb') { nil } }, Errno::ENOENT,
              'File.open on Ruby 1.8 with a NUL and no file raises ENOENT')
    assert_eq(open_error { File.new("Missing.png\0junk", 'rb') }, Errno::ENOENT,
              'File.new on Ruby 1.8 with a NUL and no file raises ENOENT')
    assert_eq(open_error { File.open("Characters/\0junk", 'rb') { nil } }, Errno::EACCES,
              'File.open on Ruby 1.8 with a NUL after a folder raises EACCES')
  end
  %w[1.9.3 3.1.3].each do |version|
    as_ruby_version(version) do
      nul_calls.merge(checked_calls).each do |name, call|
        assert_eq(open_error(&call), ArgumentError, "#{name} on Ruby #{version} refuses a NUL, like RGSS3")
      end
      assert_eq(open_error { File.open('', 'wb') { nil } }, Errno::ENOENT,
                "File.open 'wb' on Ruby #{version} with an empty path raises ENOENT")
    end
  end
  assert_false(File.exist?('Made'), 'a refused Dir.mkdir makes no folder')
  File.delete('Characters/hero.png', 'Saved.txt', 'Opened.txt', 'Battler.png')
  Dir.rmdir('Characters')
end

# On Windows, Ruby 1.8.1, 1.9.2 and 3.1.0 raise TypeError for a nil
# path and EINVAL for an empty one, and stay in the folder. Only
# Dir.chdir with no path goes to HOME, which on a device is the app
# folder, not the game folder.
Dir.chdir(GAME) do
  { '1.8.1' => ['', "\0junk"], '3.1.3' => [''] }.each do |version, empty_paths|
    as_ruby_version(version) do
      load_windows_fs!(false)
      assert_eq(open_error { Dir.chdir(nil) }, TypeError, "Dir.chdir(nil) on Ruby #{version} raises TypeError")
      assert_eq(open_error { Dir.chdir(nil) { nil } }, TypeError,
                "Dir.chdir(nil) with a block on Ruby #{version} raises TypeError")
      empty_paths.each do |path|
        assert_eq(open_error { Dir.chdir(path) }, Errno::EINVAL, "Dir.chdir(#{path.inspect}) on Ruby #{version} raises EINVAL")
        assert_eq(open_error { Dir.chdir(path) { nil } }, Errno::EINVAL,
                  "Dir.chdir(#{path.inspect}) with a block on Ruby #{version} raises EINVAL")
      end
      assert_eq(Dir.pwd, GAME, "a refused Dir.chdir on Ruby #{version} stays in the game folder")
      assert_eq(Dir.chdir { Dir.pwd }, File.realpath(ENV['HOME']), "Dir.chdir with no path on Ruby #{version} goes to HOME")
      assert_eq(open_error { Dir.chdir('', '.') }, ArgumentError, "Dir.chdir with two paths on Ruby #{version} raises ArgumentError")
    end
  end
  load_windows_fs!(false)
end

test_passed('test_save_fs', 411)
