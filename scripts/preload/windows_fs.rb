# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# windows_fs.rb
# Windows filesystem-semantics emulation layer.
# Auto-loaded before platform_compat.rb and game scripts. Games
# authored on Windows lean on OS behavior the device filesystem does
# not give them: case-insensitive paths, text-mode reads, a system
# font directory, and save files next to the executable. The modules
# and File / FileTest / Dir wrappers here rebuild those semantics.
# Engine-generic runtime shims live in platform_compat.rb, and
# Essentials-specific patches live in pokemon_compat.rb.

# Windows guarantees a system font directory, and games write into
# it without a mkdir: the stock Essentials FontInstaller copies its
# font files to "<SystemRoot>\Fonts\" and reads them back on the
# next launch as its "already installed" check. The faked
# SystemRoot points at the per-game data root, so map that one
# Windows-only shape onto a real "Fonts" folder there - for reads
# and writes alike, or the installed-check never passes. Every
# other path keeps MKXPSaveFS's literal resolution.
unless defined?(MKXPWindowsFonts)
  module MKXPWindowsFonts
    module_function

    # The host-wide shared font pool when the host declares one
    # (System.shared_fonts_path). The per-game fallback otherwise.
    # The shared pool matches Windows exactly: the system font
    # folder is one store for every game, so a font one game
    # installs satisfies the next game's installed-check too.
    def pool_root(savefs)
      shared = shared_root
      return shared if shared

      base = savefs.root
      base && "#{base}/Fonts"
    end

    def shared_root
      return @shared_root if defined?(@shared_root)

      @shared_root =
        if defined?(System) && System.respond_to?(:shared_fonts_path)
          path = System.shared_fonts_path.to_s
          path.empty? ? nil : path.gsub(%r{[\\/]+\z}, '')
        end
    end

    def target(savefs, path)
      return nil unless path.is_a?(String)

      base = savefs.root
      return nil unless base
      return nil unless path[0, base.length] == base

      rest = path[base.length..-1].to_s
      match = rest.match(%r{\A[\\/]+Fonts[\\/]+([^\\/]+)\z}i)
      return nil unless match

      pool = pool_root(savefs)
      pool && "#{pool}/#{match[1]}"
    end

    # The write half of the parity: the directory always exists on
    # Windows, so it must exist here before the first copy lands.
    def ensure_dir(savefs, target)
      dir = pool_root(savefs)
      return unless dir && target.is_a?(String)
      return unless target[0, dir.length + 1] == "#{dir}/"
      return if File._mkxp_orig_directory(dir)

      Dir.mkdir(dir)
    rescue StandardError
      nil
    end
  end
end

