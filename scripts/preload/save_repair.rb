# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# Repairs broken sound names before a game writes a save, and tells the
# host when a game loads a save.
#
# Before #81 the Ruby 1.8 sweep freed the name of the sound clone that
# Pokemon Essentials keeps across a battle. The name then held a Float,
# an Array or another object, the game saved it, and the load crashed.
#
# The VM outlives a game, and each game loads this file again. The
# wrappers must go on only once, because platform_compat.rb wraps
# Marshal.dump again, and only once.
unless defined?(MKXPSaveRepair)
  module MKXPSaveRepair
    module_function

    # Essentials writes a save with one Marshal.dump for each part. The
    # repair runs once for each open file.
    def before_dump(port)
      return unless File === port # rubocop:disable Style/CaseEquality -- a BasicObject has no is_a?.
      return if port.instance_variable_get(:@mkxp_save_repaired)

      port.instance_variable_set(:@mkxp_save_repaired, true)
      repair_all(File.basename(port.path.to_s))
    rescue StandardError
      nil
    end

    # Reads @name, because games define the getter again. Sends one
    # report for each kind of fault, not one for each sound. The name is
    # set before the report, so a name that has no #class (a BasicObject)
    # is repaired too.
    def repair_all(file_name)
      klass = audio_file_class
      return unless klass

      faults = {}
      ObjectSpace.each_object(klass) do |audio|
        begin
          name = audio.instance_variable_get(:@name)
          # rubocop:disable Style/CaseEquality -- a BasicObject has no nil? or is_a?.
          next if ::NilClass === name || ::String === name || audio.frozen?
          # rubocop:enable Style/CaseEquality

          audio.instance_variable_set(:@name, '')
          faults[fault(audio, name)] = true
        rescue StandardError
          next
        end
      end
      faults.each_key { |text| report(:report_save_repaired, "#{file_name}: #{text}") }
    end

    # audio_file_name_repair.rb calls this when the getter repairs a name.
    def repaired_on_read(audio, name)
      text = "#{fault(audio, name)} when the game read it"
      @read_faults ||= {}
      return if @read_faults[text]

      @read_faults[text] = true
      report(:report_save_repaired, "#{@last_save || 'unknown file'}: #{text}")
    rescue StandardError
      nil
    end

    def fault(audio, name)
      "the @name of #{with_article(audio.class.name.to_s)} was #{with_article(name.class.name.to_s)}"
    end

    def with_article(name)
      name =~ /\A(?:[AEIOU]|RPG)/ ? "an #{name}" : "a #{name}"
    end

    def audio_file_class
      RPG::AudioFile if defined?(RPG::AudioFile) && RPG::AudioFile.is_a?(Class)
    end

    # A save holds the Game_System, alone or in the Hash or the Array of
    # the save. Pokemon Essentials reads the first parts of Game.rxdata at
    # start and on the title screen, so a file counts only when the game
    # reads it to the end.
    def after_load(source, value)
      return unless File === source # rubocop:disable Style/CaseEquality -- a BasicObject has no is_a?.
      return if source.instance_variable_get(:@mkxp_save_loaded)

      source.instance_variable_set(:@mkxp_game_system, true) if holds_game_system?(value)
      return unless source.instance_variable_get(:@mkxp_game_system) && source.eof?

      source.instance_variable_set(:@mkxp_save_loaded, true)
      save_read(File.basename(source.path.to_s))
    rescue StandardError
      nil
    end

    def save_read(file_name)
      return loaded(file_name) unless essentials_game_load?

      wrap_game_load
      @last_save = file_name
    end

    # Pokemon Essentials 19 and later reads the whole save at start and
    # when the load screen opens. Continue gives that data to Game.load,
    # so in these games Game.load is the save load. The game defines
    # Game.load before its first read of a save.
    def essentials_game_load?
      defined?(::SaveData) && defined?(::Game) && ::Game.respond_to?(:load)
    end

    # F12 runs the game's scripts again, and they define Game.load again
    # over the wrapper. A script can also wrap Game.load after us. Each
    # wrapper keeps its original under its own name, so a second wrapper
    # cannot call itself through a shared name.
    def wrap_game_load
      return if @game_load_wrapper && ::Game.method(:load) == @game_load_wrapper

      @game_load_wraps = (@game_load_wraps || 0) + 1
      original = "_mkxp_repair_load_#{@game_load_wraps}"
      game = class << ::Game; self; end
      game.send(:alias_method, original, :load)
      # Ruby 1.8 cannot parse a block argument of a block, so the wrapper
      # is a def in a string.
      game.class_eval(<<-RUBY, __FILE__, __LINE__ + 1) # rubocop:disable Style/DocumentDynamicEvalDefinition
        def load(*args, &block)
          MKXPSaveRepair.game_load { #{original}(*args, &block) }
        end
      RUBY
      # Ruby 3 passes keyword arguments on through *args only with this.
      game.send(:ruby2_keywords, :load) if game.respond_to?(:ruby2_keywords, true)
      @game_load_wrapper = ::Game.method(:load)
    end

    # A script that wraps Game.load after us puts one of our wrappers
    # inside another. Only the outer one reports.
    def game_load
      return yield if @in_game_load

      @in_game_load = true
      begin
        report_game_load
        yield
      ensure
        @in_game_load = false
      end
    end

    def report_game_load
      loaded(@last_save || 'unknown file')
    rescue StandardError
      nil
    end

    # load_data reads the file in C, and gives Marshal.load a String.
    # windows_fs.rb defines load_data again for each game, so its
    # load_data calls this.
    def after_load_data(path, value)
      save_read(File.basename(path.to_s)) if holds_game_system?(value)
    rescue StandardError
      nil
    end

    def loaded(file_name)
      @last_save = file_name
      @read_faults = {}
      report(:report_save_load, file_name)
    end

    def holds_game_system?(value)
      return false unless defined?(::Game_System) && ::Game_System.is_a?(Class)
      # rubocop:disable Style/CaseEquality -- a BasicObject has no is_a?.
      return true if ::Game_System === value

      list = if ::Hash === value
               value.values
             elsif ::Array === value
               value
             else
               []
             end
      list.any? { |item| ::Game_System === item }
      # rubocop:enable Style/CaseEquality
    end

    def report(name, detail)
      System.send(name, detail) if defined?(System) && System.respond_to?(name)
    end
  end

  class << Marshal
    alias _mkxp_repair_dump dump
    alias _mkxp_repair_load load

    def dump(obj, *args)
      MKXPSaveRepair.before_dump(args[0])
      _mkxp_repair_dump(obj, *args)
    end

    def load(source, *args)
      value = _mkxp_repair_load(source, *args)
      MKXPSaveRepair.after_load(source, value)
      value
    end
  end
end
