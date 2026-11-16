# Cheat menu for Pokemon Essentials (v17+ and v19+).
#
# Ported from JoiPlay's cheat_pe19.rb. Loaded by cheat_dispatch.rb
# when the game is detected as Pokemon Essentials (PBItems or
# GameData::Item constant is defined). The menu is triggered
# in-game by pressing Input::HOME while MKXP.cheats_enabled? is true.
#
# Features: heal party, get items (reuses PokemonMart screen with a
# zero-price adapter), walk-through-walls toggle ($wtw).
#
# Design notes: we keep the original class / method names so that
# games defining their own Window_CommandPokemon / PokemonMartScreen
# variants plug in unchanged.

if Object.const_defined?('GameData') || Object.const_defined?('PBItems')
  MKXP.puts('[cheats] loading Pokemon Essentials cheat menu')

  def getPEVersion
    return '19' unless Object.const_defined?('Essentials')

    Essentials::VERSION
  end

  ENABLE_GET_ITEM = Object.const_defined?('PokemonMartScreen')

  if ENABLE_GET_ITEM
    class CheatItemsAdapter < PokemonMartAdapter
      def getPrice(_item, _selling = false)
        0
      end
    end

    if Object.const_defined?('PokemonMart_Scene')
      class SceneCheat_Items < PokemonMart_Scene
      end
    elsif Object.const_defined?('PokemonMartScene')
      class SceneCheat_Items < PokemonMartScene
      end
    end

    class ScreenCheat_Items < PokemonMartScreen
      # rubocop:disable Lint/MissingSuper -- PokemonMartScreen's
      # initializer pulls a regular trainer's bag/funds. We replace
      # that wiring with our cheat-specific scene+stock+adapter.
      def initialize(scene, stock)
        @scene = scene
        @stock = stock
        @adapter = CheatItemsAdapter.new
      end
      # rubocop:enable Lint/MissingSuper
    end
  end

  class Scene_Cheat
    def main
      @wtw_string = $wtw ? 'Disable WTW' : 'Enable WTW'
      @cviewport = Viewport.new(0, 0, Graphics.width, Graphics.height)
      @cviewport.z = 99_999
      @cheat_window = Window_CommandPokemon.new(
        ['Get Items', 'Heal Party', @wtw_string, 'Cancel'], 160
      )
      @cheat_window.active = true
      @cheat_window.visible = true
      @cheat_window.viewport = @cviewport
      Graphics.transition
      loop do
        Graphics.update
        Input.update
        pbUpdateSceneMap
        break if update == -1
      end
      @cheat_window.dispose
      @cviewport.dispose
    end

    def update
      @cheat_window.update
      update_cheat if @cheat_window.active
    end

    # rubocop:disable Metrics/AbcSize, Metrics/CyclomaticComplexity, Metrics/MethodLength, Metrics/PerceivedComplexity
    # The case-when handles each menu entry's distinct flow. The
    # branch bodies don't share enough structure to factor into
    # smaller methods without losing readability.
    def update_cheat
      return -1 if Input.trigger?(Input::B)

      return unless Input.trigger?(Input::C)

      # Essentials v16 to v18 make $Trainer and the bag when the intro asks
      # for the player's name, so the menu can open on a map before them.
      # Essentials v21 has only $player.
      player = $player || $Trainer
      case @cheat_window.index
      when 0
        if ENABLE_GET_ITEM && player
          @cheat_window.active = false
          @cheat_window.visible = false
          scene = SceneCheat_Items.new
          array = []
          if Object.const_defined?('GameData')
            GameData::Item.each do |i|
              array.push(i) unless i.name.empty?
            end
          else
            (0..PBItems.maxValue).each do |i|
              array.push(i) unless PBItems.getName(i).empty?
            end
          end
          screen = ScreenCheat_Items.new(scene, array)
          screen.pbBuyScreen
          pbScrollMap(-6, -5, -5)
        end
        return -1
      when 1
        # rubocop:disable Style/SymbolProc -- Ruby 1.8 can't parse `&:heal`.
        player.party.each { |pokemon| pokemon.heal } if player
        # rubocop:enable Style/SymbolProc
        @cheat_window.active = false
        @cheat_window.visible = false
        return -1
      when 2
        $wtw = !$wtw
        return -1
      when 3
        return -1
      end
      nil
    end
    # rubocop:enable Metrics/AbcSize, Metrics/CyclomaticComplexity, Metrics/MethodLength, Metrics/PerceivedComplexity

    def return_scene
      @cviewport.dispose
    end

    def cheat_cancel
      @cheat_window.close
    end
  end

  class Scene_Map
    alias cheatUpdateMaps updateMaps unless method_defined?(:cheatUpdateMaps)
    # Scene_Cheat#main runs pbUpdateSceneMap, which comes back here.
    def updateMaps
      if !@cheat_menu_open && MKXP.cheat_menu_requested?
        @cheat_menu_open = true
        $game_temp.menu_calling = false if $game_temp
        $game_temp.in_menu = true if $game_temp && $game_temp.respond_to?(:in_menu=)
        $game_player.straighten if $game_player
        $game_map.update if $game_map
        Scene_Cheat.new.main
        $game_temp.in_menu = false if $game_temp && $game_temp.respond_to?(:in_menu=)
        @cheat_menu_open = false
      end
      cheatUpdateMaps
    end
  end

  class Game_Player
    alias cheatPassable? passable? unless method_defined?(:cheatPassable?)
    def passable?(x, y, d, strict = false)
      if $wtw
        new_x = x + (if d == 6
                       1
                     else
                       d == 4 ? -1 : 0
                     end)
        new_y = y + (if d == 2
                       1
                     else
                       d == 8 ? -1 : 0
                     end)
        $game_map.valid?(new_x, new_y)
      else
        begin
          cheatPassable?(x, y, d, strict)
        rescue StandardError
          cheatPassable?(x, y, d)
        end
      end
    end
  end

  MKXP.puts('[cheats] Pokemon Essentials cheat menu ready')
end
