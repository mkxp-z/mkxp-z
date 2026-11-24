# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# platform_compat.rb
# Engine-level platform compatibility layer.
# Auto-loaded before game scripts to ensure compatibility.
# Game-specific patches live in separate files (e.g. pokemon_compat.rb).

begin
  require 'zlib'
rescue LoadError
  # zlib is optional for some vintage games. Preload continues without it.
end

# --- JoiPlay-compat signal ---
# Essentials fangames such as Reborn, Rejuvenation, and Desolation
# branch on `$joiplay` to pick between JoiPlay's stripped-down API
# and desktop mkxp-z's extended one. Reborn's `internal_se_play`
# calls `Audio.se_play` on JoiPlay and `Audio.se_play_position` on
# desktop, and this build carries neither of the mkxp-z extensions.
# Our JoiPlay shims (NilClass safe-stubs, Win32API and DL stubs,
# poke_* graphics aliases, cheats, network stubs) make that path
# work often. But `$joiplay` also turns on patches written for
# JoiPlay's old mkxp fork that misbehave here, so the host decides
# per game through `System.joiplay_compat?`, wired to
# mkxp_setJoiplayCompat. Off when the host says nothing.
$joiplay =
  defined?(System) && System.respond_to?(:joiplay_compat?) &&
  System.joiplay_compat?

# --- Thread.critical / Thread.critical= no-op shims (Ruby 1.9+) ---
# Ruby 1.8 had `Thread.critical` and `Thread.critical=` to stop
# thread switching in a critical section. Ruby 1.9 removed both when
# per-thread locks replaced the GIL. Vintage RGSS and Essentials code
# still wraps Marshal.load and save-file I/O in them.
#
# On Ruby 1.9+ those calls raise `NoMethodError`, usually during quit
# or save, where `pbExit` calls `pbSave` which sets
# `Thread.critical = true`. That has crashed the engine outright: the
# error escapes the script-eval loop, and the iOS shutdown path then
# segfaults because the Ruby VM still has an exception pending when
# SharedState::finiInstance() runs.
#
# Restore both as no-ops wherever they are missing. The 1.8
# cooperative-scheduling semantics mean nothing under modern Ruby,
# and calling code only ever cared that the methods existed.
unless Thread.respond_to?(:critical)
  # rubocop:disable Naming/PredicateMethod -- mocking Ruby 1.8's
  # `Thread.critical` reader, which returns a Boolean but is named
  # without `?` for backwards compatibility with the 1.8 API.
  def Thread.critical
    false
  end
  # rubocop:enable Naming/PredicateMethod

  def Thread.critical=(value)
    value
  end
end

# --- exit! / Process.exit! redirect to SystemExit ---
# `Kernel.exit!(status)` and `Process.exit!(status)` skip `at_exit`
# handlers AND bypass the engine's SystemExit rescue. They call
# `_exit(status)` straight away, killing the iOS process before
# mkxp-z can flush graphics state, save the engine log, or fire the
# `mkxp_setEngineTerminated` callback.
#
# Essentials' `pbExit`, and its forks such as Vanguard and Reborn,
# use `exit!` so a quit click skips the "press any key" splash that
# `at_exit` would trigger. That is a fine choice on desktop. On iOS
# the app just vanishes, and the engine never learns the user quit.
#
# Redirect both to `Kernel.exit(status)`. The script-eval loop in
# binding-mri.cpp then catches the SystemExit, sets
# `mkxp_setEngineExitedCleanly()`, fires the terminated callback, and
# the host shows the configured "game ended" screen. This drops the
# desktop "skip at_exit" semantics, which is a safe trade: no
# shipping iOS PE fork relies on that for anything the player sees.
module Kernel
  def exit!(status = false)
    exit(status)
  end
  module_function :exit!
end

module Process
  class << self
    alias _mkxp_orig_exit_bang exit! if respond_to?(:exit!) && !method_defined?(:_mkxp_orig_exit_bang)

    def exit!(status = false)
      Kernel.exit(status)
    end
  end
end

# Suppress Ruby's $DEBUG global. Some plugins gate verbose error
# dialogs / extra print output on `$DEBUG`. Without an explicit
# `false` the engine inherits whatever Ruby's startup defaulted to,
# which on some MRI builds is a truthy state (-d on the cmdline,
# RUBYOPT=-d, etc.). JoiPlay sets this in its preload for the same
# reason. Pinned here so plugins reading it during script load see
# the expected `false`.
$DEBUG = false

# Plugin-probe constants. Old Yanfly-era plugins look at
# `Graphics::PlaneSpeedUp`. Without an explicit definition our
# engine's `const_missing` returns `IOS::NullStub`, which is
# always-truthy and silently flips the plugin's optimization path
# the wrong way. Match JoiPlay's default: define the constant as
# `false` so the plugin's "no plane speedup" branch is taken.
unless defined?(Graphics::PlaneSpeedUp)
  module Graphics
    PlaneSpeedUp = false
  end
end

# Top-level no-op for `set_loop_points(intro_pos, loop_end)`. Some
# custom audio-loop plugins call this from map transitions /
# bgm_play overrides. On JoiPlay's default builds this exists as
# a Kernel-level no-op (preload.rb:119-120). Without it our
# `IOS::NullStub` const_missing path doesn't catch it (it's a
# method call, not a constant ref) and the script raises
# `NoMethodError: undefined method 'set_loop_points'`. The two-arg
# stub mirrors JoiPlay. Arity is permissive via `*args` to
# accommodate plugins that pass additional metadata.
module Kernel
  def set_loop_points(*args); end
  module_function :set_loop_points
end