unless defined?(MKXPSaveFS)
  # rubocop:disable Metrics/ModuleLength -- the module is the single
  # file-API front. The alias-era recovery block below pushes it
  # over the limit and leaves once no install carries stranded
  # saves.
  module MKXPSaveFS
    module_function

    # Everything inside the game folder is literal. Games create,
    # list, write, and delete their own files with no interception;
    # UserData is involved only when a game addresses it itself
    # (System.data_directory, faked env vars). The constants below
    # serve one purpose: the alias-era migration
    # (migrate_portable_saves!) that moves saves stranded by earlier
    # builds into the portable folder those games read. They gate no
    # other runtime behavior and can go once no install carries
    # stranded saves.
    PORTABLE_SAVE_DIR = 'Save Data'.freeze
    PORTABLE_PREFIX = "#{PORTABLE_SAVE_DIR}/".freeze
    ENGINE_DIR_ENTRY_RE = /\Akeybindings\.mkxp\d+\z/.freeze
    SAFE_WRITE_ENTRY_RE = /\A.+\.mkxp-(?:tmp|restore)\z/.freeze
    # Rooted path: "/...", "\...", or a Windows drive prefix.
    ABSOLUTE_PATH_RE = %r{\A(?:[A-Za-z]:[\\/]|[\\/])}.freeze

    def root
      return @mkxp_save_root_memo if defined?(@mkxp_save_root_memo) && @mkxp_save_root_memo

      return nil unless defined?(System) && System.respond_to?(:data_directory)

      dir = System.data_directory.to_s
      return nil if dir.empty?

      @mkxp_save_root_memo = dir.gsub(%r{[\\/]+\z}, '')
    rescue StandardError
      nil
    end

    def normalize_path(path)
      path.strip.gsub('\\', '/')
    end

    def save_filename?(name)
      return false if host_artifact?(name)
      return false if pre_literal_artifact?(name)

      lower = name.downcase
      return true if lower =~ /\A(?:save\d+|game)\.(?:rxdata|rvdata|rvdata2)\z/
      return true if lower.end_with?('.rxdata', '.rvdata', '.rvdata2')
      return true if lower.end_with?('.bak')

      false
    end

    # Host bookkeeping files carry a ".<identity>-" name segment,
    # where <identity> is the launcher identity the host declared
    # ($userAgent), for example "*.<identity>-displaced.bak". They
    # belong to the host, not to the game. No recovery may move them.
    def host_artifact?(name)
      mark = host_artifact_mark
      !mark.nil? && name.include?(mark)
    end

    def host_artifact_mark
      return @host_artifact_mark if defined?(@host_artifact_mark)

      identity = defined?($userAgent) && $userAgent ? $userAgent.to_s : ''
      @host_artifact_mark = identity.empty? ? nil : ".#{identity}-"
    end

    # Collision backups this migration wrote itself
    # ("*.pre-literal.bak", "*.pre-literal-2.bak"). They must never
    # count as migration candidates again: re-migrating a backup
    # chains another suffix onto it on every enumeration.
    def pre_literal_artifact?(name)
      name.include?('.pre-literal')
    end

    # True when the two paths name the same directory entry, by
    # device+inode identity. A string comparison cannot decide
    # this: on iOS the host config spells the data dir /var/...
    # while getcwd resolves the symlink to /private/var/..., and
    # File.expand_path resolves no symlinks. The old string
    # comparison stays as the fallback for paths the identity
    # check cannot stat.
    def same_directory?(a, b)
      File.identical?(a, b)
    rescue StandardError
      File.expand_path(a) == File.expand_path(b)
    end

    # Identity check for migration source/destination pairs. On
    # error, prefer "not identical": the migration then runs its
    # normal collision path, which never destroys data.
    def same_entry?(a, b)
      File.identical?(a, b)
    rescue StandardError
      false
    end

    def engine_internal_entry?(name)
      return false unless name.is_a?(String)

      name =~ ENGINE_DIR_ENTRY_RE || safe_write_entry?(name)
    end

    def safe_write_entry?(path)
      path.is_a?(String) && File.basename(path) =~ SAFE_WRITE_ENTRY_RE
    end

    def filter_dir_entries(entries)
      return entries unless entries.respond_to?(:reject)

      entries.reject { |entry| engine_internal_entry?(entry) }
    end

    def filter_safe_write_entries(entries)
      return entries unless entries.respond_to?(:reject)

      entries.reject { |entry| safe_write_entry?(entry) }
    end

    def list_entries(target, entries)
      save_root_target?(target) ? filter_dir_entries(entries) : filter_safe_write_entries(entries)
    end

    # Bare working-directory save filenames - the shape decade-old
    # Essentials builds use, and the only shape the legacy-save
    # recovery below reacts to.
    def candidate?(path)
      return false unless path.is_a?(String)

      stripped = normalize_path(path)
      return false if stripped.empty?
      return false if stripped.start_with?('/', '~')
      return false if stripped =~ /\A[A-Za-z]:/
      return false if stripped.include?('/')

      save_filename?(stripped)
    end

    def orig_exist?(path)
      return FileTest._mkxp_orig_exist(path) if defined?(FileTest) && FileTest.respond_to?(:_mkxp_orig_exist)
      return File._mkxp_orig_exist(path) if File.respond_to?(:_mkxp_orig_exist)

      false
    rescue StandardError
      false
    end

    # Every path resolves literally - a game's writes inside its own
    # folder land exactly where Windows would put them. This helper
    # stays as the single file-API front for two recovery side
    # effects left from the alias era:
    #  - Earlier builds redirected bare working-directory save
    #    filenames ("Game.rxdata") into UserData. The first access
    #    of such a name moves the stranded copy back into the
    #    working directory. On a collision the newer mtime keeps
    #    the canonical name and the loser stays as
    #    *.pre-literal.bak.
    #  - The same builds flattened portable "Save Data/..." paths
    #    into the UserData root. A qualifying access under the
    #    portable dir runs the one-time sweep (see the
    #    portable-save migration section below).
    # Modern games address UserData saves by absolute path, match
    # neither gate, and keep their files where they are. Delete both
    # recoveries once no install carries stranded saves.
    def path_for(path)
      maybe_recover_portable_path(path)
      recover_legacy_save(path)
      MKXPWindowsFonts.target(self, path) || path
    end

    def recover_legacy_save(path)
      return unless path.is_a?(String)

      normalized = normalize_path(path)
      return unless candidate?(normalized)

      base = recovery_base
      return unless base

      stranded = "#{base}/#{normalized}"
      return unless File._mkxp_orig_file(stranded)

      migrate_save_file(stranded, normalized)
    rescue StandardError
      nil
    end

    # The UserData root both alias-era recoveries may take files
    # FROM, or nil when no recovery may run. Two gates, shared by
    # the single-file and the glob recovery:
    #  - The redirect the recoveries undo only ever happened with
    #    the game folder as the cwd, so any other cwd skips them.
    #    Essentials' Dir.get chdirs INTO the data dir to enumerate
    #    saves. Without this gate the recovery would "migrate"
    #    those saves onto themselves.
    #  - Degenerate host config: the data dir IS the game folder.
    #    Compared by identity, not by spelling - on iOS the same
    #    directory arrives as /var/... and resolves from getcwd as
    #    /private/var/....
    def recovery_base
      base = root
      return nil unless base
      return nil unless cwd_at_game_root?
      return nil if same_directory?(base, '.')

      base
    end

    # Bare glob patterns enumerate the working directory (ancient
    # slot pickers use "Save*.rxdata"). Recover every stranded root
    # save the pattern would match before the literal glob runs, so
    # enumeration and open agree on one folder. Gated per name on the
    # save shape so a bare "*" cannot drag engine-internal UserData
    # files into the game folder.
    def recover_saves_for_glob(pattern)
      patterns = pattern.is_a?(Array) ? pattern : [pattern]
      patterns.each do |pat|
        next unless pat.is_a?(String)

        # A pattern inside the portable dir is an enumeration of it.
        maybe_recover_portable_dir(pat)

        normalized = normalize_path(pat)
        next if normalized.empty? || normalized.include?('/')

        recover_glob_matches(normalized)
      end
      nil
    rescue StandardError
      nil
    end

    def recover_glob_matches(normalized)
      base = recovery_base
      return unless base

      Dir._mkxp_orig_entries(base).each do |name|
        next unless save_filename?(name)
        next unless File.fnmatch(normalized, name)
        next unless File._mkxp_orig_file("#{base}/#{name}")

        migrate_save_file("#{base}/#{name}", name)
      end
    rescue StandardError
      nil
    end

    # iOS's filesystem is case-sensitive. Windows-authored games
    # open, probe, and delete files with mismatched case and expect
    # it to work. The machinery below resolves the actual on-disk
    # spelling. It consumes exactly three engine facts, grouped in
    # this adapter so the test harness can swap them for device
    # emulation on a case-folding host.
    module EngineFS
      module_function

      # The engine keeps the pre-casefold exist? under
      # `_mkxp_native_orig_exist?`. The `_mkxp_orig_*` aliases
      # capture the casefold-aware replacement, which reports true
      # for any spelling and would defeat a strict probe.
      def strict_exist?(path)
        if File.respond_to?(:_mkxp_native_orig_exist?)
          File._mkxp_native_orig_exist?(path)
        else
          File._mkxp_orig_exist(path)
        end
      end

      # The engine's boot-time case cache. Nil when the engine has
      # no answer, and nil for every query when a game ships
      # "pathCache": false in its mkxp.json.
      def cache_resolve(rel)
        return nil unless defined?(System) && System.respond_to?(:resolve_case_path)

        System.resolve_case_path(rel)
      rescue StandardError
        nil
      end

      def dir_entries(dir)
        Dir._mkxp_orig_entries(dir)
      end
    end

    def engine_fs
      @engine_fs || EngineFS
    end

    def engine_fs=(adapter)
      @engine_fs = adapter
      @walk_cache = nil
    end

    # Windows-authored games write over their own files with
    # mismatched case ("Battle Open.wav" over "Battle Open.WAV" -
    # Rejuvenation's updater extracts patches this way) and delete or
    # rename them the same way. On Windows both spellings are one
    # file. Here a raw create makes a duplicate on the device and
    # fails with Errno::EEXIST under the simulator's case-sensitivity
    # emulation, and a raw delete or rename misses the file. Resolve
    # the on-disk spelling first, so destructive file APIs hit the
    # file the game means.
    def resolve_case_target(path)
      str = path.to_s
      return path if str.empty?
      return path if raw_exist?(str)

      case_variant(str) || path
    rescue StandardError
      path
    end

    # Case-folding front for the probe wrappers (exist?, file?,
    # directory? on File, FileTest, and Dir). The engine's own
    # probes casefold through its boot path cache, but a game can
    # ship "pathCache": false (Rejuvenation's Windows build does),
    # and the cache also cannot see files this session created.
    # Windows folds case in the OS, so a probe with a mismatched
    # spelling must still find the file. Rejuvenation's updater
    # guards its delete-old-spelling step with such a probe, and a
    # false miss there aborts the whole patch later. The block is
    # the wrapper's own original probe. The retry runs that same
    # probe on the resolved on-disk spelling, so file? and
    # directory? keep their type semantics and a bare variant of a
    # missing file still answers false.
    def probe_casefold(path)
      target = path_for(path)
      return true if yield(target)

      variant = begin
        case_variant(target.to_s)
      rescue StandardError
        nil
      end
      variant ? yield(variant) : false
    end

    # Strict spelling probe through the engine adapter.
    def raw_exist?(str)
      engine_fs.strict_exist?(str)
    end

    # On-disk spelling for a mismatched-case path, or nil when the
    # spelling already matches or nothing resolves. Absolute paths
    # resolve when they point inside the game directory or inside
    # UserData. The engine's boot-time case cache answers first. A
    # live directory walk then covers what the cache cannot know -
    # files the game created this session (a self-updater extracts
    # files and touches them again under another spelling).
    def case_variant(str)
      prefix, rel = split_case_prefix(str)
      return nil if rel.nil?

      fixed = nil
      # The cache is game-root-relative: valid for absolute paths
      # under the game root, and for relative paths only while the
      # working directory IS the game root.
      fixed = live_cache_hit(rel, prefix) if prefix == game_root_prefix || (prefix.nil? && cwd_at_game_root?)
      if fixed.nil?
        walked = live_case_walk(rel, prefix ? prefix[0, prefix.length - 1] : '.')
        fixed = walked unless walked == rel
      end
      return nil unless fixed

      prefix ? "#{prefix}#{fixed}" : fixed
    end

    # [prefix, rel] split of a path against the known roots. prefix
    # is nil for relative paths. Both are nil when an absolute path
    # points outside every known root.
    def split_case_prefix(str)
      return [nil, str] unless str =~ ABSOLUTE_PATH_RE

      [game_root_prefix, userdata_prefix].each do |base|
        next unless base
        next unless str.length > base.length && str[0, base.length] == base

        return [base, str[base.length..-1]]
      end
      [nil, nil]
    end

    def userdata_prefix
      base = root
      base ? "#{base}/" : nil
    end

    def cwd_at_game_root?
      "#{File.expand_path('.')}/" == game_root_prefix
    rescue StandardError
      true
    end

    # Boot-cache lookup that survives same-session deletions. The
    # cache cannot see a deletion: a self-updater deletes a file to
    # change its spelling, and a stale hit would send the
    # replacement write back to the deleted name (Rejuvenation's
    # patcher aborted on the follow-up chmod this way). Trust a hit
    # only while that spelling is still on disk.
    def live_cache_hit(rel, prefix)
      return nil if rel.empty? || rel =~ ABSOLUTE_PATH_RE

      fixed = engine_fs.cache_resolve(rel)
      return nil if fixed.nil? || fixed == rel

      on_disk = prefix ? "#{prefix}#{fixed}" : fixed
      raw_exist?(on_disk) ? fixed : nil
    rescue StandardError
      nil
    end

    # Case-insensitive component walk against the live filesystem,
    # anchored at base. A component with an on-disk case variant
    # takes that spelling. A component with no match stays literal
    # (it is about to be created). Returns the input unchanged when
    # nothing differs.
    def live_case_walk(rel, base)
      current = base
      rel.split(%r{[\\/]+}).map do |part|
        spelled = live_component_spelling(current, part)
        current = "#{current}/#{spelled}"
        spelled
      end.join('/')
    end

    def live_component_spelling(dir, part)
      return part if part.empty? || part == '.' || part == '..'

      names, lowered = walk_entries(dir)
      return part if names.key?(part)

      lowered[part.downcase] || part
    rescue StandardError
      part
    end

    # Directory listings for the live walk, memoized per filesystem
    # generation. Probe misses are hot in Essentials games (sprite
    # lookups probe several extensions per frame), and an unmemoized
    # walk re-lists the same directories on every miss. Every
    # mutating wrapper bumps the generation before it resolves, and
    # the cache stays off during that resolution, so a listing never
    # outlives the disk state it described.
    def walk_entries(dir)
      return entry_index(engine_fs.dir_entries(dir)) if @walk_cache_off

      cache = (@walk_cache ||= {})
      unless cache[:generation] == fs_generation
        cache.clear
        cache[:generation] = fs_generation
      end
      cache[dir] ||= entry_index(engine_fs.dir_entries(dir))
    end

    # A miss in a folder with thousands of files lowercased every name,
    # and the garbage made Ruby 1.8 collect about twice a second.
    def entry_index(entries)
      names = {}
      lowered = {}
      entries.each do |name|
        names[name] = true
        lowered[name.downcase] ||= name
      end
      [names, lowered]
    end

    def fs_generation
      @fs_generation || 0
    end

    def bump_fs_generation!
      @fs_generation = fs_generation + 1
    end

    # Spelling resolution for a mutating operation (open for write,
    # delete, rename, mkdir, rmdir). The generation bump invalidates
    # every earlier listing, and the walk reads the live filesystem
    # during this resolution, so a listing of a directory the
    # operation is about to change never gets cached.
    def mutating_resolution
      bump_fs_generation!
      @walk_cache_off = true
      begin
        yield
      ensure
        @walk_cache_off = false
      end
    end

    def change_spelling(target)
      mutating_resolution { resolve_case_target(target) }
    end

    def change_target(path)
      change_spelling(path_for(path))
    end

    # The engine starts every session in the game directory, and this
    # file loads before any game script can chdir away. The captured
    # spelling anchors absolute-path case resolution to the game root
    # even when a game changes the working directory later. The
    # capture call sits at the end of this file, outside the module
    # guard, so test harnesses that reload the file refresh it too.
    def capture_game_root!
      @mkxp_game_root_prefix = "#{File.expand_path('.')}/"
    end

    def game_root_prefix
      @mkxp_game_root_prefix
    end

    # True when an open mode creates, truncates, appends, or opens
    # for update - every case where the open must land on the
    # existing on-disk spelling instead of creating a duplicate.
    # Encoding suffixes ("r:windows-1252") stay out of the check, and
    # a keyword-style {mode: ...} hash contributes its :mode value.
    def write_mode?(mode)
      mode = mode[:mode] if mode.is_a?(Hash) && mode.key?(:mode)
      case mode
      when Integer
        (mode & (File::WRONLY | File::RDWR | File::APPEND | File::CREAT)) != 0
      when String, Symbol
        mode.to_s.split(':', 2)[0] =~ /[wa+]/ ? true : false
      else
        false
      end
    end

    # Shared File.open / File.new front: legacy-save recovery first,
    # then the write-mode case resolution.
    # Windows refuses to open a folder with EACCES. POSIX opens it and
    # the first read raises EISDIR, which games do not rescue (Pokémon
    # TGOM probes "Graphics/Characters/" for an empty sprite name).
    # An empty path names the working directory to the safe write,
    # which then raises EISDIR. Windows raises ENOENT.
    def open_target(path, mode)
      raise Errno::ENOENT, path if path == ''

      target = refuse_directory(path_for(path))
      return target unless write_mode?(mode)

      MKXPWindowsFonts.ensure_dir(self, target)
      change_spelling(target)
    end

    # Ruby 1.8.1 to 1.8.6 of RGSS1 and RGSS2 give the path to the C
    # library with no NUL check, so the path ends at the first NUL.
    # Ruby 1.8.7 and later raise ArgumentError in File and IO.
    def cut_at_nul(path)
      return path unless RUBY_VERSION < '1.9' && path.is_a?(String)

      nul = path.index("\0")
      nul ? path[0, nul] : path
    end

    def refuse_directory(target)
      raise Errno::EACCES, target if target.is_a?(String) && File.directory?(target)

      target
    end

    # Errno::ENOENT retry target for the open wrappers. The same
    # resolution the write side uses, so absolute in-game paths and
    # session-created files resolve for reads too. A path with a
    # stray "\r" gets a second retry with those stripped: Windows
    # forbids control characters in file names, so the "\r" can only
    # come from CRLF text split with binary reads (Desolation's
    # script loader parses its CSV listing this way). Windows' own
    # text-mode reads drop the "\r" before the path is built.
    def read_fallback_target(path)
      str = path.to_s
      fixed = case_variant(str)
      return path_for(fixed) if fixed

      return nil unless str.include?("\r")

      cleaned = str.delete("\r")
      return path_for(cleaned) if raw_exist?(cleaned)

      fixed = case_variant(cleaned)
      fixed ? path_for(fixed) : nil
    rescue StandardError
      nil
    end

    # True when a resolved directory target IS the per-game UserData
    # root - the game using System.data_directory (trailing separator
    # included) or a faked env var pointing at it. Callers use this
    # to (a) filter engine-internal entries out of listings of the
    # root and (b) no-op mkdir/rmdir of the root: the host owns that
    # directory, games must never remove it, and "create my data
    # folder" is always already satisfied.
    def save_root_target?(target)
      base = root
      return false unless base && target.is_a?(String)

      target.gsub(%r{[\\/]+\z}, '') == base
    end

    # --- Portable-save migration (virtual alias -> literal) ---
    # Earlier builds virtualized the portable save dir: portable
    # games wrote "Save Data/..." and the shim flattened it into the
    # UserData root. "Save Data" is literal now, so affected
    # installs have their portable content stranded at that root.
    # The sweep below moves it back into the folder the game reads.
    #
    # The sweep is access-driven. No boot-time gate can know whether
    # a game resolves saves through the portable dir. Rejuvenation-
    # lineage builds go portable on the launcher identity alone
    # (RTP.isPortable -> mobile? -> the launcher global, such as $kirin), with no marker
    # file and no JoiPlay toggle. And a $joiplay-keyed boot sweep
    # would move root saves for games that never read the portable
    # dir. So the wrappers watch for the game itself to touch
    # portable save content. The first qualifying access runs one
    # sweep per boot:
    #  - an enumeration of the portable dir (the Reborn-lineage
    #    load screens list it), or
    #  - a file access under "Save Data/" that misses literally
    #    while the flattened counterpart exists at the root.
    # A bare probe of a missing marker never qualifies. A
    # non-portable game that only checks "Save Data/.portable"
    # keeps its root saves untouched.
    #
    # The sweep moves every stranded entry, subdirectories included
    # (the alias flattened "Save Data/Battle Logs/..." the same
    # way). Engine bookkeeping entries and host ".<identity>-" artifacts
    # stay at the root. On a name collision the newer mtime wins
    # the canonical name (ties go to the root copy, which the alias
    # preferred for both reads and writes). The loser is kept
    # beside it as *.pre-literal.bak.
    # Paths resolve against the engine cwd (the game folder, set by
    # main.cpp/config.cpp before the binding boots), the same base
    # the game's own "Save Data/" strings resolve against. The cwd
    # guard skips the sweep while a game has chdir'd elsewhere.
    def maybe_recover_portable_path(path)
      return if @portable_sweep_done
      return unless path.is_a?(String) && path.start_with?('Save')

      normalized = normalize_path(path)
      return unless normalized.start_with?(PORTABLE_PREFIX)

      rel = normalized[PORTABLE_PREFIX.length..-1]
      return if rel.nil? || rel.empty?
      return if orig_exist?(normalized)

      base = sweep_base
      return unless base && orig_exist?("#{base}/#{rel}")

      run_portable_sweep!
    rescue StandardError
      nil
    end

    def maybe_recover_portable_dir(path)
      return if @portable_sweep_done
      return unless path.is_a?(String) && path.start_with?('Save')

      normalized = normalize_path(path).gsub(%r{/+\z}, '')
      return unless normalized == PORTABLE_SAVE_DIR || normalized.start_with?(PORTABLE_PREFIX)

      run_portable_sweep!
    rescue StandardError
      nil
    end

    def run_portable_sweep!
      return if @portable_sweep_done
      return unless cwd_at_game_root?

      @portable_sweep_done = true
      migrate_portable_saves!
    end

    # The boot section at the end of this file re-arms the sweep on
    # every load, so test harness reloads start clean.
    def arm_portable_sweep!
      @portable_sweep_done = false
    end

    # UserData root for sweeps, or nil when the host config is
    # degenerate (no data dir, or the data dir IS the game folder -
    # a sweep there would shuffle shipped root saves into Save
    # Data).
    def sweep_base
      base = root
      return nil if base.nil? || base == '.'
      return nil if same_directory?(base, '.')

      base
    end

    def migrate_portable_saves!
      base = sweep_base
      return unless base

      names = begin
        Dir._mkxp_orig_entries(base)
      rescue StandardError
        nil
      end
      return if names.nil?

      movable = names.reject { |name| skip_portable_entry?(name) }
      return if movable.empty?

      Dir._mkxp_orig_mkdir(PORTABLE_SAVE_DIR) unless orig_exist?(PORTABLE_SAVE_DIR)
      movable.each do |name|
        migrate_save_entry("#{base}/#{name}", "#{PORTABLE_PREFIX}#{name}")
      end
    rescue StandardError
      nil
    end

    def skip_portable_entry?(name)
      return true if ['.', '..'].include?(name)
      return true if name == '.DS_Store'
      return true if engine_internal_entry?(name)

      host_artifact?(name)
    end

    def migrate_save_entry(src, dst)
      if File._mkxp_orig_file(src)
        migrate_save_file(src, dst)
      elsif File._mkxp_orig_directory(src)
        migrate_save_tree(src, dst)
      end
    rescue StandardError
      nil
    end

    # Directory form of the migration. A whole stranded tree renames
    # over when the destination is free. Otherwise merge entry by
    # entry, recursing into shared subdirectories. Per-file
    # collisions follow the migrate_save_file mtime rule. The source
    # directory goes away only when everything inside it moved.
    def migrate_save_tree(src, dst)
      return File._mkxp_orig_rename(src, dst) unless orig_exist?(dst)
      return unless File._mkxp_orig_directory(dst)

      Dir._mkxp_orig_entries(src).each do |name|
        next if ['.', '..'].include?(name)

        migrate_save_entry("#{src}/#{name}", "#{dst}/#{name}")
      end
      Dir._mkxp_orig_rmdir(src) if Dir._mkxp_orig_entries(src).size <= 2
    rescue StandardError
      nil
    end

    def migrate_save_file(src, dst)
      # The same inode reached through two spellings must never
      # "migrate": the rename pair below would strip the file down
      # to its backup name and leave nothing at the canonical one.
      return if same_entry?(src, dst)

      unless orig_exist?(dst)
        File._mkxp_orig_rename(src, dst)
        return
      end

      backup = pre_literal_backup_name(dst)
      if migrate_source_newer?(src, dst)
        File._mkxp_orig_rename(dst, backup)
        File._mkxp_orig_rename(src, dst)
      else
        File._mkxp_orig_rename(src, backup)
      end
    rescue StandardError
      nil
    end

    # First free backup name beside dst. rename(2) replaces an
    # existing target silently, so a fixed backup name would
    # destroy the previous backup on a second collision.
    def pre_literal_backup_name(dst)
      name = "#{dst}.pre-literal.bak"
      return name unless orig_exist?(name)

      index = 2
      index += 1 while orig_exist?("#{dst}.pre-literal-#{index}.bak")
      "#{dst}.pre-literal-#{index}.bak"
    end

    # Ties go to the root (src) copy: on the first alias->literal
    # boot equal stamps mean copies of the same file, and the alias
    # treated the root as authoritative.
    def migrate_source_newer?(src, dst)
      File._mkxp_orig_mtime(src) >= File._mkxp_orig_mtime(dst)
    rescue StandardError
      true
    end
  end
  # rubocop:enable Metrics/ModuleLength
