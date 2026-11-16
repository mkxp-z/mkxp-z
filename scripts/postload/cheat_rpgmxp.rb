# Cheat menu for RPG Maker XP (RGSS1) generic projects.
#
# Ported from the non-Pokemon branch of JoiPlay's cheat_rpgmxp.rb.
# Loaded by cheat_dispatch.rb when the RGSS version is 1 and the
# project is not a Pokemon Essentials game. Menu is triggered
# in-game by pressing Input::HOME while MKXP.cheats_enabled? is true.
#
# Features: level up actor[0] (x1, x5, x10, x100), gain gold,
# add items/weapons/armors via a custom Window_GetItem.

MKXP.puts('[cheats] loading RPG Maker XP cheat menu')

if defined?(Window_Selectable).nil? && defined?(Window_DrawableCommand)
  class Window_Selectable < Window_DrawableCommand
  end
end

class Window_GetItem < Window_Selectable
  def initialize
    super(0, 128, 368, 352)
    @shop_goods = []
    @shop_goods += $data_items   if defined?($data_items)   && $data_items
    @shop_goods += $data_weapons if defined?($data_weapons) && $data_weapons
    @shop_goods += $data_armors  if defined?($data_armors)  && $data_armors
    refresh
    self.index = 0
  end

  def item
    @data[index]
  end

  def refresh
    unless contents.nil?
      contents.dispose
      self.contents = nil
    end
    @data = []
    @shop_goods.each do |item|
      @data.push(item) if !item.nil? && item.name != ''
    end
    @item_max = @data.size
    return unless @item_max > 0

    self.contents = Bitmap.new(width - 32, row_max * 32)
    (0...@item_max).each do |i|
      draw_item(i)
    end
  end

  # rubocop:disable Metrics/AbcSize -- the per-row item draw
  # walks count + label + price in one pass to keep the
  # visual layout grouped in the source.
  def draw_item(index)
    item = @data[index]
    number = case item
             when RPG::Item   then $game_party.item_number(item.id)
             when RPG::Weapon then $game_party.weapon_number(item.id)
             when RPG::Armor  then $game_party.armor_number(item.id)
             else 0
             end
    contents.font.color = number < 99 ? normal_color : disabled_color
    x = 4
    y = index * 32
    rect = Rect.new(x, y, width - 32, 32)
    contents.fill_rect(rect, Color.new(0, 0, 0, 0))
    begin
      bitmap = RPG::Cache.icon(item.icon_name)
      opacity = contents.font.color == normal_color ? 255 : 128
      contents.blt(x, y + 4, bitmap, Rect.new(0, 0, 24, 24), opacity)
    rescue StandardError
      # Icon name missing on this RPG Maker XP build. Skip the icon.
    end
    contents.draw_text(x + 28, y, 212, 32, item.name, 0)
    contents.draw_text(x + 240, y, 88, 32, item.price.to_s, 2)
  end
  # rubocop:enable Metrics/AbcSize
end

