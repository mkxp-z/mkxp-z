# win32api_stub.rb
# Switch has no native DLLs, so desktop MiniFFI (which normally defines
# Win32API) is not built. This base class behaves like a MiniFFI that failed
# to bind, so win32_wrap.rb (loaded next) can wrap it: wrapped functions use
# its Ruby implementations, everything else returns 0.

class Win32API
	def initialize(dll, func, *args)
		raise RuntimeError, "Win32API is unavailable on Switch (#{dll}:#{func})"
	end

	def call(*args)
		return 0
	end

	def Call(*args)
		return call(*args)
	end
end
