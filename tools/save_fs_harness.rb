# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# Shared harness for the save-fs test files (test_save_fs.rb and
# test_save_fs_case.rb). Workspace setup, windows_fs loading,
# assertions, raw probes, and the device case-semantics emulation
# live here. The file name stays outside the tools/test_*.rb CI glob
# on purpose: this file is a library, not a test.

require 'fileutils'
require_relative 'assertion_count'

# The tests simulate a device (case-sensitive) filesystem on top of a
# case-folding host: an MKXPSaveFS.engine_fs fake answers the engine
# facts strictly, and with_recorded_raw_paths proves the wiring from
# the paths handed to the raw syscalls. On a genuinely case-sensitive
# host the simulation collides with the real disk - mkdir_p('audio/...')
# resolves onto the existing 'Audio' and raises EEXIST - so the run
# proves nothing rather than finding a defect. Linux CI is
# case-sensitive. Skip there instead of failing.
# Probes the filesystem the workspace will live on, not the system
# temp dir: on macOS those two can differ in case behaviour.
def host_folds_case?
  probe = File.join(__dir__, "case_probe_#{Process.pid}")
  FileUtils.mkdir_p(probe)
  File.write(File.join(probe, 'Probe.TXT'), 'x')
  File.exist?(File.join(probe, 'probe.txt'))
ensure
  FileUtils.rm_rf(probe)
end

unless host_folds_case?
  name = File.basename($PROGRAM_NAME)
  # A skip that reads like a pass is worse than no test. CI runs these
  # on a case-folding host as well, and sets MKXP_TESTS_NO_SKIP there
  # so the skip cannot hide a break.
  if ENV['MKXP_TESTS_NO_SKIP']
    warn "FAIL: #{name} skipped, but MKXP_TESTS_NO_SKIP is set."
    warn '  This host filesystem is case-sensitive. Run it on one that folds case.'
    exit 1
  end
  puts "SKIP: #{name}, this host filesystem is case-sensitive"
  exit 0
end

ROOT = File.expand_path('..', __dir__)
WORK = File.expand_path('test_save_fs_workspace', __dir__)
USERDATA = File.join(WORK, 'UserData')
GAME = File.join(WORK, 'Game')

def reset_workspace!
  FileUtils.rm_rf(WORK)
  FileUtils.mkdir_p(USERDATA)
  FileUtils.mkdir_p(GAME)
end

# windows_fs.rb targets the in-game VMs (1.8 / 1.9 / 3.1), which
# all still have the `exists?` aliases. Ruby 3.2 removed them. Restore
# them so this harness also runs on a modern host Ruby.
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

# Loads (or reloads) windows_fs.rb, the preload under test. Does NOT
# reset the workspace - callers seed files first, and must chdir into
# GAME when the portable-save sweep's cwd-relative behavior is under
# test. Each load re-arms the once-per-boot sweep.
def load_windows_fs!(joiplay = false)
  ensure_ruby31_compat_aliases!
  Object.send(:remove_const, :System) if defined?(System)
  Object.const_set(:System, Module.new do
    module_function

    define_method(:data_directory) { USERDATA }
    define_method(:puts) { |*args| Kernel.puts(*args) }
    define_method(:joiplay_compat?) { joiplay }
  end)
  unless Kernel.private_method_defined?(:load_data)
    Kernel.module_eval do
      def load_data(_path, *_args); end
      def save_data(_obj, _path, *_args); end
      module_function :load_data, :save_data
    end
  end
  # Reloading the whole file re-evaluates constants that production
  # only defines once per VM. Silence the redefinition warnings.
  prev_verbose = $VERBOSE
  $VERBOSE = nil
  begin
    load File.join(ROOT, 'scripts', 'preload', 'windows_fs.rb')
  ensure
    $VERBOSE = prev_verbose
  end
end

def assert_eq(actual, expected, label)
  asserted
  return if actual == expected

  $stderr.puts "FAIL: #{label}\n  expected: #{expected.inspect}\n  actual:   #{actual.inspect}"
  exit 1
end

def assert_true(value, label)
  assert_eq(value, true, label)
end

def assert_false(value, label)
  assert_eq(value, false, label)
end

# Raw filesystem probes that bypass every wrapper. Ruby's own
# Dir.entries calls the Dir.open wrapper, so list through an instance.
def raw_entries(dir)
  names = []
  handle = Dir._mkxp_orig_new(dir)
  handle._mkxp_orig_each { |name| names << name }
  names
ensure
  handle.close if handle
end

def raw_dir?(path)
  File._mkxp_orig_directory(path)
end

def set_mtime(path, time)
  File.utime(time, time, path)