end

# --- Windows text-mode read emulation ---
# Windows Ruby opens files in text mode by default and folds CRLF to
# LF on read. Games depend on that: Desolation's script loader splits
# its CSV listing on "\n" and derives file paths and control flow
# from the fields, and a kept "\r" breaks both. Pokemon Pathways ships
# the RGSS Script Editor loader, which splits a CRLF load_order.txt on
# "\n" and then rejects every entry because File.extname sees ".rb\r".
# Fold the same way for plain read-mode opens on the modern VM. Binary
# opens ('b' flag, integer flags), update modes, write modes, and calls
# with explicit extra arguments stay raw.
unless defined?(MKXPTextMode)
  module MKXPTextMode
    module_function

    # File.read and its friends take no mode argument, so Windows
    # applies text mode to every one of these calls. Fold the bare
    # form only: a call that gives a length, a separator, or its own
    # options keeps the arguments it was given.
    def whole_file_args(args)
      opts = universal_newline_opts
      return args unless opts && args.empty?

      [opts]
    end

    def read_args(args)
      opts = universal_newline_opts
      return args unless opts
      return ['r', opts] if args.empty?
      return args unless args.length == 1

      mode = args[0]
      return args unless mode.is_a?(String)

      flags = mode.split(':', 2)[0]
      return args unless %w[r rt].include?(flags)

      [mode, opts]
    end

    # The option hash carries the ruby2_keywords flag so the splat
    # in the File wrappers passes it as keywords.
    def universal_newline_opts
      return @universal_newline_opts if defined?(@universal_newline_opts)

      capable = defined?(System) && System.respond_to?(:ruby_version) &&
                System.ruby_version.to_f >= 3.1 &&
                Hash.respond_to?(:ruby2_keywords_hash)
      @universal_newline_opts =
        capable ? Hash.ruby2_keywords_hash({ :newline => :universal }) : nil
    end
  end
