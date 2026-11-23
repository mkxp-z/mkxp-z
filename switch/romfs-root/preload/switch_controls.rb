# switch_controls.rb
# Game Boy style controls for Pokemon Essentials games, on top of the Switch
# controller layout in src/input/keybindings.cpp:
#
#   B (Input::B)          back; held on the map, runs instead of opening the menu
#   Y / + (Input::B + F6) Start: opens the pause menu on the map
#   ZL (Input::F5)        soft reset, after a Yes/No prompt, from any screen
#
# The B and run hooks only change behaviour when called from the Essentials
# method they target, so other games (and other uses of the same buttons) are
# unaffected. The soft reset works in any game.

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

	# On the Essentials map with nothing else running, where its own "return
	# to title" (Scene_Map#update checks $game_temp.to_title) is instant
	def self.map_reset_possible?
		return false unless defined?(Scene_Map) && $scene.is_a?(Scene_Map)
		return false unless $game_temp && $game_temp.respond_to?(:to_title=)
		return false if $game_temp.in_battle || $game_temp.in_menu ||
		                $game_temp.message_window_showing
		return false if defined?(pbMapInterpreterRunning?) && pbMapInterpreterRunning?
		true
	end

	# Whether the game allows the engine's reset (F12). Games that can't
	# survive their scripts being run again turn it off in mkxp.json.
	def self.engine_reset_allowed?
		return @engine_reset_allowed unless @engine_reset_allowed.nil?
		json = File.exist?("mkxp.json") ? File.read("mkxp.json") : ""
		off = json.each_line.any? { |l| l !~ %r{\A\s*//} && l =~ /"enableReset"\s*:\s*false/ }
		@engine_reset_allowed = !off
	end

	def self.reset_exception
		Object.const_defined?(:RGSSReset) ? RGSSReset : Reset
	end

	# Polled every frame from Graphics.update. On the Essentials map this
	# returns to the title at once. Anywhere else (battles, menus, other
	# games) it does what F12 does in RPG Maker: the engine reloads the game's
	# scripts and starts again from the title screen.
	def self.poll_soft_reset
		down = Input.press?(SOFT_RESET)
		pressed = down && !@reset_down
		@reset_down = down
		return unless pressed && !@prompting
		on_map = map_reset_possible?
		return unless on_map || engine_reset_allowed?
		@prompting = true
		begin
			yes = confirm("Soft reset?", "Unsaved progress will be lost.")
		ensure
			@prompting = false
		end
		return unless yes
		if on_map
			$game_temp.to_title = true
		else
			show_resetting
			raise reset_exception
		end
	end

	# Presents one "Resetting..." frame, which stays on screen while the
	# scripts reload (like loading_screen.rb at startup)
	def self.show_resetting
		bitmap = Bitmap.new(Graphics.width, Graphics.height)
		bitmap.fill_rect(bitmap.rect, Color.new(0, 0, 0))
		bitmap.font.size = [Graphics.height / 18, 16].max
		bitmap.font.color = Color.new(220, 220, 220)
		line = bitmap.font.size * 2
		bitmap.draw_text(0, (Graphics.height - line) / 2, Graphics.width, line, "Resetting...", 1)
		sprite = Sprite.new
		sprite.bitmap = bitmap
		sprite.z = 0x7fffffff
		Graphics.__switch_update
	ensure
		sprite.dispose if sprite
		bitmap.dispose if bitmap
	end

	# A Yes/No box drawn by the engine above everything else, so it works on
	# any screen of any game. A answers yes; B or ZL again answers no.
	def self.confirm(title, detail)
		viewport = Viewport.new(0, 0, Graphics.width, Graphics.height)
		viewport.z = 1_000_000
		sprite = Sprite.new(viewport)
		sprite.bitmap = Bitmap.new(Graphics.width, Graphics.height)
		draw_box(sprite.bitmap, title, detail)
		answer = nil
		while answer.nil?
			Graphics.update
			Input.update
			if Input.trigger?(Input::C)
				answer = true
			elsif Input.trigger?(Input::B) || Input.trigger?(SOFT_RESET)
				answer = false
			end
		end
		answer
	ensure
		sprite.bitmap.dispose if sprite && sprite.bitmap
		sprite.dispose if sprite
		viewport.dispose if viewport
	end

	def self.draw_box(b, title, detail)
		b.fill_rect(0, 0, b.width, b.height, Color.new(0, 0, 0, 110))
		w = [b.width - 48, 380].min
		h = 116
		x = (b.width - w) / 2
		y = (b.height - h) / 2
		b.fill_rect(x, y, w, h, Color.new(28, 28, 28, 240))
		b.fill_rect(x, y, w, 3, Color.new(255, 190, 40))
		b.font.color = Color.new(255, 255, 255)
		b.font.size = 26
		b.draw_text(x, y + 12, w, 32, title, 1)
		b.font.color = Color.new(200, 200, 200)
		b.font.size = 20
		b.draw_text(x, y + 46, w, 26, detail, 1)
		b.draw_text(x, y + 78, w, 26, "A: Yes      B: No", 1)
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