# --- Process spawning neutralization ---
# fork()/exec() are forbidden on iOS and cause immediate SIGKILL.
# Neutralize all process-spawning methods at the engine level.
module Kernel
  # Process-spawning methods are no-ops on iOS: fork/exec would be
  # killed by the sandbox and system("game.exe") only makes sense on
  # Windows. Return nil so games keep running. Real exec() would
  # terminate the process but that's the entire iOS app here, so a
  # silent no-op is the safer default.
  def system(*_args)
    nil
  end

  def exec(*_args)
    nil
  end

  def fork(*_args)
    nil
  end

  def spawn(*_args)
    nil
  end
  module_function :system, :exec, :fork, :spawn
end

# --- Windows environment variable stubs ---
# Many RGSS games use ENV["TEMP"] / ENV["APPDATA"] for file operations,
# and some derive save paths from USERPROFILE / LOCALAPPDATA /
# COMPUTERNAME. Mirror JoiPlay's full fake-Windows environment so
# scripts constructing paths from ENV don't return nil and crash.
# Values point into the iOS sandbox (or are blank strings) so File.exist?
# returns false rather than reading unrelated system dirs.
# iOS puts the app's own temp folder in TMPDIR. /tmp is outside the sandbox.
tmp = (ENV['TMPDIR'] || '/tmp').chomp('/')

# SDL_GetPrefPath contract: directory paths used for string-concat
# save joins must end with '/'. iOS normalize strips trailing slashes.
# The C++ System.data_directory binding re-adds one. Shared by the
# Ruby mirror (stale-merged.o safety) and ENV setup below.
module MkxpPath
  module_function

  def ensure_trailing_dir_sep(path)
    p = path.to_s
    return './' if p.empty? || p == '.'

    p.end_with?('/', '\\') ? p : "#{p}/"
  end
end

if defined?(System) && System.respond_to?(:data_directory) && !System.respond_to?(:_mkxp_orig_data_directory)
  class << System
    alias _mkxp_orig_data_directory data_directory

    def data_directory(*args)
      MkxpPath.ensure_trailing_dir_sep(_mkxp_orig_data_directory(*args))
    end
  end
end

# mkxp-z moved mouse_in_window from System to Input. Pokemon
# Essentials v20 and v20.1 still call the System name in
# Mouse.getMousePos, so every mouse read raised NoMethodError.
if defined?(System) && defined?(Input) &&
   Input.respond_to?(:mouse_in_window) && !System.respond_to?(:mouse_in_window)
  def System.mouse_in_window
    Input.mouse_in_window
  end
end

save_root = nil
begin
  if defined?(System) && System.respond_to?(:data_directory)
    root = System.data_directory.to_s
    save_root = root unless root.empty?
  end
rescue StandardError
  save_root = nil
end
# Fake Windows env vars → per-game save root (trailing slash required
# for `ENV['APPDATA'] + "Game.rxdata"`-style concat).
# Machine/user identity for the fake Windows env below: prefer the
# launcher identity the host declared ($userAgent), fall back to the
# engine's own name so nothing here is tied to a specific host app.
host_identity = defined?($userAgent) && $userAgent ? $userAgent.to_s : 'mkxp'
userdata = MkxpPath.ensure_trailing_dir_sep(save_root || "#{tmp}/UserData")
ENV['TEMP'] ||= tmp
ENV['TMP']  ||= tmp
ENV['APPDATA']              ||= userdata
ENV['LOCALAPPDATA']         ||= userdata
ENV['ALLUSERSPROFILE']      ||= userdata
ENV['USERPROFILE']          ||= userdata
ENV['HOMEDRIVE']            ||= ''
ENV['HOMEPATH']             ||= userdata
ENV['SystemRoot']           ||= userdata
ENV['windir']               ||= userdata
ENV['COMPUTERNAME']         ||= host_identity
ENV['USERNAME']             ||= host_identity
ENV['USERDOMAIN']           ||= host_identity
ENV['SESSIONNAME']          ||= host_identity
ENV['OS']                   ||= 'Windows_NT'
ENV['PATH']                 ||= ''
ENV['PATHEXT']              ||= ''
ENV['Platform']             ||= ''
ENV['NUMBER_OF_PROCESSORS'] ||= '4'
ENV['PROCESSOR_ARCHITECTURE'] ||= 'x86'
ENV['PROCESSOR_IDENTIFIER'] ||= 'Intel64 Family6'
ENV['PROCESSOR_LEVEL']      ||= '6'
ENV['PROCESSOR_REVISION']   ||= '2a07'
ENV['AV_APPDATA']           ||= userdata

# --- Float bitwise-op monkey-patches ---
# RGSS scripts occasionally do `x ^ 2` when they mean `x ** 2` (a
# Game-Maker-idiom leak) or `x << n` to cheaply multiply by 2**n. On
# stock Ruby these raise NoMethodError against Float. Adding the ops
# is a zero-risk unlock for a long tail of buggy scripts.
class Float
  def ^(other)
    self**other
  end

  def <<(num)
    self * (2**num)
  end

  def >>(other)
    self / (2**other)
  end
end

# --- Input::Controller state stubs ---
# Prevents NoMethodError crashes from games that probe gamepad
# state via a pad API the iOS port doesn't expose. Cited offender
# is Sometimes Always Monsters, which calls
# `Input::Controller.first_state.thumb_left_x` (and friends) at
# startup. Without these stubs the script terminates before the
# title screen. We are NOT implementing real gamepad support here
# - every method returns a zero / false / [] sentinel so probes
# succeed and the game falls through to keyboard / touch input.
#
# Guarded on `defined?(Input::Controller)` so a future engine-level
# Controller binding (or a game that ships its own) isn't clobbered.
# Source: JoiPlay mkxp/binding-mri/preload.rb:123-177.
unless defined?(Input::Controller)
  module Input
    module Controller
      class State
        def left_trigger_value
          0
        end

        def right_trigger_value
          0
        end

        def thumb_left_x
          0
        end

        def thumb_left_y
          0
        end

        def thumb_right_x
          0
        end

        def thumb_right_y
          0
        end

        def thumb_left_dir4
          0
        end

        def thumb_left_dir8
          0
        end

        def thumb_right_dir4
          0
        end

        def thumb_right_dir8
          0
        end

        def press?(_button)
          false
        end

        def trigger?(_button)
          false
        end

        def repeat?(_button)
          false
        end

        def pressed_buttons
          []
        end
      end

      def self.states
        [State.new]
      end

      def self.first_state
        State.new
      end
    end
  end
