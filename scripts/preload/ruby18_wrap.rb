# ruby18_wrap.rb
# Author: Splendide Imaginarius (2022)

# Creative Commons CC0: To the extent possible under law, Splendide Imaginarius
# has waived all copyright and related or neighboring rights to ruby18_wrap.rb.
# https://creativecommons.org/publicdomain/zero/1.0/

# This preload script provides functions that existed in RPG Maker's Ruby v1.8,
# but were renamed in the current Ruby version used in mkxp-z, so that games
# (or other preload scripts) that expect Ruby v1.8's function names can find
# them.

class Hash
	def index(*args)
		key(*args)
	end
end
