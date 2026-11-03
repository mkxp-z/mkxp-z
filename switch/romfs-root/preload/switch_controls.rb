# switch_controls.rb
# Game Boy style controls for Pokemon Essentials games, on top of the Switch
# controller layout in src/input/keybindings.cpp:
#
#   B (Input::B)          back; held on the map, runs instead of opening the menu
#   Y / + (Input::B + F6) Start: opens the pause menu on the map
#   ZL (Input::F5)        soft reset, after a Yes/No prompt
#
# Each hook only changes behaviour when called from the Essentials method it
# targets, so other games (and other uses of the same buttons) are unaffected.

module SwitchControls
	START = Input::F6
	SOFT_RESET = Input::F5

	@reset_down = false
	@prompting = false

	# Called from Scene_Map#update itself (where Essentials checks B to open
	# the menu), not from a message or menu running on the map?
	def self.map_menu_check?
		return false unless defined?(Scene_Map) && $scene.is_a?(Scene_Map)
		loc = caller_locations(2, 1)[0]
		loc && loc.base_label == "update" && loc.path.end_with?("Scene_Map")
	end

	# Called from Game_Player#pbCanRun?, Essentials' run check (it reads A)?
	def self.run_check?
		loc = caller_locations(2, 1)[0]
		loc && loc.base_label == "pbCanRun?"
	end

	def self.soft_reset_allowed?
		return false unless defined?(Scene_Map) && $scene.is_a?(Scene_Map)
		return false unless defined?(pbConfirmMessageSerious) && $game_temp
		return false unless $game_temp.respond_to?(:to_title=)
		return false if $game_temp.in_battle || $game_temp.in_menu ||
		                $game_temp.message_window_showing
		return false if defined?(pbMapInterpreterRunning?) && pbMapInterpreterRunning?
		true
	end

	# Polled every frame from Graphics.update. Uses Essentials' own "return to
	# title" path (Scene_Map#update checks $game_temp.to_title) rather than a
	# full script reset, which re-runs the game's alias chains.
	def self.poll_soft_reset
		down = Input.press?(SOFT_RESET)
		pressed = down && !@reset_down
		@reset_down = down
		return unless pressed && !@prompting && soft_reset_allowed?
		@prompting = true
		begin
			text = "Soft reset? Unsaved progress will be lost."
			text = _INTL(text) if defined?(_INTL)
			$game_temp.to_title = true if pbConfirmMessageSerious(text)
		ensure
			@prompting = false
		end
	end
end

class << Input
	alias_method :__switch_trigger?, :trigger?
	alias_method :__switch_press?, :press?

	def trigger?(button)
		if button == Input::B && SwitchControls.map_menu_check?
			return __switch_trigger?(SwitchControls::START)
		end
		__switch_trigger?(button)
	end

	def press?(button)
		pressed = __switch_press?(button)
		return pressed if pressed || button != Input::A
		return pressed unless SwitchControls.run_check?
		__switch_press?(Input::B) && !__switch_press?(SwitchControls::START)
	end
end

class << Graphics
	alias_method :__switch_update, :update

	def update
		__switch_update
		SwitchControls.poll_soft_reset
	end
end
