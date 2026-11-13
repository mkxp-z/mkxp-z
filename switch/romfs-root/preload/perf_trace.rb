# perf_trace.rb
# With sdmc:/switch/mkxp-z/debug.txt present, logs every slow frame (over
# 80 ms) to mkxpz_log.txt with where its time went: presenting the previous
# frame (Graphics.update), the slowest engine calls (bitmap drawing, loading,
# transitions, audio, Marshal data), Ruby's garbage collector, and whatever is
# left, which is the game's own Ruby code.

if File.exist?("sdmc:/switch/mkxp-z/debug.txt")
	module SwitchPerf
		SLOW = 0.08

		# Engine calls worth timing. Cheap accessors (x=, opacity=, ...) are
		# left out: they run thousands of times per frame.
		TIMED = {
			Bitmap => %i[initialize initialize_copy dispose blt stretch_blt fill_rect
			             gradient_fill_rect clear clear_rect get_pixel set_pixel
			             hue_change blur radial_blur draw_text text_size raw_data
			             raw_data= to_file],
			Sprite => %i[initialize dispose update flash bitmap=],
			Window => %i[initialize dispose update contents= windowskin=],
			Viewport => %i[initialize dispose update flash],
			Plane => %i[initialize dispose bitmap=],
			Tilemap => %i[initialize dispose update tileset= map_data= priorities=],
		}
		TIMED_SINGLETON = {
			Graphics => %i[freeze transition frame_reset wait fadeout fadein
			               snap_to_bitmap resize_screen play_movie],
			Audio => %i[bgm_play bgm_stop bgm_fade bgs_play bgs_stop bgs_fade
			            me_play me_stop me_fade se_play se_stop],
			Marshal => %i[load dump],
			# Ruby file access. Ruby forces a full GC and retries when opening a
			# file fails for lack of file handles or memory.
			File => %i[open new exist? file? directory? size read binread],
			FileTest => %i[exist? file? directory? size?],
			IO => %i[read binread readlines foreach],
			Dir => %i[glob entries exist? [] mkdir],
		}
		# Calls whose first argument (a path, or Marshal's source) is logged
		DETAILED = /\A(File|FileTest|IO|Dir|Marshal)\.|\Aload_data\z/

		@times = Hash.new(0.0)
		@counts = Hash.new(0)
		@depth = 0
		@slowest_file = nil
		@slowest_file_time = 0.0
		@slowest_call = nil
		@slowest_call_time = 0.0
		@major_in = []
		@attributed_major = 0

		def self.now
			Process.clock_gettime(Process::CLOCK_MONOTONIC)
		end

		def self.describe(key, arg)
			return nil unless key =~ DETAILED
			if arg.respond_to?(:path)
				arg = arg.path
			elsif key.start_with?("Marshal.") && arg.is_a?(String)
				return "#{arg.bytesize / 1024}KB string"
			end
			arg.is_a?(String) ? arg[-60..] || arg : nil
		end

		# Times the outermost timed call only, so a game override that calls
		# other timed methods is not counted twice. A major GC is credited to the
		# innermost timed call it happened in.
		def self.time(key, arg = nil)
			major = GC.stat(:major_gc_count)
			outer = @depth == 0
			@depth += 1
			t = now
			begin
				yield
			ensure
				@depth -= 1
				dt = now - t
				if outer
					@times[key] += dt
					@counts[key] += 1
				end
				detail = describe(key, arg)
				if detail && dt > @slowest_call_time
					@slowest_call_time = dt
					@slowest_call = "#{key}(#{detail})"
				end
				m = GC.stat(:major_gc_count)
				if m > major && m > @attributed_major
					@attributed_major = m
					@major_in << (detail ? "#{key}(#{detail})" : key)
				end
			end
		end

		def self.note_file(name, dt)
			if dt > @slowest_file_time
				@slowest_file_time = dt
				@slowest_file = name
			end
		end

		# Wraps with alias_method, like the games' own scripts do. Module#prepend
		# must not be used here: games alias these methods (Pokemon Essentials
		# aliases Graphics.transition), and aliasing a prepended method sends
		# the call chain back into itself until the stack overflows.
		def self.wrap(owner, names, label)
			names.each do |name|
				next unless owner.method_defined?(name) || owner.private_method_defined?(name)
				orig = :"__perf_#{name}"
				key = "#{label}#{name}"
				owner.send(:alias_method, orig, name)
				if name == :initialize && label == "Bitmap#"
					owner.send(:define_method, name) do |*args, **kw, &blk|
						return send(orig, *args, **kw, &blk) unless args[0].is_a?(String)
						t = SwitchPerf.now
						SwitchPerf.time("Bitmap.new(file)") { send(orig, *args, **kw, &blk) }
						SwitchPerf.note_file(args[0], SwitchPerf.now - t)
					end
				else
					owner.send(:define_method, name) do |*args, **kw, &blk|
						SwitchPerf.time(key, args[0]) { send(orig, *args, **kw, &blk) }
					end
				end
			end
		end

		def self.install
			TIMED.each { |klass, names| wrap(klass, names, "#{klass}#") }
			TIMED_SINGLETON.each do |mod, names|
				wrap(mod.singleton_class, names, "#{mod}.")
			end
		end

		# Why Ruby ran a full (major) GC, and how close each trigger was in the
		# frame before it (before is that frame's GC.stat). Ruby starts one when
		# any of these counts passes its limit, or when the heap has no free
		# slots left ("nofree").
		def self.major_gc_details(before)
			v = ->(key) { before[key].to_i }
			pct = ->(value, limit) { limit > 0 ? value * 100 / limit : 0 }
			format("[perf]   major gc by %s (gc_by %s): old objects %d/%d (%d%%), wb-unprotected %d/%d (%d%%)," \
			       " oldmalloc %dKB/%dKB (%d%%), free slots %d of %d",
			       GC.latest_gc_info(:major_by).inspect, GC.latest_gc_info(:gc_by).inspect,
			       v.(:old_objects), v.(:old_objects_limit),
			       pct.(v.(:old_objects), v.(:old_objects_limit)),
			       v.(:uncollectible_wb_unprotected_objects),
			       v.(:uncollectible_wb_unprotected_objects_limit),
			       pct.(v.(:uncollectible_wb_unprotected_objects),
			            v.(:uncollectible_wb_unprotected_objects_limit)),
			       v.(:oldmalloc_increase_bytes) / 1024, v.(:oldmalloc_increase_bytes_limit) / 1024,
			       pct.(v.(:oldmalloc_increase_bytes), v.(:oldmalloc_increase_bytes_limit)),
			       v.(:heap_free_slots), v.(:heap_available_slots))
		end

		# Called at the start of every Graphics.update; present is how long the
		# previous frame's Graphics.update took.
		def self.frame(present)
			t = now
			gc = GC.stat
			if @last && t - @last > SLOW
				total = t - @last
				timed = @times.values.sum
				scene = $scene ? $scene.class.name : "-"
				ff = defined?(SwitchFastForward) && SwitchFastForward.active?
				speed = ff ? " speed=#{SwitchFastForward.label}" : ""
				top = @times.sort_by { |_, v| -v }.first(4).map do |k, v|
					format("%s=%dms(%d)", k, v * 1000, @counts[k])
				end
				line = format("[perf] slow frame %dms scene=%s%s present=%dms ruby=%dms gc=%d(major %d)" \
				              " live=%d | %s",
				              total * 1000, scene, speed, present * 1000,
				              [total - present - timed, 0].max * 1000,
				              gc[:count] - @gc[:count], gc[:major_gc_count] - @gc[:major_gc_count],
				              gc[:heap_live_slots], top.join(" "))
				line << format(" slowest_file=%s(%dms)", @slowest_file, @slowest_file_time * 1000) if @slowest_file
				line << format(" slowest_call=%s=%dms", @slowest_call, @slowest_call_time * 1000) if @slowest_call
				line << " major_gc_in=#{@major_in.join(",")}" unless @major_in.empty?
				puts line
				puts major_gc_details(@gc) if gc[:major_gc_count] > @gc[:major_gc_count]
			end
			@last = t
			@gc = gc
			@times.clear
			@counts.clear
			@slowest_file = nil
			@slowest_file_time = 0.0
			@slowest_call = nil
			@slowest_call_time = 0.0
			@major_in.clear
		end
	end

	SwitchPerf.install

	alias __perf_load_data load_data

	def load_data(*args)
		SwitchPerf.time("load_data", args[0]) { __perf_load_data(*args) }
	end

	# Who asks for full collections explicitly
	[[GC, :start], [ObjectSpace, :garbage_collect]].each do |mod, name|
		mod.singleton_class.send(:alias_method, :"__perf_#{name}", name)
		mod.singleton_class.send(:define_method, name) do |*args, **kw|
			from = caller_locations(1, 3).map { |l| "#{l.path}:#{l.lineno}:#{l.label}" }.join(" < ")
			puts "[perf] #{mod}.#{name} called from #{from}"
			send(:"__perf_#{name}", *args, **kw)
		end
	end

	class << Graphics
		alias_method :__perf_update, :update

		def update
			SwitchPerf.frame(@__perf_present || 0.0)
			t = SwitchPerf.now
			__perf_update
		ensure
			@__perf_present = SwitchPerf.now - t if t
		end
	end

	puts "[perf] tracing slow frames"
end