class Scene_Cheat
  # rubocop:disable Metrics/AbcSize -- builds the full menu set
  # in one pass. Splitting the windows would scatter related
  # state across many helper methods.
  def main
    @cheat_window = Window_Command.new(160, ['Level Up', 'Gain Gold', 'Get Items', 'Cancel'])
    @cheat_window.active = true
    @cheat_window.visible = true

    @level_window = Window_Command.new(160, ['1 Level', '5 Level', '10 Level', '100 Level', 'Cancel'])
    @level_window.active = false
    @level_window.visible = false

    @gold_window = Window_Command.new(160, ['100 G', '1K G', '10K G', '100K G', '1M G', 'Cancel'])
    @gold_window.active = false
    @gold_window.visible = false

    @item_window = Window_GetItem.new
    @item_window.active = false
    @item_window.visible = false

    @number_window = Window_ShopNumber.new
    @number_window.active = false
    @number_window.visible = false

    Graphics.transition
    loop do
      Graphics.update
      Input.update
      update
      break if $scene != self
    end
    Graphics.freeze
    @cheat_window.dispose
    @level_window.dispose
    @gold_window.dispose
    @item_window.dispose
    @number_window.dispose
  end
  # rubocop:enable Metrics/AbcSize

  def update
    @cheat_window.update
    @level_window.update
    @gold_window.update
    @item_window.update
    @number_window.update
    return update_cheat  if @cheat_window.active
    return update_level  if @level_window.active
    return update_gold   if @gold_window.active
    return update_item   if @item_window.active

    update_number if @number_window.active
  end

  def update_cheat
    if Input.trigger?(Input::B)
      $scene = Scene_Map.new
      return
    end
    return unless Input.trigger?(Input::C)

    case @cheat_window.index
    when 0
      @cheat_window.active = false
      @cheat_window.visible = false
      @level_window.active = true
      @level_window.visible = true
      @level_window.refresh
    when 1
      @cheat_window.active = false
      @cheat_window.visible = false
      @gold_window.active = true
      @gold_window.visible = true
      @gold_window.refresh
    when 2
      @cheat_window.active = false
      @cheat_window.visible = false
      @item_window.active = true
      @item_window.visible = true
      @item_window.refresh
    when 3
      $scene = Scene_Map.new
    end
  end

  def update_level
    if Input.trigger?(Input::B)
      $scene = Scene_Map.new
      return
    end
    return unless Input.trigger?(Input::C)

    case @level_window.index
    when 0 then cheat_level_up1
    when 1 then cheat_level_up5
    when 2 then cheat_level_up10
    when 3 then cheat_level_up100
    end
    $scene = Scene_Map.new
  end

  def update_gold
    if Input.trigger?(Input::B)
      $scene = Scene_Map.new
      return
    end
    return unless Input.trigger?(Input::C)

    case @gold_window.index
    when 0 then cheat_add_gold(100)
    when 1 then cheat_add_gold(1_000)
    when 2 then cheat_add_gold(10_000)
    when 3 then cheat_add_gold(100_000)
    when 4 then cheat_add_gold(1_000_000)
    end
    $scene = Scene_Map.new
  end

  def update_item
    if Input.trigger?(Input::B)
      $scene = Scene_Map.new
      return
    end
    return unless Input.trigger?(Input::C)

    @item = @item_window.item
    return if @item.nil?

    number = case @item
             when RPG::Item   then $game_party.item_number(@item.id)
             when RPG::Weapon then $game_party.weapon_number(@item.id)
             when RPG::Armor  then $game_party.armor_number(@item.id)
             else 0
             end
    return if number == 99

    max = [99, 99 - number].min
    @item_window.active = false
    @item_window.visible = false
    @number_window.set(@item, max, @item.price)
    @number_window.active = true
    @number_window.visible = true
  end

  def update_number
    if Input.trigger?(Input::B)
      @number_window.active = false
      @number_window.visible = false
      @item_window.active = true
      @item_window.visible = true
      return
    end
    return unless Input.trigger?(Input::C)

    @number_window.active = false
    @number_window.visible = false
    case @item
    when RPG::Item   then $game_party.gain_item(@item.id, @number_window.number)
    when RPG::Weapon then $game_party.gain_weapon(@item.id, @number_window.number)
    when RPG::Armor  then $game_party.gain_armor(@item.id, @number_window.number)
    end
    @item_window.refresh
    @item_window.active = true
    @item_window.visible = true
  end

  def cheat_level_up
    return unless $game_party && $game_party.actors && $game_party.actors[0]

    $game_party.actors[0].level = $game_party.actors[0].level + 1
  end

  def cheat_level_up1
    cheat_level_up
  end

  def cheat_level_up5
    5.times   { cheat_level_up }
  end

  def cheat_level_up10
    10.times  { cheat_level_up }
  end

  def cheat_level_up100
    100.times { cheat_level_up }
  end

  def cheat_add_gold(amount)
    cap = 9_999_999
    if ($game_party.gold + amount) >= cap
      $game_party.gain_gold(cap - $game_party.gold)
    else
      $game_party.gain_gold(amount)
    end
  end
end

# Guard the Game_Player#update hook behind a runtime check that
# `Game_Player` is actually a loaded game class with an `update`
# method.
#
# Why this matters on iOS: the engine's postload phase runs right
# before the last entry in the Scripts.rxdata array (the classic
# placeholder "Main" in stock RMXP). For 99% of RMXP games that's
# AFTER Game_Character / Game_Player / etc. have been evaluated,
# so `Game_Player` is already a proper subclass of
# `Game_Character` with an `update` method, and the alias+override
# below works cleanly.
#
# But some games ship a tiny meta-loader as their Scripts.rxdata
# (Pokemon Reborn's is 107 bytes and just `eval`s
# `Scripts/Reborn/Bootstrap.rb` + `Scripts/ScriptLoader.rb` at
# runtime, which THEN loops through an explicit SCRIPTS array to
# load each .rb file). For those games, scriptCount == 1 and our
# postload runs BEFORE any of the game's real scripts. Without
# this guard the unguarded `class Game_Player` here creates
# Game_Player as a bare subclass of Object. Then the `alias`
# raises NameError (since `update` isn't defined on Object),
# aborting the rest of this class body but leaving the
# Object-rooted Game_Player constant in place. When the game's
# own `Scripts/Game_Player.rb` later runs with
# `class Game_Player < Game_Character`, Ruby detects
# `superclass mismatch` and raises TypeError. Reborn's
# ScriptLoader rescues + logs and proceeds, so the game keeps
# running with a half-defined Game_Player that has no `moveto`
# of its own. A later script (or the scene chain) then defines
# `moveto` via a separate reopen. `super` in that moveto fails
# because the superclass chain doesn't include Game_Character.
#
# Symptom: `Save Data/errorlog.txt` fills with
# `NoMethodError: super: no superclass method 'moveto' for
# #<Game_Player>` on every map load, and the game can't reach
# its first map after pressing New Game.
if defined?(Game_Player) && Game_Player.method_defined?(:update)
  class Game_Player
    alias cheat_update update unless method_defined?(:cheat_update)
    def update
      cheat_update
      return unless MKXP.cheat_menu_requested?

      $scene = Scene_Cheat.new
    end
  end
  MKXP.puts('[cheats] RPG Maker XP cheat menu ready')
else
  MKXP.puts('[cheats] RPG Maker XP cheat menu deferred: ' \
            'Game_Player not loaded at postload time')
end
