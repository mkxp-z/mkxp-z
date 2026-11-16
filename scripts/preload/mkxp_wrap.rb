# mkxp_wrap.rb
# Author: Splendide Imaginarius (2022)

# Creative Commons CC0: To the extent possible under law, Splendide Imaginarius
# has waived all copyright and related or neighboring rights to mkxp_wrap.rb.
# https://creativecommons.org/publicdomain/zero/1.0/

# This preload script provides functions that existed in Ancurio's mkxp, but
# were renamed in mkxp-z, so that games (or other preload scripts) that expect
# Ancurio's function names can find them.

module MKXP
  class << self
    def data_directory(*args)
      System.data_directory(*args)
    end

    def puts(*args)
      System.puts(*args)
    end

    def raw_key_states(*args)
      states = Input.raw_key_states(*args)
      class << states
        def getbyte(byte)
          self[byte] ? 1 : 0
        end

        def setbyte(byte, val)
          self[byte] = val != 0
        end
      end
      states
    end

    def mouse_in_window(*args)
      Input.mouse_in_window(*args)
    end

    def cheats_enabled?
      System.cheats_enabled?
    end

    # True on the map frame that the HOME press of the cheat button
    # starts. HOME is a button of this engine only, so game scripts
    # that replace Input.trigger? do not expect it, and Yanfly's
    # Debug Extension raises ArgumentError for :HOME. jpress? is the
    # native method under a name that game scripts do not replace.
    #
    # The press advances on each call, not on each Input.update.
    # Pokemon Essentials fan games call Input.update in the middle of
    # a frame, which clears the native trigger edge before the map
    # reads it (see pokemon_input.rb).
    def cheat_menu_requested?
      return false unless cheats_enabled?

      down = Input.jpress?(Input::HOME)
      requested = down && !@cheat_home_down
      @cheat_home_down = down
      requested
    end

    # Per-game in-game-keyboard toggle. The host app sets
    # the bridge value at session start. `pokemon_input.rb`
    # reads this to decide whether to force `USEKEYBOARDTEXTENTRY
    # = false` and surface the game's own keyboard scene.
    # Scripts calling this on engines without the binding hit
    # `NoMethodError`, so callers should `respond_to?` first.
    def use_in_game_keyboard?
      System.use_in_game_keyboard?
    end

    # Per-game state directory the host manages (see
    # `mkxp_setManagedConfigDir`). Engine postloads use this to
    # write runtime-detection marker files.
    def managed_config_dir
      System.managed_config_dir
    end

    # MKXP.rpg_version / ruby_version / power_state mirror
    # JoiPlay's helper module so scripts written against it
    # (notably PE fangames doing `if MKXP.rpg_version > 2`)
    # keep working on mkxp-z.
    def rpg_version(*args)
      System.rpg_version(*args)
    end

    def ruby_version(*args)
      System.ruby_version(*args)
    end

    def power_state(*args)
      System.power_state(*args)
    end

    def platform(*args)
      System.platform(*args)
    end

    # Pokemon Reborn (and several other PE fangames that target
    # JoiPlay) gate at boot on `MKXP.plugin_version.to_i`, which
    # encodes JoiPlay's RPG Maker Plugin version as a 5-digit
    # integer (1.20.53 -> 12053). The check refuses to run if
    # the integer is below a hardcoded minimum. When the host
    # enables JoiPlay compat (`$joiplay = true`, see
    # platform_compat.rb) these games take their JoiPlay code
    # path, so we also have to advertise a plugin version high
    # enough to satisfy those gates. We report 99999 - well
    # above any known minimum - so future releases that bump
    # the floor also pass without us having to chase every
    # version. Harmless when the flag is off. Games only
    # consult this on the JoiPlay path.
    def plugin_version
      99_999
    end

    # Pokemon Reborn's ScriptLoader pipes each script section
    # through `MKXP.apply_overrides` before `eval`, expecting
    # the JoiPlay-shipped patcher to rewrite known-broken
    # lines. We route to our own patcher (src/patcher.cpp),
    # so the same config-driven rewrites work when users set
    # `scriptPatches` in mkxp.json. Scripts that call this on
    # engines without a patcher would just get `str` back.
    def apply_overrides(*args)
      System.apply_overrides(*args)
    end
  end
end