end

# --- Audio.se_play_position shim ---
# Pokemon Reborn's custom desktop mkxp-z fork extends Audio with
# spatially-positioned sound effects:
# `se_play_position(name, volume, pitch, x, y, z)`. Reborn's
# `internal_se_play` calls it on every SE when `$joiplay` is false,
# so without a shim the first message-confirm sound raises
# NoMethodError and soft-locks the scene. Our engine's SE path has
# no spatial support. Drop the coordinates and play the SE plain -
# identical to the game's own JoiPlay branch
# (`Audio.se_play(name, volume, pitch)`).
#
# Guarded so a future engine-native implementation (or a game's own
# monkey-patch loaded later via preload) wins over the shim.
if defined?(Audio) && Audio.respond_to?(:se_play) && !Audio.respond_to?(:se_play_position)
  module Audio
    def self.se_play_position(filename, volume = 100, pitch = 100, *_position)
      se_play(filename, volume, pitch)
    end
  end
end

# --- MKXP module shim ---
# Some game preload scripts expect the MKXP module from Ancurio's
# original mkxp. mkxp-z uses "System" module instead.
module MKXP
  def self.zinflate(string)
    Zlib::Inflate.inflate(string)
  end

  def self.zdeflate(string, level = Zlib::DEFAULT_COMPRESSION)
    Zlib::Deflate.deflate(string, level)
  end

  def self.data_directory(*args)
    System.data_directory(*args) if defined?(System)
  end

  def self.puts(*args)
    if defined?(System)
      System.puts(*args)
    else
      Kernel.puts(*args)
    end
  end

  def self.desensitize(path)
    System.desensitize(path) if defined?(System)
  end
end

# --- Win32 library null-stub via const_missing ---
# Win32-only library scripts (RGSS Linker, FMODEX, network loaders, etc.)
# reference constants that never get defined on iOS because DLL loading is a
# no-op (see win32_wrap.rb). Instead of adding per-library stubs, hook
# Module#const_missing so any undefined constant - top-level OR nested
# inside a partially-defined module like Berka::NetErrorErr - resolves to
# a safe stub rather than raising NameError.
#
# Two kinds of stubs are returned:
#
# 1. Constants whose name ends in Error, Err, Exception, or Failure become
#    real StandardError subclasses. This matters because games commonly
#    write `raise Berka::NetErrorErr, "msg"`. The raised exception must
#    inherit from Exception or Ruby rejects it, and if it is NullStub the
#    alert ends up showing "IOS::NullStub" as the error message.
#
# 2. Everything else becomes IOS::NullStub, which silently absorbs any
#    method call and any nested constant lookup. This covers library
#    namespaces like FmodEx, FmodEx::System, etc.
#
# NullStub is also raisable. Games sometimes raise a typo'd nested
# constant that only resolves on iOS (Daybreak's downloader does
# `raise Berka::NetErrorErr::ConIn`. `ConIn` has no error suffix, so
# it resolves to NullStub). What `raise <stub>` does depends on the
# VM:
#
# - Ruby 1.9+ coerces the argument through `to_str` first, so the
#   stub raises as a RuntimeError with an empty message. Ugly, but
#   not fatal.
# - Ruby 1.8 accepts only real Strings, then requires the argument
#   to respond to `exception`. method_missing does not satisfy that
#   check, so the script's own raise dies with a fatal
#   "exception class/object expected" TypeError.
# - The two-argument form `raise <stub>, msg` requires `exception`
#   on every VM.
#
# The explicit `exception` hook below makes the 1.8 and two-argument
# paths produce a plain StandardError with a readable message.
module IOS
  class NullStub
    # Raise protocol hook: `raise NullStub` calls exception() and
    # `raise NullStub, msg` calls exception(msg) on every VM we
    # target (1.8 / 1.9 / 3.x).
    def self.exception(message = nil)
      StandardError.new(message || 'a Win32-only feature is unavailable on this platform')
    end

    def self.method_missing(_name, *_args)
      self
    end

    # Marshal calls a dump hook when respond_to? says it exists. A save
    # that holds the stub then fails ("marshal_dump returned same
    # class instance"), and the game saves only part of the file. The
    # load hooks still answer, so an older save that holds a stub
    # object still loads.
    # rubocop:disable Style/SymbolArray -- `%i` does not parse on Ruby 1.8.
    MARSHAL_HOOKS = [:marshal_dump, :_dump].freeze
    # rubocop:enable Style/SymbolArray

    def self.respond_to_missing?(name, _include_private = false)
      !MARSHAL_HOOKS.include?(name.to_sym)
    end

    def self.const_missing(_name)
      self
    end

    def self.new(*_args)
      self
    end

    # to_s/to_str return empty string so `"prefix: #{stub}"` and any implicit
    # string coercion produce clean output instead of leaking the internal
    # class name or raising TypeError on Ruby 3.x strict coercion.
    def self.to_s
      ''
    end

    def self.to_str
      ''
    end

    def self.inspect
      '#<IOS::NullStub>'
    end

    # Instance-level twin of the raise protocol hook. NullStub.new
    # returns the class, so instances are rare - but a game can still
    # obtain one (e.g. via allocate) and raise it.
    def exception(message = nil)
      self.class.exception(message)
    end

    def method_missing(_name, *_args)
      nil
    end

    def respond_to_missing?(name, _include_private = false)
      !MARSHAL_HOOKS.include?(name.to_sym)
    end
  end

  # rubocop:disable Style/MutableConstant -- const_missing populates
  # this lazily via `ErrorStubs[key] ||= ...` (line ~437) so it can't
  # be frozen.
  ErrorStubs = {}
  # rubocop:enable Style/MutableConstant
  ERROR_SUFFIX_RE = /(?:Error|Err|Exception|Failure)\z/.freeze
  PB_DATA_TABLE_RE = /\APB[A-Z]/.freeze
  TOP_LEVEL_CONSTANT_RE = /^(?:(?:class|module)[ \t]+([A-Z]\w*)(?![\w:])|([A-Z]\w*)[ \t]*=(?![=~>]))/.freeze
  SECTION_FRAME_RE = /\A(?:(\d+):|Section(\d+):|\{(\d+)\}:)/.freeze

  # Name => index of the first section that declares it at column 0.
  # The VM outlives a game, so the table follows $RGSS_SCRIPTS.
  def self.game_constant_sections
    scripts = $RGSS_SCRIPTS
    return @game_constant_sections if @game_constant_sections && @game_constant_sections_for.equal?(scripts)

    @game_constant_sections_for = scripts
    @game_constant_sections = {}
    return @game_constant_sections unless scripts.is_a?(Array)

    scripts.each_with_index do |script, index|
      code = script[3] if script.is_a?(Array)
      next unless code.is_a?(String)

      code = code.dup.force_encoding('BINARY') if code.respond_to?(:force_encoding)
      code.scan(TOP_LEVEL_CONSTANT_RE) do |declared, assigned|
        @game_constant_sections[declared || assigned] ||= index
      end
    end
    @game_constant_sections
  end

  # The section that the engine evals now: the outermost frame named
  # like one. Nil outside the sections, for example in a postload.
  def self.running_section
    index = nil
    caller.each do |frame|
      match = SECTION_FRAME_RE.match(frame)
      index = (match[1] || match[2] || match[3]).to_i if match
    end
    index
  end

  # Windows raises NameError for a game constant whose section has not
  # run yet, and games rescue it to take a fallback (Pokemon GS Vitro
  # picks the language before its language scene class loads). Once
  # the section ran, a missing constant means the section failed on
  # iOS, and the stub stands in for it.
  def self.game_constant_not_loaded_yet?(name)
    sections = game_constant_sections
    section = sections[name.to_s]
    return false unless section

    running = running_section
    return false unless running
    return true if section > running

    sections.delete(name.to_s)
    false
  end