end

# Shadows the raw-syscall aliases with recorders so the assertions
# can see the exact paths the wrappers hand down. The host filesystem
# folds case, so disk state alone cannot prove the wiring.
RECORDED_RAW_METHODS = [
  [File, :_mkxp_orig_open], [File, :_mkxp_orig_delete],
  [File, :_mkxp_orig_rename], [Dir, :_mkxp_orig_mkdir]
].freeze

def with_recorded_raw_paths
  records = []
  originals = {}
  RECORDED_RAW_METHODS.each do |receiver, name|
    original = receiver.method(name)
    originals[[receiver, name]] = original
    receiver.define_singleton_method(name) do |*args, &blk|
      records << args[0].to_s
      records << args[1].to_s if name == :_mkxp_orig_rename
      original.call(*args, &blk)
    end
  end
  yield records
ensure
  originals.each { |(receiver, name), original| receiver.define_singleton_method(name, original) }
end

# Spelling-strict exist?, the engine's raw-syscall answer on a real
# device. The host folds case, so a directory listing stands in for
# the syscall.
def device_strict_exist?(path)
  str = path.to_s
  raw_entries(File.dirname(str)).include?(File.basename(str))
rescue StandardError
  false
end

def build_boot_case_cache
  cache = {}
  walk = nil
  walk = lambda do |dir, rel|
    raw_entries(dir).each do |name|
      next if ['.', '..'].include?(name)

      path = rel.empty? ? name : "#{rel}/#{name}"
      cache[path.downcase] = path
      full = File.join(dir, name)
      walk.call(full, path) if File._mkxp_orig_directory(full)
    end
  end
  walk.call('.', '')
  cache
end

# Fake for MKXPSaveFS.engine_fs. strict_exist? emulates the device's
# raw case-sensitive syscall, and cache_resolve replays a boot-time
# snapshot. A nil cache models the "pathCache": false engine state,
# where the boot cache answers nothing.
class FakeEngineFS
  def initialize(boot_cache)
    @boot_cache = boot_cache
  end

  def strict_exist?(path)
    device_strict_exist?(path)
  end

  def cache_resolve(rel)
    return nil unless @boot_cache

    @boot_cache[rel.to_s.gsub('\\', '/').downcase]
  end

  def dir_entries(dir)
    raw_entries(dir)
  end
end

def with_engine_fs(adapter)
  MKXPSaveFS.engine_fs = adapter
  yield
ensure
  MKXPSaveFS.engine_fs = nil
end

# Device emulation with the boot case cache available: the engine
# resolves boot-time spellings, and the cache snapshot is taken when
# the fake installs, so files created afterwards stay invisible to
# it - exactly like the engine, where only the live-walk fallback in
# case_variant can resolve them.
def with_device_case_semantics(&block)
  with_engine_fs(FakeEngineFS.new(build_boot_case_cache), &block)
end

# The `_mkxp_orig_*` probe aliases the wrappers yield to answer
# strictly on a real device. The host folds case, so these stubs
# rebuild that strictness. Typed probes keep their type answer
# through the captured host original: the strict-exist gate already
# confirmed the exact spelling, so the host fold cannot lie there.
STRICT_PROBE_STUBS = [
  [File, :_mkxp_orig_exist, false],
  [File, :_mkxp_orig_exists, false],
  [File, :_mkxp_orig_file, true],
  [File, :_mkxp_orig_directory, true],
  [FileTest, :_mkxp_orig_exist, false],
  [FileTest, :_mkxp_orig_exists, false],
  [FileTest, :_mkxp_orig_file, true],
  [FileTest, :_mkxp_orig_directory, true],
  [Dir, :_mkxp_orig_exist, true],
  [Dir, :_mkxp_orig_exists, true]
].freeze

def with_strict_engine_probes
  originals = []
  STRICT_PROBE_STUBS.each do |receiver, name, typed|
    next unless receiver.respond_to?(name)

    original = receiver.method(name)
    originals << [receiver, name, original]
    receiver.define_singleton_method(name) do |path|
      device_strict_exist?(path) && (typed ? original.call(path) : true)
    end
  end
  yield
ensure
  originals.each { |receiver, name, original| receiver.define_singleton_method(name, original) }
end

# Rejuvenation's Windows build ships "pathCache": false. That leaves
# the engine-level probes with only the raw syscall, and the boot
# case cache answers nothing. Model both, so the probe wrappers must
# fold case on their own through the live walk.
def with_pathcache_off_semantics(&block)
  with_engine_fs(FakeEngineFS.new(nil)) do
    with_strict_engine_probes(&block)
  end
end
