# fast_forward.rb
# Smoother fast-forward for games using Marin's "Better Fast-forward" script
# (Pokemon Fire Ash and many other Essentials fan games), in any version that
# aliases Graphics.update as fast_forward_update.
#
# For N-times speed the script shows every Nth tick at the game's frame rate.
# At 40 fps (EBDX games) that shows 2x and 3x at 40 fps, which a 60 Hz screen
# holds for one and two refreshes in turn. Sprites move N times as far per
# frame, so the uneven spacing reads as stutter.
#
# The script's own update still runs every tick, so its toggle button and
# settings work as before, but this decides which ticks are shown: when
# fast-forwarding, a whole number of ticks per refresh at 60 fps. At 40 fps,
# 3x becomes 2 ticks per refresh and 2x becomes 1 (1.5x), since 80 ticks a
# second can't be spread evenly over 60 refreshes. Games at 60 fps pace the
# same as before.
#
# Graphics.frame_rate and frame_count still behave as if the game ran at its
# own rate, since games derive timings and play time from them.

module SwitchFastForward
	REFRESH = 60

	@show = nil          # the update below the script, captured once
	@script_update = nil # the script's own update
	@game_rate = nil     # Graphics.frame_rate as the game set it
	@stride = 1          # ticks per frame the script asks for, i.e. its speed
	@since = 0
	@ticks = 0
	@count_debt = 0
	@alias_seen = false
	@defining = false

	class << self
		attr_reader :game_rate

		def active?
			!@show.nil?
		end

		# Ticks per frame shown, and frames shown per second
		def pacing
			return [1, Rational(@game_rate)] if @stride <= 1
			per_frame = [Rational(@game_rate * @stride, REFRESH).round, 1].max
			[per_frame, Rational(REFRESH)]
		end

		# For the slow-frame log, e.g. "1.5x"
		def label
			per_frame, shown = pacing
			s = per_frame * shown / @game_rate
			s.denominator == 1 ? "#{s.to_i}x" : "#{s.to_f}x"
		end

		def game_rate=(value)
			@game_rate = [value.to_i, 1].max
			apply_rate
		end

		# Points the engine's frame limiter at the rate frames are shown.
		# Returns ticks per frame shown.
		def apply_rate
			per_frame, shown = pacing
			rate = shown.round
			Graphics.__ff_engine_frame_rate = rate if Graphics.__ff_engine_frame_rate != rate
			per_frame
		end

		def update
			@since += 1
			@script_update.call
			per_frame = apply_rate
			@ticks += 1
			return if @ticks < per_frame
			@ticks = 0
			show_frame
		end

		# Stands in for fast_forward_update inside the script's update
		def script_frame
			@stride = @since
			@since = 0
		end

		# The engine counts every frame shown. Count them at the game's own
		# rate instead, so play time and frame_count-based animations don't
		# run faster at 60 fps.
		def show_frame
			count = Graphics.frame_count
			@show.call
			return unless Graphics.frame_count == count + 1
			@count_debt += Rational(@game_rate) / pacing[1]
			whole = @count_debt.floor
			@count_debt -= whole
			Graphics.frame_count = count + whole if whole != 1
		end

		# Called for every singleton method added to Graphics. The script
		# aliases update to fast_forward_update and then redefines update, and
		# does both again on every script reset.
		def graphics_method_added(name)
			return if @defining
			if name == :fast_forward_update
				@alias_seen = true
			elsif name == :update && @alias_seen
				@alias_seen = false
				install
			end
		end

		# After a reset, fast_forward_update points at this update rather than
		# the engine's, so the original is kept from the first install.
		def install
			@defining = true
			unless @show
				@show = Graphics.method(:fast_forward_update)
				@game_rate = Graphics.frame_rate
				Graphics.singleton_class.class_eval do
					alias_method :__ff_engine_frame_rate, :frame_rate
					alias_method :__ff_engine_frame_rate=, :frame_rate=
					define_method(:frame_rate) { SwitchFastForward.game_rate }
					define_method(:frame_rate=) { |value| SwitchFastForward.game_rate = value }
				end
			end
			@script_update = Graphics.method(:update)
			Graphics.define_singleton_method(:fast_forward_update) { SwitchFastForward.script_frame }
			Graphics.define_singleton_method(:update) { SwitchFastForward.update }
		ensure
			@defining = false
		end
	end
end

module Graphics
	def self.singleton_method_added(name)
		super
		SwitchFastForward.graphics_method_added(name)
	end
end