end

class Module
  # `super` does not work in a redefined Module#const_missing (there
  # is no superclass implementation), so keep the original around for
  # the passthrough paths. It raises the stock NameError.
  alias _mkxp_orig_const_missing const_missing unless method_defined?(:_mkxp_orig_const_missing)

  def const_missing(name)
    return _mkxp_orig_const_missing(name) unless Object.const_defined?(:IOS)
    return _mkxp_orig_const_missing(name) if ::IOS.game_constant_not_loaded_yet?(name)

    # Pokemon Essentials keeps its data tables in PB* classes
    # (PBItems, PBSpecies, PBMoves, ...). Games probe these tables
    # for optional entries with `const_get(...) rescue nil` and treat
    # the NameError as the "entry is absent" signal. A stub does not
    # raise, so the probe would hand a Class to game code that
    # expects an Integer id. Empyrean does exactly this for optional
    # *_CARD items and then crashes on `card < 2000`. No Win32
    # library lives in a PB* namespace, so let these lookups raise
    # as they do on Windows.
    return _mkxp_orig_const_missing(name) if self.name.to_s =~ ::IOS::PB_DATA_TABLE_RE

    if name.to_s =~ ::IOS::ERROR_SUFFIX_RE
      key = [self, name]
      ::IOS::ErrorStubs[key] ||= begin
        klass = Class.new(StandardError)
        const_set(name, klass)
        klass
      end
    else
      ::IOS::NullStub
    end
  end
end