end

# A save that stops half way, from a crash or from a value that
# Marshal cannot dump, leaves a cut-short file, and the next load
# fails with "end of file reached". A File.open that truncates and
# takes a block writes to a temp file next to the target. The
# target changes only when the block ends without an exception.
unless defined?(MKXPSafeWrite)
  # Ruby 1.9 gives a killed thread back its old status while its
  # ensure blocks run, so a kill leaves a mark on the thread there.
  # Thread#[] is per fiber, so the mark is an instance variable.
  # Later Rubies can defer a kill with Thread.handle_interrupt, and a
  # mark would then drop a save that ends normally.
  if RUBY_VERSION.start_with?('1.9')
    class Thread
      alias _mkxp_orig_kill kill unless method_defined?(:_mkxp_orig_kill)

      def kill
        @mkxp_killed = true
        _mkxp_orig_kill
      end
      alias exit kill
      alias terminate kill

      class << self
        alias _mkxp_orig_kill kill unless method_defined?(:_mkxp_orig_kill)
        alias _mkxp_orig_exit exit unless method_defined?(:_mkxp_orig_exit)

        def kill(thread)
          thread.instance_variable_set(:@mkxp_killed, true)
          _mkxp_orig_kill(thread)
        end

        def exit
          Thread.current.instance_variable_set(:@mkxp_killed, true)
          _mkxp_orig_exit
        end
      end
    end
  end

  module MKXPSafeWrite
    MODE_BITS = 0xFFF
    CHUNK = 65_536
    # Most file systems stop at 255 bytes in one name.
    NAME_MAX = 255 - '.mkxp-restore'.length
    LOCK = defined?(Mutex) ? Mutex.new : nil

    module_function

    def use?(target, args)
      return false unless target.is_a?(String) && truncate_mode?(args[0])
      return false if args.any? { |arg| arg.is_a?(Hash) && arg.key?(:flags) }

      safe_place?(target, args[0])
    end

    def safe_place?(target, mode)
      return false if File.basename(target).bytesize > NAME_MAX

      dir = File.dirname(target)
      return false unless File._mkxp_orig_directory(dir) && File.writable?(dir)
      return false if File._mkxp_orig_directory(temp_path(target))
      return false if File._mkxp_orig_directory(temp_path(target, 'restore'))
      return false if File.symlink?(target)
      return create_mode?(mode) unless File._mkxp_orig_exist(target)

      File._mkxp_orig_file(target) && File.writable?(target) && File.stat(target).nlink == 1
    end

    def truncate_mode?(mode)
      mode = mode[:mode] if mode.is_a?(Hash) && mode.key?(:mode)
      case mode
      when Integer
        (mode & File::TRUNC) != 0 && (mode & File::EXCL).zero?
      when String
        flags = mode.split(':', 2)[0].to_s
        flags.start_with?('w') && !flags.include?('x')
      else
        false
      end
    end

    def flags(mode)
      mode = mode[:mode] if mode.is_a?(Hash) && mode.key?(:mode)
      mode if mode.is_a?(Integer)
    end

    def create_mode?(mode)
      bits = flags(mode)
      bits.nil? || (bits & File::CREAT) != 0
    end

    # Integer flags without File::CREAT open the target only if it
    # exists. The temp file never exists yet.
    def temp_args(args)
      bits = flags(args[0])
      return args if bits.nil? || (bits & File::CREAT) != 0

      args = args.dup
      args[0] = args[0].is_a?(Hash) ? keywords(args[0].merge(:mode => bits | File::CREAT)) : bits | File::CREAT
      args
    end

    # Ruby 3 reads a splat Hash as keywords only with this flag, and
    # Hash#merge drops it.
    def keywords(hash)
      Hash.respond_to?(:ruby2_keywords_hash) ? Hash.ruby2_keywords_hash(hash) : hash
    end

    def temp_path(target, kind = 'tmp')
      # The iOS Files app hides a name that starts with a dot. The
      # player must see a file that a crash leaves.
      File.join(File.dirname(target), "#{File.basename(target)}.mkxp-#{kind}")
    end

    def remove(path)
      File._mkxp_orig_delete(path) if File._mkxp_orig_exist(path)
    end

    # File.expand_path reads a leading "~" as a home folder.
    def absolute(path)
      File.expand_path(path.start_with?('/') ? path : "./#{path}")
    end

    # On iOS, /var is a link to /private/var. Two names of one folder
    # must give one key.
    def key(path)
      full = absolute(path)
      dir = File.dirname(full)
      dir = File.realpath(dir) if File.respond_to?(:realpath)
      File.join(dir, File.basename(full)).downcase
    rescue SystemCallError
      full.downcase
    end

    # A write that stops in a Fiber that never resumes keeps its claim.
    # Later saves of that file then write directly.
    def claim(name)
      locked do
        @active ||= {}
        taken = @active[name]
        @active[name] = true
        !taken
      end
    rescue ThreadError
      # A Mutex refuses to lock inside a Signal.trap handler.
      false
    end

    def release(name)
      locked { @active.delete(name) }
    end

    # Ruby 1.8 has no Mutex until a script requires 'thread'.
    def killed?
      thread = Thread.current
      thread.status == 'aborting' || thread.instance_variable_get(:@mkxp_killed) ? true : false
    end

    def locked
      if LOCK
        # Ruby 1.9 defines Mutex#synchronize in the thread library,
        # which games do not load.
        LOCK.lock
        begin
          return yield
        ensure
          LOCK.unlock
        end
      end
      return yield unless Thread.respond_to?(:critical=)

      begin
        Thread.critical = true
        yield
      ensure
        Thread.critical = false
      end
    end

    # Pokemon Insurgence renames Game.rxdata to Save_0_Backup_1.rxdata
    # and then writes a new Game.rxdata. Without the old file at the
    # target name, a failed write leaves no save to continue from.
    def note_rename(from, to)
      @renamed ||= {}
      @renamed[key(from)] = [absolute(to), File.stat(to).ino]
    rescue StandardError
      nil
    end

    def keep_renamed_file(target)
      renamed = @renamed
      @renamed = nil
      to, ino = renamed && renamed[key(target)]
      return unless to
      return if File._mkxp_orig_exist(target)
      return unless File._mkxp_orig_file(to) && File.stat(to).ino == ino

      copy = temp_path(target, 'restore')
      remove(copy)
      File._mkxp_orig_open(to, 'rb') do |src|
        File._mkxp_orig_open(copy, 'wb') do |dst|
          while (chunk = src.read(CHUNK))
            dst.write(chunk)
          end
          finish(dst, copy)
        end
      end
      File.chmod(File.stat(to).mode & MODE_BITS, copy)
      File._mkxp_orig_rename(copy, target)
    rescue StandardError
      begin
        remove(copy) if copy
      rescue StandardError
        nil
      end
    end

    def finish(file, path)
      if file.closed?
        # A game can close the file itself inside the block.
        File._mkxp_orig_open(path, 'rb') { |again| sync(again) }
      else
        file.flush
        sync(file)
      end
    end

    # Some file systems refuse fsync. A full or failing disk must
    # still fail the save, so the old file stays.
    NO_FSYNC = [Errno::EINVAL, NotImplementedError].concat(
      %w[ENOTSUP EOPNOTSUPP].select { |name| Errno.const_defined?(name) }.map { |name| Errno.const_get(name) }
    ).freeze

    def sync(file)
      file.fsync
    rescue *NO_FSYNC
      nil
    end

    def finish_rename(temp, target)
      File._mkxp_orig_rename(temp, target)
    rescue Errno::ENOENT
      # The block deleted the folder. A direct write loses the file too.
      raise if File._mkxp_orig_directory(File.dirname(target))
    end

    # A Pathname names a file. An open File also has to_path.
    def name_of(target)
      return target if target.is_a?(String)
      return nil if target.is_a?(IO) || !target.respond_to?(:to_path)

      target.to_path
    end

    # A game that renames file.path inside the block moves the old file.
    # The new data still lands at the name it opened. Windows refuses to
    # rename an open file, so games made for it do not do this.
    def show_target_path(file, target)
      file.instance_variable_set(:@mkxp_target_path, target)
      class << file
        def path
          @mkxp_target_path
        end

        def to_path
          @mkxp_target_path
        end
      end
    end

    def open(asked, args, &block)
      # A Dir.chdir inside the block must not move the final rename.
      target = absolute(asked)
      # The block can delete the folder, and the key of a path through
      # a link then changes.
      name = key(target)
      return File._mkxp_orig_open(asked, *args, &block) unless claim(name)

      begin
        write(target, asked, args, &block)
      ensure
        release(name)
      end
    end

    def write(target, asked, args)
      keep_renamed_file(target)
      temp = temp_path(target)
      remove(temp)
      remove(temp_path(target, 'restore'))
      mode = File._mkxp_orig_exist(target) ? File.stat(target).mode & MODE_BITS : nil
      failed = false
      begin
        File._mkxp_orig_open(temp, *temp_args(args)) do |file|
          File.chmod(mode, temp) if mode
          show_target_path(file, asked)
          result = yield file
          finish(file, temp)
          result
        end
      rescue Exception # rubocop:disable Lint/RescueException -- an Interrupt or an exit must not replace the save either
        failed = true
        raise
      ensure
        # A throw, a break, or a return in the block raises nothing.
        # The new file then replaces the target, as a direct write does.
        begin
          # Thread#kill and an exit from another thread raise nothing
          # in this thread.
          if failed || MKXPSafeWrite.killed?
            remove(temp)
          else
            finish_rename(temp, target)
          end
        ensure
          # A case lookup inside the block can list the folder before
          # the target is there.
          MKXPSaveFS.bump_fs_generation!
        end
      end
    end
  end
