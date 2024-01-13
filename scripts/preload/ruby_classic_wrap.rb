# ruby_classic_wrap.rb
# Author: WaywardHeart (2023)

# Creative Commons CC0: To the extent possible under law, WaywardHeart has
# dedicated all copyright and related and neighboring rights to this script
# to the public domain worldwide.
# https://creativecommons.org/publicdomain/zero/1.0/

# This preload script provides functions that existed in RPG Maker's versions of Ruby,
# but were renamed or changed in the current Ruby version used in mkxp-z, so that games
# (or other preload scripts) that expect the older Ruby behavior can function.

class Hash
	alias_method :index, :key unless method_defined?(:index)
end

class Object
	TRUE = true unless const_defined?("TRUE")
	FALSE = false unless const_defined?("FALSE")
	NIL = nil unless const_defined?("NIL")
	
	alias_method :id, :object_id unless method_defined?(:id)
	alias_method :type, :class unless method_defined?(:type)
end

class NilClass
	def id
		4 # Starting with Ruby2, 64-bit builds of ruby make this 8
	end
end

class TrueClass
	def id
		2 # Starting with Ruby2, 64-bit builds of ruby make this 20
	end
end

if defined?(BasicObject) && BasicObject.instance_method(:initialize).arity == 0
	# In ruby 1.9.2, and only ruby 1.9.2, BasicObject.initialize accepted any number of arguments
	class BasicObject
		def initialize(*args)
		end
	end
end
