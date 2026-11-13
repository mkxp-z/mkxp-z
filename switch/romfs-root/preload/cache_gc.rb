# cache_gc.rb
# The engine's RPG Maker XP RPG::Cache.clear empties the image cache and runs
# a full garbage collection so the dropped images get freed. Pokemon
# Essentials calls it on every map transfer and battle start, and on the
# Switch that collection stops the game for about 0.3 s. This starts the same
# full collection incrementally, so Ruby spreads the work over the next
# frames. Games that define their own clear replace this one.

if defined?(RPG::Cache)
	module RPG
		module Cache
			def self.clear
				@cache = {}
				GC.start(full_mark: true, immediate_mark: false, immediate_sweep: false)
			end
		end
	end
end