end

# --- File API front: legacy-save recovery + case resolution ---
# Paths resolve literally. path_for only performs the one-time
# legacy-save recovery side effect. The case helpers keep destructive
# operations on the on-disk spelling. IO.read gets no save-path
# handling. No observed game reads saves through it.
class << File
  alias _mkxp_orig_open open unless method_defined?(:_mkxp_orig_open)
  alias _mkxp_orig_delete delete unless method_defined?(:_mkxp_orig_delete)
  alias _mkxp_orig_rename rename unless method_defined?(:_mkxp_orig_rename)
  alias _mkxp_orig_new new unless method_defined?(:_mkxp_orig_new)
  alias _mkxp_orig_exist exist? unless method_defined?(:_mkxp_orig_exist)
  alias _mkxp_orig_exists exists? unless method_defined?(:_mkxp_orig_exists)
  alias _mkxp_orig_file file? unless method_defined?(:_mkxp_orig_file)
  alias _mkxp_orig_directory directory? unless method_defined?(:_mkxp_orig_directory)
  alias _mkxp_orig_size size unless method_defined?(:_mkxp_orig_size)
  alias _mkxp_orig_size? size? unless method_defined?(:_mkxp_orig_size?)
  alias _mkxp_orig_zero? zero? unless method_defined?(:_mkxp_orig_zero?)
  alias _mkxp_orig_mtime mtime unless method_defined?(:_mkxp_orig_mtime)

  def open(path, *args)
    path = MKXPSaveFS.cut_at_nul(path)
    args = MKXPTextMode.read_args(args)
    target = MKXPSaveFS.open_target(path, args[0])
    started = false
    retried = false
    begin
      return _mkxp_orig_open(target, *args) unless block_given?

      name = MKXPSafeWrite.name_of(target)
      if MKXPSafeWrite.use?(name, args)
        MKXPSafeWrite.open(name, args) do |file|
          started = true
          yield file
        end
      else
        _mkxp_orig_open(target, *args) do |file|
          started = true
          yield file
        end
      end
    rescue Errno::ENOENT
      # An Errno::ENOENT from the game code in the block must not run
      # the block a second time.
      raise if started || retried

      fixed = MKXPSaveFS.read_fallback_target(path)
      raise unless fixed

      target = MKXPSaveFS.refuse_directory(fixed)
      retried = true
      retry
    end
  end

  def new(path, *args)
    path = MKXPSaveFS.cut_at_nul(path)
    args = MKXPTextMode.read_args(args)
    _mkxp_orig_new(MKXPSaveFS.open_target(path, args[0]), *args)
  rescue Errno::ENOENT
    fixed = MKXPSaveFS.read_fallback_target(path)
    raise unless fixed

    _mkxp_orig_new(MKXPSaveFS.refuse_directory(fixed), *args)
  end

  def delete(*paths)
    _mkxp_orig_delete(*paths.map { |path| MKXPSaveFS.change_target(path) })
  end
  # File.unlink is a distinct singleton method. Without this alias it
  # would bypass the legacy-save recovery and the case resolution.
  alias unlink delete

  def rename(from, to)
    from = MKXPSaveFS.change_target(from)
    to = MKXPSaveFS.change_target(to)
    result = _mkxp_orig_rename(from, to)
    MKXPSafeWrite.note_rename(from, to)
    result
  end

  def exist?(path)
    MKXPSaveFS.probe_casefold(path) { |target| _mkxp_orig_exist(target) }
  end

  def exists?(path)
    MKXPSaveFS.probe_casefold(path) { |target| _mkxp_orig_exists(target) }
  end

  def file?(path)
    MKXPSaveFS.probe_casefold(path) { |target| _mkxp_orig_file(target) }
  end

  def directory?(path)
    MKXPSaveFS.probe_casefold(path) { |target| _mkxp_orig_directory(target) }
  end

  def size(path)
    _mkxp_orig_size(MKXPSaveFS.path_for(path))
  end

  def size?(path)
    _mkxp_orig_size?(MKXPSaveFS.path_for(path))
  end

  def zero?(path)
    _mkxp_orig_zero?(MKXPSaveFS.path_for(path))
  end

  def mtime(path)
    _mkxp_orig_mtime(MKXPSaveFS.path_for(path))
  end

  # Whole-file read/write helpers must resolve bare save names the
  # same way File.open does, or a game's File.read of a save sees a
  # different copy than its File.open (guard/act disagreement).
  # binread/binwrite/write are guarded: absent on Ruby 1.8.
  alias _mkxp_orig_read read unless method_defined?(:_mkxp_orig_read)

  def read(path, *args)
    path = MKXPSaveFS.cut_at_nul(path)
    _mkxp_orig_read(MKXPSaveFS.path_for(path), *MKXPTextMode.whole_file_args(args))
  end

  alias _mkxp_orig_readlines readlines unless method_defined?(:_mkxp_orig_readlines)

  def readlines(path, *args)
    path = MKXPSaveFS.cut_at_nul(path)
    _mkxp_orig_readlines(MKXPSaveFS.path_for(path), *MKXPTextMode.whole_file_args(args))
  end

  alias _mkxp_orig_foreach foreach unless method_defined?(:_mkxp_orig_foreach)

  def foreach(path, *args, &block)
    path = MKXPSaveFS.cut_at_nul(path)
    _mkxp_orig_foreach(MKXPSaveFS.path_for(path), *MKXPTextMode.whole_file_args(args), &block)
  end

  if (method_defined?(:binread) || private_method_defined?(:binread)) && !method_defined?(:_mkxp_orig_binread)
    alias _mkxp_orig_binread binread

    def binread(path, *args)
      _mkxp_orig_binread(MKXPSaveFS.path_for(path), *args)
    end
  end

  if (method_defined?(:write) || private_method_defined?(:write)) && !method_defined?(:_mkxp_orig_write)
    alias _mkxp_orig_write write

    def write(path, *args)
      _mkxp_orig_write(MKXPSaveFS.change_target(path), *args)
    end
  end

  if (method_defined?(:binwrite) || private_method_defined?(:binwrite)) &&
     !method_defined?(:_mkxp_orig_binwrite)
    alias _mkxp_orig_binwrite binwrite

    def binwrite(path, *args)
      _mkxp_orig_binwrite(MKXPSaveFS.change_target(path), *args)
    end
  end

  # Ruby 3 separates keyword args from positionals. Without the
  # ruby2_keywords flag these *args wrappers would collapse
  # `File.open(path, mode: 'rb')`-style kwargs into a positional
  # Hash and the original method raises TypeError. No-op relevant
  # on 1.8/1.9 (hash-positional is the native semantic there).
  if respond_to?(:ruby2_keywords, true)
    ruby2_keywords :open
    ruby2_keywords :new
    ruby2_keywords :read
    ruby2_keywords :readlines
    ruby2_keywords :foreach
    ruby2_keywords :binread if method_defined?(:_mkxp_orig_binread)
    ruby2_keywords :write if method_defined?(:_mkxp_orig_write)
    ruby2_keywords :binwrite if method_defined?(:_mkxp_orig_binwrite)
  end