# --- Log the value that stops a save ---
# Essentials' pbSave rescues every error and returns false, so a save
# that fails leaves no trace. When
# Marshal.dump raises, find the value that Marshal could not write and
# put its path in the debug log.
unless defined?(MKXPMarshalTrace)
  module MKXPMarshalTrace
    # Some scripts call Marshal.dump in a rescue on purpose, to test
    # a value. Stop after this many lines in one game.
    MAX_REPORTS = 10
    MAX_VALUES = 200_000
    FILE_PREFIX = "#{__FILE__}:".freeze

    @reports = 0

    # The search runs while the game's own error is in flight. An
    # error here must not replace it.
    # rubocop:disable Lint/RescueException
    class << self
      def reset
        @reports = 0
      end

      def report(obj, args, error)
        return if @reports >= MAX_REPORTS
        return unless logging?

        @reports += 1
        log("Marshal.dump failed: #{error.class}: #{error.message}. #{explain(obj, args)}")
      rescue Exception
        nil
      end

      # Without this, a crash alert names this file, not the game script
      # that called Marshal.dump.
      def trim_backtrace(error)
        trace = error.backtrace
        return unless trace

        i = 0
        i += 1 while trace[i] && trace[i][0, FILE_PREFIX.length] == FILE_PREFIX
        error.set_backtrace(trace[i..-1]) if i > 0 && i < trace.length
      rescue Exception
        nil
      end

      def logging?
        return true unless defined?(System) && System.respond_to?(:debug_log?)

        System.debug_log?
      end

      def log(line)
        if defined?(System) && System.respond_to?(:puts)
          System.puts(line)
        else
          warn(line)
        end
      end

      def explain(obj, args)
        found = locate(obj)
        return "The value holds more than #{MAX_VALUES} objects, too many to search." if found == :too_big
        return "First value that cannot be dumped: #{found[0]} = #{describe(found[1])}" if found

        limit = args.find { |a| a.is_a?(Integer) }
        if limit && limit >= 0 && !dumps?(obj, limit)
          return "The value is nested deeper than the depth limit of #{limit}."
        end

        return 'Could not find which value fails.' unless dumps?(obj)

        stream = args.any? { |a| !a.is_a?(Integer) }
        return 'A second dump to memory worked, so the write to the stream failed.' if stream

        'A second dump worked, so the error did not repeat.'
      end

      def dumps?(obj, limit = -1)
        Marshal.mkxp_untraced_dump(obj, limit)
        true
      rescue Exception
        false
      end

      # Walks the value in the order Marshal writes it and skips values
      # already walked. Marshal writes those as links, so a link back to
      # a parent never fails, and the first value that fails on its own
      # is the one Marshal stopped at.
      def locate(root)
        @shell_cache = {}
        @budget = MAX_VALUES
        catch(:mkxp_too_big) { walk(root) }
      end

      # Returns [path, value], or nil when every value dumps.
      def walk(root)
        seen = {}
        stack = [[root, describe_root(root)]]
        until stack.empty?
          obj, path = stack.pop
          next if seen[obj.__id__]

          seen[obj.__id__] = true
          kids = children(obj)
          return [path, obj] unless kids

          kids.reverse_each do |label, child|
            stack.push([child, "#{path}#{label}"]) unless seen[child.__id__]
          end
        end
        nil
      end

      # The values Marshal writes inside obj, or nil when obj itself
      # cannot be written.
      def immediate?(obj)
        case obj
        when nil, true, false, Integer, Float, Symbol then true
        else false
        end
      rescue Exception
        false
      end

      # This calls a game's own marshal_dump or _dump once more. That is
      # the only way to find a hook that fails.
      def children(obj)
        return opaque_children(obj) if opaque?(obj)
        return [['.marshal_dump', marshal_data(obj)]] if obj.respond_to?(:marshal_dump, true)
        return nil unless shell_dumps?(obj) || dumps?(obj)

        own_children(obj)
      rescue Exception
        nil
      end

      # Marshal writes these whole, so their insides are not walked.
      def opaque?(obj)
        # rubocop:disable Style/CaseEquality -- a BasicObject has no is_a?.
        return true unless Object === obj
        # rubocop:enable Style/CaseEquality

        obj.is_a?(Module) || obj.respond_to?(:_dump, true)
      end

      def opaque_children(obj)
        dumps?(obj) ? [] : nil
      end

      def own_children(obj)
        list = []
        case obj
        when Array
          obj.each_with_index { |v, i| add_child(list, v) { "[#{i}]" } }
        when Hash
          obj.each_pair do |k, v|
            add_child(list, k) { " key #{describe(k)}" }
            add_child(list, v) { "[#{describe(k)}]" }
          end
          add_child(list, obj.default) { '.default' }
        when Struct
          obj.each_pair { |k, v| add_child(list, v) { ".#{k}" } }
        end
        obj.instance_variables.each do |name|
          add_child(list, obj.instance_variable_get(name)) { name.to_s }
        end
        list
      end

      # A plain value always dumps, so it is left out of the search.
      def add_child(list, value)
        return if immediate?(value)

        @budget -= 1
        throw(:mkxp_too_big, :too_big) if @budget < 0
        list.push([yield, value])
      end

      def marshal_data(obj)
        data = obj.__send__(:marshal_dump)
        raise TypeError if data.instance_of?(obj.class)

        data
      end

      # Can Marshal write obj with every value inside it left out? A
      # Proc, an IO, a singleton, an anonymous class, or a hash with a
      # default proc cannot.
      def shell_dumps?(obj)
        return false unless obj.singleton_methods(false).empty?
        return false if obj.is_a?(Hash) && obj.default_proc

        klass = obj.class
        @shell_cache[klass] = dumps?(klass.allocate) unless @shell_cache.key?(klass)
        @shell_cache[klass]
      rescue Exception
        false
      end

      def describe_root(obj)
        obj.is_a?(Module) ? 'the value' : obj.class.name.to_s
      rescue Exception
        'the value'
      end

      def describe(obj)
        # rubocop:disable Style/CaseEquality -- a BasicObject has no is_a?.
        return 'a BasicObject' unless Object === obj
        # rubocop:enable Style/CaseEquality

        case obj
        when Module then "#{obj.inspect} (#{with_article(obj.class.name.to_s)})"
        when nil, true, false, Numeric, Symbol then obj.inspect
        when String then obj.length > 40 ? "#{obj[0, 40].inspect}..." : obj.inspect
        else with_article(obj.class.name.to_s)
        end
      rescue Exception
        'a value that cannot be shown'
      end

      def with_article(name)
        return 'a value of a class with no name' if name.empty?

        name =~ /\A[AEIOU]/ ? "an #{name}" : "a #{name}"
      end
    end
    # rubocop:enable Lint/RescueException
  end

  class << Marshal
    alias mkxp_untraced_dump dump

    # A game's _dump hook and the search both call Marshal.dump again.
    # Only the outer call reports, so one failed save gives one line.
    def dump(obj, *args)
      return mkxp_untraced_dump(obj, *args) if Thread.current[:mkxp_marshal_dump]

      Thread.current[:mkxp_marshal_dump] = true
      begin
        mkxp_untraced_dump(obj, *args)
      rescue StandardError => e
        MKXPMarshalTrace.report(obj, args, e)
        MKXPMarshalTrace.trim_backtrace(e)
        raise e
      rescue Exception => e # rubocop:disable Lint/RescueException -- a stack overflow needs the trim too.
        MKXPMarshalTrace.trim_backtrace(e)
        raise e
      ensure
        Thread.current[:mkxp_marshal_dump] = nil
      end
    end
  end
