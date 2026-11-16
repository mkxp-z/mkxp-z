# Cheat menu for RPG Maker VX (RGSS2).
#
# Ported from JoiPlay's cheat_rpgmvx.rb. Loaded by cheat_dispatch.rb
# when the RGSS version is 2. Menu is triggered in-game by pressing
# Input::HOME while MKXP.cheats_enabled? is true.
#
# Features: level up party member 0 (x1, x5, x10, x100), gain gold,
# add items/weapons/armors with a number-picker window.

MKXP.puts('[cheats] loading RPG Maker VX cheat menu')

class Window_GetItemNumber < Window_Base
  def initialize(x, y)
    super(x, y, 304, 120)
    @item = nil
    @max = 1
    @price = 0
    @number = 1
  end

  def set(item, max, _price)
    @item = item
    @max = max
    @price = 0
    @number = 1
    refresh
  end

  attr_reader :number

  def refresh
    y = 0
    contents.clear
    draw_item_name(@item, 0, y)
    contents.font.color = normal_color
    contents.draw_text(212, y, 20, WLH, 'x')
    contents.draw_text(248, y, 20, WLH, @number.to_s, 2)
    cursor_rect.set(244, y, 28, WLH)
    draw_currency_value(@price * @number, 4, y + (WLH * 2), 264)
  end

  def update
    super
    return unless active

    last_number = @number
    @number += 1 if Input.repeat?(Input::RIGHT) && @number < @max
    @number -= 1 if Input.repeat?(Input::LEFT) && @number > 1
    @number = [@number + 10, @max].min if Input.repeat?(Input::UP) && @number < @max
    @number = [@number - 10, 1].max if Input.repeat?(Input::DOWN) && @number > 1
    return unless @number != last_number

    Sound.play_cursor
    refresh
  end
end

class Window_GetItem < Window_Selectable
  def initialize
    super(0, 0, 280, 304)
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

  def draw_item(index)
    item = @data[index]
    contents.font.color = normal_color
    x = 4
    y = index * WLH
    rect = Rect.new(x, y, width - 32, WLH)
    contents.fill_rect(rect, Color.new(0, 0, 0, 0))
    contents.draw_text(x + 4, y, 212, WLH, item.name, 0)
    contents.draw_text(x + 220, y, 88, WLH, item.price.to_s, 2)
  end
end

class Scene_Cheat
  # rubocop:disable Metrics/AbcSize -- builds the full menu set
  # in one pass. Splitting across helpers would scatter related
  # window state.
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

    @number_window = Window_GetItemNumber.new(0, 0)
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

    number = $game_party.item_number(@item)
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
    $game_party.gain_item(@item, @number_window.number)
    @item_window.refresh
    @item_window.active = true
    @item_window.visible = true
  end

  def cheat_level_up
    return unless $game_party && $game_party.members[0]

    $game_party.members[0].level_up unless $game_party.members[0].level >= 99
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

# Same postload-before-scripts guard as cheat_rpgmxp.rb - see
# that file's comment on why this matters for meta-loader games.
if defined?(Game_Player) && Game_Player.method_defined?(:update)
  class Game_Player
    alias cheat_update update unless method_defined?(:cheat_update)
    def update
      cheat_update
      return unless MKXP.cheat_menu_requested?

      $scene = Scene_Cheat.new
    end
  end
  MKXP.puts('[cheats] RPG Maker VX cheat menu ready')
else
  MKXP.puts('[cheats] RPG Maker VX cheat menu deferred: ' \
            'Game_Player not loaded at postload time')
end