end

module FileTest
  class << self
    alias _mkxp_orig_exist exist? unless method_defined?(:_mkxp_orig_exist)
    alias _mkxp_orig_file file? unless method_defined?(:_mkxp_orig_file)
    alias _mkxp_orig_directory directory? unless method_defined?(:_mkxp_orig_directory)

    def exist?(path)
      MKXPSaveFS.probe_casefold(path) { |target| _mkxp_orig_exist(target) }
    end

    if (method_defined?(:exists?) || private_method_defined?(:exists?)) &&
       !method_defined?(:_mkxp_orig_exists)
      alias _mkxp_orig_exists exists?

      def exists?(path)
        MKXPSaveFS.probe_casefold(path) { |target| _mkxp_orig_exists(target) }
      end
    end

    def file?(path)
      MKXPSaveFS.probe_casefold(path) { |target| _mkxp_orig_file(target) }
    end

    def directory?(path)
      MKXPSaveFS.probe_casefold(path) { |target| _mkxp_orig_directory(target) }
    end

    # Size probes need the same ENOENT casefold retry File.open has.
    # Daybreak's AudioUtilities scans MP3 frames through an already
    # casefolded File.open, then calls FileTest.size with the
    # game's own mismatched spelling ("Audio/BGM/TITLE.mp3" for
    # Title.mp3) and dies at the title screen without the retry.
    if (method_defined?(:size) || private_method_defined?(:size)) && !method_defined?(:_mkxp_orig_size)
      alias _mkxp_orig_size size

      def size(path)
        _mkxp_orig_size(MKXPSaveFS.path_for(path))
      rescue Errno::ENOENT
        fixed = MKXPSaveFS.read_fallback_target(path)
        raise unless fixed

        _mkxp_orig_size(fixed)
      end
    end

    if (method_defined?(:size?) || private_method_defined?(:size?)) && !method_defined?(:_mkxp_orig_size?)
      alias _mkxp_orig_size? size?

      # size? answers nil for a missing file instead of raising, so
      # the casefold retry keys on that rather than on ENOENT.
      # rubocop:disable Style/ReturnNilInPredicateMethodDefinition
      # -- nil-for-missing is FileTest.size?'s documented contract.
      def size?(path)
        found = _mkxp_orig_size?(MKXPSaveFS.path_for(path))
        return found unless found.nil?

        fixed = MKXPSaveFS.read_fallback_target(path)
        fixed ? _mkxp_orig_size?(fixed) : nil
      end
      # rubocop:enable Style/ReturnNilInPredicateMethodDefinition
    end
  end