end
# The Ruby VM outlives a game, and each game loads this file again.
MKXPMarshalTrace.reset

# --- DL / DL::CFunc legacy fake module ---
# Older Pokemon Essentials forks and a few community plugins use
# Ruby 1.8's `require 'dl'` + `DL::CFunc` to call user32 / kernel32
# functions directly (pre-dating Win32API). On Ruby 3 the stdlib
# `dl` gem doesn't exist and our `const_missing` hook only returns
# `IOS::NullStub`, which is not hash-indexable. Scripts doing
# `dll = DL.dlopen('user32'); dll['GetSystemMetrics']` therefore
# fail on `Hash#[]` even though the constant itself resolves.
#
# Ported from JoiPlay's preload.rb. `dlopen` returns a populated
# hash so the lookup succeeds. `CFunc.new` stores the function
# name and delegates `call` to Win32API (which our win32_wrap
# already routes to noop / safe-default returns on iOS).
module DL
  class CFunc
    def initialize(func, type = 'i')
      @func_name = func.to_s
      @type = type
      @impl = begin
        Win32API.new('User32', @func_name, %w[l p], 'i') if defined?(Win32API)
      rescue StandardError
        nil
      end
    end

    def call(*args)
      return @impl.call(*args) if @impl

      0
    end

    def to_s
      @func_name.to_s
    end

    def to_str
      @func_name.to_s
    end
  end

  USER32_FUNCS = %w[
    GetActiveWindow GetSystemMetrics GetWindowRect SetWindowLong
    SetWindowPos FindWindow GetForegroundWindow GetCursorPos
    SetWindowText
  ].freeze

  KERNEL32_FUNCS = %w[
    GetModuleHandle GetPrivateProfileString GetCurrentThreadId
    GetCurrentProcess SetPriorityClass
  ].freeze

  def self.dlopen(lib = '')
    name = lib.to_s.downcase
    table = case name
            when /user32/   then USER32_FUNCS
            when /kernel32/ then KERNEL32_FUNCS
            else []
            end
    h = Hash.new { |_, k| k.to_s } # unknown keys echo their own name
    table.each { |fn| h[fn] = fn }
    h
  end

  # Hime's full keyboard script (LonaRPG) runs
  # `DL::CPtr.new(DL.malloc(256), 256)` and passes `ptr.to_i` to
  # GetKeyboardState.
  def self.malloc(size)
    Win32API_Impl::FakeHeap.alloc("\0" * size.to_i)
  end

  class CPtr
    def initialize(address, size = 0)
      @address = address.to_i
      @bytes = Win32API_Impl::FakeHeap.block(@address) || ("\0" * size.to_i)
    end

    def to_i
      @address
    end

    # Windows DL reads a signed char.
    def [](index)
      byte = (defined?(Encoding) ? @bytes.getbyte(index) : @bytes[index]) || 0
      byte >= 0x80 ? byte - 0x100 : byte
    end

    def []=(index, value)
      byte = value.to_i & 0xff
      if defined?(Encoding)
        @bytes.setbyte(index, byte)
      else
        @bytes[index] = byte
      end
    end
  end
end

# --- Socket / network stubs ---
# Our embedded Ruby doesn't compile the network stdlib (socket,
# net/http, net/https, openssl, uri) and games can't install user
# gems (discord-rpc, poke-api-v2, rest-client). Without help a
# single `require 'socket'` at the top of a bootstrap script
# raises LoadError and terminates the whole eval, so Reborn and
# similar games don't even reach the title screen. We solve this
# in two layers:
#
# 1. Provide minimal no-op classes for socket primitives (below)
#    so scripts that do `TCPSocket.open(...)` don't NameError
#    later on.
# 2. Intercept Kernel#require with an allowlist of known-missing
#    network stdlib + gems. If the require matches the allowlist
#    and the original require raises LoadError, we swallow it and
#    return false (mimicking "already loaded") so the calling
#    script continues. Non-network requires still propagate.
#
# TODO: compile Ruby with network stdlib so online features
# work. See TODO.md "Engine / compatibility".

# --- rbconfig fallback ---
# Our embedded Ruby doesn't ship `rbconfig` (it's generated per-arch
# at Ruby build time, so it never lands in the static ext set).
# Desktop-targeting games hit it indirectly - e.g. Pokemon Reborn's
# bundled rubyzip does `require 'rbconfig'` on the non-JoiPlay path
# and reads CONFIG['host_os'] to pick Windows vs POSIX path
# handling. Reborn ships its own stdlib copies of rbconfig.rb but
# only pushes the arch subdir (stdlib/x64-mingw32 etc.) onto the
# load path for platforms it knows about, so on iOS the require
# fails and the whole script eval aborts.
#
# When `require 'rbconfig'` raises LoadError, install a minimal
# darwin-flavored RbConfig instead - iOS is closest to macOS (POSIX
# paths, case-insensitive FS), so callers branch away from the
# Windows-specific path handling that would corrupt our paths.
# Installing a real module matters: merely swallowing the require
# would leave `RbConfig::CONFIG['host_os']` to the const_missing
# NullStub, whose method_missing chain is always-truthy and would
# match ANY `=~ /mswin|mingw/` probe as Windows.
#
# Lazy (hooked into the require rescue below, not pre-defined) so a
# game that gets a real rbconfig.rb onto the load path still loads
# the genuine article without constant-redefinition noise.
module MKXPRbConfigFallback
  module_function

  # Unknown keys resolve to '' (not nil) so string ops on unprobed
  # CONFIG entries don't raise.
  def config_values
    parts = RUBY_VERSION.split('.')
    config = Hash.new { |_hash, _key| '' }
    config.update(
      'MAJOR' => parts[0],
      'MINOR' => parts[1],
      'TEENY' => parts[2],
      'ruby_version' => "#{parts[0]}.#{parts[1]}.0",
      'RUBY_PROGRAM_VERSION' => RUBY_VERSION,
      'host_os' => 'darwin',
      'target_os' => 'darwin',
      'host_cpu' => 'arm64',
      'target_cpu' => 'arm64',
      'arch' => 'arm64-darwin',
      'sitearch' => 'arm64-darwin',
      'host' => 'arm64-apple-darwin',
      'ruby_install_name' => 'ruby',
      'RUBY_INSTALL_NAME' => 'ruby',
      'RUBY_SO_NAME' => 'ruby',
      'EXEEXT' => '',
      'DLEXT' => 'bundle',
      'SOEXT' => 'dylib',
      'PATH_SEPARATOR' => ':'
    )
  end

  def install
    return if Object.const_defined?(:RbConfig)

    config = config_values
    mod = Module.new
    mod.const_set(:CONFIG, config)
    mod.const_set(:MAKEFILE_CONFIG, config)
    mod.const_set(:TOPDIR, nil)
    mod.const_set(:DESTDIR, '')
    def mod.ruby
      'ruby'
    end

    def mod.expand(val, _config = nil)
      val
    end
    Object.const_set(:RbConfig, mod)
    System.puts '[platform_compat] rbconfig fallback installed' if defined?(System)
    nil
  end