end

class << Dir
  alias _mkxp_orig_glob glob unless method_defined?(:_mkxp_orig_glob)
  alias _mkxp_orig_brackets [] unless method_defined?(:_mkxp_orig_brackets)
  alias _mkxp_orig_new new unless method_defined?(:_mkxp_orig_new)
  alias _mkxp_orig_open open unless method_defined?(:_mkxp_orig_open)
  alias _mkxp_orig_entries entries unless method_defined?(:_mkxp_orig_entries)
  if (method_defined?(:exist?) || private_method_defined?(:exist?)) && !method_defined?(:_mkxp_orig_exist)
    alias _mkxp_orig_exist exist?
  end
  alias _mkxp_orig_mkdir mkdir unless method_defined?(:_mkxp_orig_mkdir)

  # Globs are literal. The one side effect: a bare save-file pattern
  # first recovers any legacy saves stranded at the UserData root
  # into the working directory (see recover_saves_for_glob), so slot
  # enumeration and the open that follows see the same folder.
  def glob(pattern, *args, &block)
    MKXPSaveFS.recover_saves_for_glob(pattern)
    found = MKXPSaveFS.filter_safe_write_entries(_mkxp_orig_glob(pattern, *args))
    return found unless block

    found.each(&block)
    nil
  end
  # Keep Ruby 3 kwargs (`Dir.glob(pat, base: dir)` - rubygems uses
  # this) flowing through the *args wrapper. See the File note above.
  ruby2_keywords :glob if respond_to?(:ruby2_keywords, true)

  # Dir[] shares glob's recovery. Ancient slot pickers use both.
  def [](*patterns)
    MKXPSaveFS.recover_saves_for_glob(patterns)
    MKXPSaveFS.filter_safe_write_entries(_mkxp_orig_brackets(*patterns))
  end
  ruby2_keywords :[] if respond_to?(:ruby2_keywords, true)

  # Dir.new / Dir.open enumerate too - the Reborn-lineage load
  # screens list saves through Dir.new(getSaveFolder). Give both
  # the portable recovery trigger. Paths stay literal.
  def new(path, *args)
    path = MKXPSaveFS.cut_at_nul(path)
    MKXPSaveFS.maybe_recover_portable_dir(path)
    _mkxp_orig_new(path, *args)
  end
  ruby2_keywords :new if respond_to?(:ruby2_keywords, true)

  def open(path, *args, &block)
    path = MKXPSaveFS.cut_at_nul(path)
    MKXPSaveFS.maybe_recover_portable_dir(path)
    _mkxp_orig_open(path, *args, &block)
  end
  ruby2_keywords :open if respond_to?(:ruby2_keywords, true)

  def entries(path = '.', *args)
    # Listings are literal. A listing of the portable save dir first
    # recovers alias-era stranded content (maybe_recover_portable_dir).
    # Listings of the UserData root (System.data_directory, faked
    # env var spellings) hide engine-internal entries. Everything
    # else passes through.
    path = MKXPSaveFS.cut_at_nul(path)
    MKXPSaveFS.maybe_recover_portable_dir(path)
    target = MKXPSaveFS.path_for(path)
    MKXPSaveFS.list_entries(target, _mkxp_orig_entries(target, *args))
  end
  ruby2_keywords :entries if respond_to?(:ruby2_keywords, true)

  def foreach(path = '.', *args, &block)
    path = MKXPSaveFS.cut_at_nul(path)
    MKXPSaveFS.maybe_recover_portable_dir(path)
    target = MKXPSaveFS.path_for(path)
    found = MKXPSaveFS.list_entries(target, _mkxp_orig_entries(target, *args))
    return found.each unless block

    found.each(&block)
    nil
  end
  ruby2_keywords :foreach if respond_to?(:ruby2_keywords, true)

  # Ruby 2.5+/2.6+ additions. Pokemon Rejuvenation's New Game Plus
  # code lists the save folder via Dir.each_child.
  if method_defined?(:children) || private_method_defined?(:children)
    alias _mkxp_orig_children children unless method_defined?(:_mkxp_orig_children)

    def children(path, *args)
      MKXPSaveFS.maybe_recover_portable_dir(path)
      target = MKXPSaveFS.path_for(path)
      MKXPSaveFS.list_entries(target, _mkxp_orig_children(target, *args))
    end
    ruby2_keywords :children if respond_to?(:ruby2_keywords, true)
  end

  if method_defined?(:each_child) || private_method_defined?(:each_child)
    def each_child(path, *args, &block)
      MKXPSaveFS.maybe_recover_portable_dir(path)
      target = MKXPSaveFS.path_for(path)
      found = MKXPSaveFS.list_entries(target, _mkxp_orig_children(target, *args))
      return found.each unless block

      found.each(&block)
      nil
    end
    ruby2_keywords :each_child if respond_to?(:ruby2_keywords, true)
  end

  if method_defined?(:exist?) || private_method_defined?(:exist?)
    def exist?(path)
      MKXPSaveFS.probe_casefold(path) { |target| _mkxp_orig_exist(target) }
    end
  end

  if (method_defined?(:exists?) || private_method_defined?(:exists?)) && !method_defined?(:_mkxp_orig_exists)
    alias _mkxp_orig_exists exists?

    def exists?(path)
      MKXPSaveFS.probe_casefold(path) { |target| _mkxp_orig_exists(target) }
    end
  end

  # The portable save dir is LITERAL: Pokemon Rejuvenation's
  # portable mode (isPortable -> getSaveFolder == "Save Data/")
  # creates it with
  #   Dir.mkdir("Save Data/") unless File.exists?("Save Data/")
  # and then runs
  #   Dir.mkdir(RTP.getSaveFolder + "Battle Debug Logs/")
  # at the start of every battle, unrescued. Both resolve relative
  # to the game folder and must really create the directories there,
  # exactly as on Windows/JoiPlay - an earlier alias scheme answered
  # the first mkdir with a virtual success, which left the second
  # one without a parent and black-screened the battle transition
  # with Errno::ENOENT.
  # The wrappers exist for one reason: the UserData root itself is
  # host-owned - "create it" reports success instead of
  # Errno::EEXIST, and "remove it" must never actually happen.
  def mkdir(path, *args)
    target = MKXPSaveFS.path_for(MKXPSaveFS.cut_at_nul(path))
    return 0 if MKXPSaveFS.save_root_target?(target)

    # Case-resolve so FileUtils.mkdir_p of "audio/bgs" extends the
    # on-disk "Audio/BGS" tree instead of creating a duplicate one.
    _mkxp_orig_mkdir(MKXPSaveFS.change_spelling(target), *args)
  end

  alias _mkxp_orig_rmdir rmdir unless method_defined?(:_mkxp_orig_rmdir)

  def rmdir(path)
    target = MKXPSaveFS.path_for(MKXPSaveFS.cut_at_nul(path))
    return 0 if MKXPSaveFS.save_root_target?(target)

    _mkxp_orig_rmdir(MKXPSaveFS.change_spelling(target))
  end
  alias delete rmdir
  alias unlink rmdir
end

module Kernel
  alias _mkxp_orig_load_data load_data unless method_defined?(:_mkxp_orig_load_data)
  alias _mkxp_orig_save_data save_data unless method_defined?(:_mkxp_orig_save_data)

  def load_data(path, *args)
    _mkxp_orig_load_data(MKXPSaveFS.path_for(path), *args)
  end

  def save_data(obj, path, *args)
    _mkxp_orig_save_data(obj, MKXPSaveFS.path_for(path), *args)
  end
  module_function :load_data, :save_data
end

# IO.read and Kernel#open do not call the File wrappers above.
# Kernel#open makes the File object in C (rb_io_s_open).
if RUBY_VERSION < '1.9'
  class << IO
    alias _mkxp_orig_io_read read unless method_defined?(:_mkxp_orig_io_read)
    alias _mkxp_orig_io_readlines readlines unless method_defined?(:_mkxp_orig_io_readlines)
    alias _mkxp_orig_io_foreach foreach unless method_defined?(:_mkxp_orig_io_foreach)

    def read(path, *args)
      _mkxp_orig_io_read(MKXPSaveFS.cut_at_nul(path), *args)
    end

    def readlines(path, *args)
      _mkxp_orig_io_readlines(MKXPSaveFS.cut_at_nul(path), *args)
    end

    def foreach(path, *args, &block)
      _mkxp_orig_io_foreach(MKXPSaveFS.cut_at_nul(path), *args, &block)
    end
  end

  module Kernel
    alias _mkxp_orig_kernel_open open unless private_method_defined?(:_mkxp_orig_kernel_open)

    def open(path, *args, &block)
      _mkxp_orig_kernel_open(MKXPSaveFS.cut_at_nul(path), *args, &block)
    end
    module_function :open
  end
end

# A Dir object lists the folder through its own reader, past the
# class methods above.
class Dir
  alias _mkxp_orig_read read unless method_defined?(:_mkxp_orig_read)
  alias _mkxp_orig_each each unless method_defined?(:_mkxp_orig_each)

  def read
    name = _mkxp_orig_read
    name = _mkxp_orig_read while name && MKXPSaveFS.safe_write_entry?(name)
    name
  end

  def each
    return enum_for(:each) unless block_given?

    _mkxp_orig_each { |name| yield name unless MKXPSaveFS.safe_write_entry?(name) }
    self
  end

  if method_defined?(:children)
    alias _mkxp_orig_children children unless method_defined?(:_mkxp_orig_children)

    def children
      MKXPSaveFS.filter_safe_write_entries(_mkxp_orig_children)
    end
  end

  if method_defined?(:each_child)
    alias _mkxp_orig_each_child each_child unless method_defined?(:_mkxp_orig_each_child)

    def each_child
      return enum_for(:each_child) unless block_given?

      _mkxp_orig_each_child { |name| yield name unless MKXPSaveFS.safe_write_entry?(name) }
      self
    end
  end
end

# --- Dir.chdir ---
# RGSS raises for Dir.chdir(nil) and Dir.chdir(""). Only the call with
# no argument goes to HOME, which on a device is the app folder.
# Windows refuses an empty path with EINVAL, POSIX with ENOENT.
class << Dir
  alias _mkxp_orig_chdir chdir unless method_defined?(:_mkxp_orig_chdir)
  def chdir(*args, &block)
    if args.length == 1
      args[0] = MKXPSaveFS.cut_at_nul(args[0])
      raise Errno::EINVAL, args[0] if args[0] == ''
    end
    _mkxp_orig_chdir(*args, &block)
  end
end
if defined?(System) && System.respond_to?(:puts)
  has_orig = Dir.respond_to?(:_mkxp_orig_chdir)
  System.puts "[windows_fs] Dir.chdir patch applied (orig defined? #{has_orig})"
end

# --- Portable-save migration arming ---
# The sweep is access-driven (see the portable-save migration
# section in MKXPSaveFS): the wrappers above watch for the game to
# touch portable "Save Data" content, and the first qualifying
# access runs one sweep per boot through the _mkxp_orig_* aliases.
# No boot-time gate: launcher-identity portable modes
# (Rejuvenation's RTP.isPortable -> mobile? -> the launcher global) are invisible
# before game scripts run, and a $joiplay-keyed boot sweep would
# move root saves for games that never read the portable dir.
MKXPSaveFS.capture_game_root!
MKXPSaveFS.arm_portable_sweep!