end

module Kernel
  # Network stdlib. When the host enables network access these
  # requires resolve for real: on modern Rubies against the bundled
  # pure-Ruby stdlib + static socket/openssl exts, on 1.8/1.9 partly
  # via the Net::HTTP facade in `net_http_compat.rb`. A require that
  # still fails (a stdlib piece we didn't ship) is absorbed exactly
  # like in offline mode - games historically survive the resulting
  # NameError through their own rescues, and a shipping gap must not
  # crash a game that used to boot - but it is logged loudly so the
  # gap can be reported and closed.
  #
  # Match by exact path or by prefix so `net/http`, `net/https`,
  # `net/http/status`, etc. are all absorbed by a single `net/`
  # entry.
  NETWORK_STDLIB_PATHS = [
    'socket', 'resolv', 'resolv-replace',
    'openssl', 'digest',
    'uri', 'ipaddr', 'open-uri',
    'net', 'net/'
  ].freeze

  # Gems that desktop games bundle but that can't exist here (no
  # user gems, no dlopen). Genuinely absent regardless of the
  # network toggle, so always absorbed.
  MISSING_GEM_PATHS = %w[
    httparty rest-client rest_client
    discord discord-rpc discordrb
    poke-api-v2 pokeapi
    websocket websocket-client
    json-jwt jwt
  ].freeze

  # Back-compat: older patches/scripts referenced the combined list.
  NETWORK_REQUIRE_PATHS = (NETWORK_STDLIB_PATHS + MISSING_GEM_PATHS).freeze

  orig_require = instance_method(:require)

  # rubocop:disable Metrics/CyclomaticComplexity, Metrics/PerceivedComplexity
  # -- the interceptor enumerates the historical absorb rules in one place
  define_method(:require) do |path|
    begin
      orig_require.bind(self).call(path)
    rescue LoadError => e
      str = path.to_s

      # rbconfig gets a real fallback module (see
      # MKXPRbConfigFallback above), not a swallow: callers read
      # RbConfig::CONFIG values right after requiring.
      if ['rbconfig', 'rbconfig.rb'].include?(str)
        MKXPRbConfigFallback.install
        feature = 'rbconfig.rb'
        $LOADED_FEATURES << feature unless $LOADED_FEATURES.include?(feature)
        next true
      end

      match = lambda do |list|
        list.any? do |entry|
          entry.end_with?('/') ? str.start_with?(entry) : str == entry
        end
      end

      matched_stdlib = match.call(NETWORK_STDLIB_PATHS)
      raise e unless matched_stdlib || match.call(MISSING_GEM_PATHS)

      # With networking enabled these requires should have resolved
      # against the bundled stdlib/shims. Absorbing one means we
      # failed to ship something the game wants. Keep the game alive
      # (as in offline mode) but say so loudly.
      if matched_stdlib &&
         defined?(System) && System.respond_to?(:network_enabled?) &&
         System.network_enabled? && defined?(MKXP) && MKXP.respond_to?(:puts)
        MKXP.puts("[platform_compat] network stdlib require '#{str}' failed " \
                  "despite networking being enabled: #{e.message}")
      end

      # Mark as loaded so future `require` calls short-circuit.
      feature = str.end_with?('.rb') ? str : "#{str}.rb"
      $LOADED_FEATURES << feature unless $LOADED_FEATURES.include?(feature)
      false
    end
  end
  # rubocop:enable Metrics/CyclomaticComplexity, Metrics/PerceivedComplexity
end

# --- Socket class stubs ---
# Removed. Pokemon Essentials forks (Insurgence, Reborn) ship
# their own Sockets script that defines TCPSocket / UDPSocket /
# BasicSocket with their own class hierarchy.
#
# A stub here never survives those scripts, and the way it fails
# depends on the Ruby version. On 1.9 and 3.1 the game's definition
# raises "superclass mismatch", and the script loop skips the rest
# of that section. On 1.8 nothing raises at all, because
# ruby18/rgss-superclass-override.patch restores the RGSS1 rule
# that the last definition wins. The stub is then replaced in
# silence. A stub helps in neither case.
#
# Games that need network stubs must ship their own. With
# networking enabled the real socket classes come from the
# statically-linked ext instead (gated in extinit so offline mode
# still matches this old behavior). Games that call Winsock
# through Win32API reach the network through Win32API_Impl::Ws2_32
# in win32_wrap.rb.

# --- Eager static extension load ---
# The socket and openssl extensions are statically linked into the
# Ruby 3.1 VM. They initialize lazily, on the first `require`. Some
# games replace `Kernel#require` with a loader that searches the load
# path for plain .rb files and evals them (Rejuvenation's
# ScriptLoader does this). Such a loader cannot start a static
# extension: `require 'socket.so'` finds no file and returns false.
# The stdlib wrapper then evals into hollow classes, and the C-only
# constants (IPSocket, the OpenSSL internals) never exist. The first
# stdlib file that touches one crashes. Example: ipaddr.rb sees no
# Socket::AF_INET6, takes its no-IPv6 branch, and its
# `class << IPSocket` block raises a NameError inside the game's
# update flow.
#
# Load both extensions here, through the real `require`, before any
# game code runs. The real `require` records the absolute stdlib
# paths in $LOADED_FEATURES. Replacement loaders honor that list, so
# they skip these files and the real classes stay in place. The
# eager load does not conflict with the stub-removal note above: the
# 1.8/1.9 VMs do not link the extensions, and the version gate skips
# them, so no class appears there. The gate reads System.ruby_version
# (the C API version), not RUBY_VERSION, because the syntax-transform
# layer can mimic an old RUBY_VERSION on the 3.1 VM. When networking
# is off, the airplane-mode section below still patches the connect
# surface.
if defined?(System) && System.respond_to?(:ruby_version) &&
   System.ruby_version.to_f >= 3.1
  begin
    require 'socket'
    require 'openssl'
  rescue StandardError
    nil
  end
end

# --- TLS trust store protection ---
# Desktop-targeting games routinely do
#   ENV['SSL_CERT_FILE'] = 'cacert.pem'
# pointing at a bundle shipped next to Game.exe (Rejuvenation's
# ScriptLoader does exactly this). The host already exports a
# working SSL_CERT_FILE for Ruby's openssl. Letting a game point it
# at a file that doesn't exist in the iOS import silently breaks
# every TLS handshake with "unable to get local issuer
# certificate". Honor the game's assignment when the file is really
# there (game-relative paths resolve against the game dir, our
# cwd), otherwise keep the host trust store.
class << ENV
  unless method_defined?(:__mkxp_orig_env_set)
    alias __mkxp_orig_env_set []=

    def []=(key, value)
      if %w[SSL_CERT_FILE SSL_CERT_DIR].include?(key.to_s) &&
         value && !File.exist?(value.to_s)
        if defined?(MKXP) && MKXP.respond_to?(:puts)
          MKXP.puts("[platform_compat] ignoring ENV['#{key}'] = " \
                    "#{value.inspect}: file missing; keeping host trust store")
        end
        return
      end
      __mkxp_orig_env_set(key, value)
    end

    alias store []=
  end
end

# --- Airplane-mode socket blocking ---
# With network access toggled off the game must see the equivalent
# of airplane mode: libraries load, classes exist, connections fail.
# The native HTTP client refuses on its own, but Ruby 3.1's real
# socket classes are statically compiled in and would happily reach
# the network. Patch the connection-making surface to raise
# ENETDOWN - the exact errno airplane mode produces - so raw-socket
# code takes the same rescue paths it takes on a device with no
# connectivity. Local binds/listens are left alone (they work in
# airplane mode too). On the 1.8/1.9 VMs the socket classes aren't
# registered while offline, so these guards simply never match.
network_off = defined?(System) &&
              System.respond_to?(:network_enabled?) &&
              !System.network_enabled?
if network_off
  # The eager static extension load above already initialized the
  # socket classes on the VMs that carry them. On the 1.8/1.9 VMs
  # the classes stay absent and the guards below are no-ops.
  if defined?(TCPSocket)
    class << TCPSocket
      def open(*_args)
        raise Errno::ENETDOWN
      end
      alias new open
    end
  end

  if defined?(Socket)
    class Socket
      def connect(*_args)
        raise Errno::ENETDOWN
      end

      def connect_nonblock(*_args)
        raise Errno::ENETDOWN
      end
    end

    class << Socket
      def tcp(*_args)
        raise Errno::ENETDOWN
      end
    end
  end

  if defined?(UDPSocket)
    class UDPSocket
      def send(*_args)
        raise Errno::ENETDOWN
      end

      def connect(*_args)
        raise Errno::ENETDOWN
      end
    end
  end

  # Datagram and DNS surfaces that skip `connect`. A bare
  # `Socket.new(:INET, :DGRAM)` can emit via `send`/`sendmsg` with
  # an explicit destination, and resolver calls reach the network
  # on their own - all of it must go dark with the toggle too.
  # `BasicSocket#send` stays usable for connected sockets (two
  # args, no destination): those already got ENETDOWN at connect.
  if defined?(BasicSocket)
    class BasicSocket
      # Alias, never `super`: a replaced #send falling through to
      # the superclass lands on Object#send - Ruby MESSAGE
      # dispatch - which would treat the payload as a method name.
      alias _mkxp_orig_socket_send send

      def send(*args)
        raise Errno::ENETDOWN if args.length >= 3

        _mkxp_orig_socket_send(*args)
      end

      def sendmsg(*_args)
        raise Errno::ENETDOWN
      end

      def sendmsg_nonblock(*_args)
        raise Errno::ENETDOWN
      end
    end
  end

  if defined?(Socket)
    class << Socket
      def udp_server_sockets(*_args)
        raise Errno::ENETDOWN
      end

      def getaddrinfo(*_args)
        raise Errno::ENETDOWN
      end
    end
  end

  if defined?(Addrinfo)
    class << Addrinfo
      def getaddrinfo(*_args)
        raise Errno::ENETDOWN
      end
    end
  end

  if defined?(IPSocket)
    class << IPSocket
      def getaddress(*_args)
        raise Errno::ENETDOWN
      end
    end
  end
end
