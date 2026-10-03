# kgl2_wrap.rb
# Author: white-axe (2026)

# Creative Commons CC0: To the extent possible under law, white-axe has
# dedicated all copyright and related and neighboring rights to this script
# to the public domain worldwide.
# https://creativecommons.org/publicdomain/zero/1.0/

unless Win32API.method_defined? :kgl2_native_call

require_relative 'win32_wrap' unless Win32API.method_defined? :mkxp_native_call
require 'objspace'

module KGL2_Impl
	class << self
		attr_accessor :initialized
		attr_accessor :light_blending
		attr_accessor :soft_shadows
		attr_accessor :framebuffer
		attr_accessor :shadowbuffer
	end

	self.initialized = false
	self.light_blending = false
	self.soft_shadows = false
	self.framebuffer = nil
	self.shadowbuffer = nil

	class KglVersion
		def call
			200
		end
	end

	# If the library is already initialized, return 102.
	# If the library is not initialized, initialize it and return 1.
	class KglLoad
		def call
			return 102 if KGL2_Impl.initialized
			KGL2_Impl.initialized = true
			1
		end
	end

	# Set the red, green, blue and alpha components of each pixel in the bitmap to zero, then return 1.
	class KglBlank
		def call(bitmap_id)
			bitmap = ObjectSpace._id2ref(bitmap_id)
			bitmap.clear
			1
		end
	end

	# Set each pixel in the bitmap to the given color, then return 1.
	# The color is a 32-bit integer, where
	# the least significant 8 bits are the blue component,
	# the second-least significant 8 bits are the green component,
	# the second-most significant 8 bits are the red component, and
	# the most significant 8 bits are the alpha component.
	class KglClear
		def call(bitmap_id, color_packed)
			bitmap = ObjectSpace._id2ref(bitmap_id)
			color = Color.new((color_packed >> 16) & 0xff, (color_packed >> 8) & 0xff, color_packed & 0xff, (color_packed >> 24) & 0xff)
			bitmap.fill_rect(bitmap.rect, color)
			1
		end
	end

	# Invert the values of the red, green and blue components of each pixel in the bitmap, then return 1.
	class KglInvert
		def call(bitmap_id)
			bitmap = ObjectSpace._id2ref(bitmap_id)
			bitmap._kgl_invert
			1
		end
	end

	# If the two bitmaps do not have the same width and height, return 112.
	# If the two bitmaps have the same width and height, copy the contents of the second bitmap to the first bitmap, then return 1.
	class KglClone
		def call(dst_bitmap_id, src_bitmap_id)
			dst_bitmap = ObjectSpace._id2ref(dst_bitmap_id)
			src_bitmap = ObjectSpace._id2ref(src_bitmap_id)
			rect = dst_bitmap.rect
			return 112 if rect != src_bitmap.rect
			dst_bitmap.clear
			dst_bitmap.blt(0, 0, src_bitmap, rect)
			1
		end
	end

	# If the library is not initialized, return 101.
	# If the library is initialized, record the bitmap as the KGL framebuffer, then return 1.
	class KglBindFramebuffer
		def call(bitmap_id)
			return 101 unless KGL2_Impl.initialized
			bitmap = ObjectSpace._id2ref(bitmap_id)
			KGL2_Impl.framebuffer = bitmap
			1
		end
	end

	# If the library is not initialized, return 101.
	# If the library is initialized, record the bitmap as the KGL shadowbuffer, then return 1.
	class KglBindShadowbuffer
		def call(bitmap_id)
			return 101 unless KGL2_Impl.initialized
			bitmap = ObjectSpace._id2ref(bitmap_id)
			KGL2_Impl.shadowbuffer = bitmap
			1
		end
	end

	# Record the KGL framebuffer as unbound, then return 1.
	class KglUnbindFramebuffer
		def call
			KGL2_Impl.framebuffer = nil
			1
		end
	end

	# Record the KGL shadowbuffer as unbound, then return 1.
	class KglUnbindShadowbuffer
		def call
			KGL2_Impl.shadowbuffer = nil
			1
		end
	end

	# If the KGL framebuffer is unbound, return 103.
	# If the KGL framebuffer is bound, set the red, green, blue and alpha components of each pixel in the KGL framebuffer to zero, then return 1.
	class KglClearFramebuffer
		def call
			return 103 if KGL2_Impl.framebuffer.nil?
			KGL2_Impl.framebuffer.clear
			1
		end
	end

	# Multiply the red, green and blue components of each pixel of the bitmap by its alpha component divided by 255,
	# then set the alpha component of each pixel to 0, then return 1.
	class KglCompressAlpha
		def call(bitmap_id)
			bitmap = ObjectSpace._id2ref(bitmap_id)
			bitmap._kgl_compress_alpha
			1
		end
	end

	# If the argument is nonzero, enable light blending, then return 1.
	# If the argument is zero, disable light blending, then return 1.
	class KglLightBlending
		def call(enabled_integer)
			enabled = enabled_integer && enabled_integer != 0 ? true : false
			KGL2_Impl.light_blending = enabled
			1
		end
	end

	# If the KGL framebuffer is unbound, return 103.
	# Otherwise, perform a pixel-by-pixel subtraction of a region of the bitmap from the KGL framebuffer,
	# and return 1 if the operation succeeded or 111 if it failed due to out-of-bounds x and y values.
	class KglLightShader
		def call(bitmap_id, x, y, opacity)
			return 103 if KGL2_Impl.framebuffer.nil?
			bitmap = ObjectSpace._id2ref(bitmap_id)
			framebuffer_width = KGL2_Impl.framebuffer.width
			framebuffer_height = KGL2_Impl.framebuffer.height
			bitmap_width = bitmap.width
			bitmap_height = bitmap.height
			x = x.to_i
			y = y.to_i
			framebuffer_width -= [x, 0].min
			framebuffer_height -= [y, 0].min
			bitmap_width += [x, 0].max
			bitmap_height += [y, 0].max
			width = [framebuffer_width, bitmap_width].min - x.abs
			height = [framebuffer_height, bitmap_height].min - y.abs
			return 111 if width < 0 || height < 0
			framebuffer_x = [x, 0].max
			framebuffer_y = [y, 0].max
			bitmap_x = -[x, 0].min
			bitmap_y = -[y, 0].min
			opacity = opacity > 100 ? 255 : 0 unless KGL2_Impl.light_blending
			KGL2_Impl.framebuffer._kgl_subtract_rect(
				framebuffer_x,
				framebuffer_y,
				bitmap,
				Rect.new(
					bitmap_x,
					bitmap_y,
					width,
					height,
				),
				opacity,
			)
			1
		end
	end

	# If the argument is nonzero, enable soft shadows, then return 1.
	# If the argument is zero, disable soft shadows, then return 1.
	class KglSoftShadows
		def call(enabled_integer)
			enabled = enabled_integer && enabled_integer != 0 ? true : false
			KGL2_Impl.soft_shadows = enabled
			1
		end
	end

	# If the KGL shadowbuffer is unbound, return 105.
	# If the KGL shadowbuffer is bound but y is less than 0, greater than or equal to the height of the KGL shadowbuffer or
	# exactly equal to half the height of the KGL shadowbuffer rounded down, return 111.
	# Otherwise, cast a shadow of transparent black pixels from an invisible horizontal line segment with the given end points,
	# radially away from the center of the KGL shadowbuffer.
	# If y is less than half the height of the KGL shadowbuffer rounded down, the shadow begins at one less than the y coordinate of the line segment.
	# If y is greater than half the height of the KGL shadowbuffer rounded down, the shadow begins on the y coordinate of the line segment.
	# If the width or height of the shadowbuffer is even, the center is located at the smaller x or y coordinate.
	# If soft shadows are enabled, for edges of the shadow that are not exactly horizontal or vertical,
	# there is an additional horizontal 3 pixel wide zone where the red, green and blue pixel components are
	# linearly interpolated between the original color and black and the alpha component is unchanged.
	# After that, return 1.
	# For example, if the KGL shadowbuffer is initially 50 pixels by 50 pixels opaque white, and
	# this function is called with x1 = 22, x2 = 40 and y = 16 with soft shadows enabled,
	# the result should be the following, where '.' represents fully opaque pixels, '#' represents fully transparent pixels,
	# '3' represents 25% white pixels that are fully opaque, '2' represents 50% white pixels that are fully opaque and '1' represents 75% white pixels that are fully opaque.
	# If soft shadows are disabled, the partially white pixels are instead transparent black.
	#     ..............123#################################
	#     ..............123#################################
	#     ..............123#################################
	#     ...............123################################
	#     ...............123################################
	#     ...............123################################
	#     ................123###############################
	#     ................123###############################
	#     ................123###############################
	#     .................123##############################
	#     .................123##############################
	#     .................123##############################
	#     ..................123###########################32
	#     ..................123#########################321.
	#     ..................123########################321..
	#     ...................123#####################321....
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	#     ..................................................
	class KglShadowShaderH
		def call(x1, x2, y)
			return 105 if KGL2_Impl.shadowbuffer.nil?
			KGL2_Impl.shadowbuffer._kgl_shadow_shader_h(x1, x2, y, KGL2_Impl.soft_shadows)
		end
	end

	# This is the same as ShadowShaderH but with a vertical line instead of a horizontal line.
	class KglShadowShaderV
		def call(y1, y2, x)
			return 105 if KGL2_Impl.shadowbuffer.nil?
			KGL2_Impl.shadowbuffer._kgl_shadow_shader_v(y1, y2, x, false, KGL2_Impl.soft_shadows)
		end
	end

	# This is the same as ShadowShaderV except the part of the shadow where the y coordinate is greater than or equal to
	# half the height of the KGL shadowbuffer rounded down is cast horizontally instead of radially.
	class KglShadowShaderW
		def call(y1, y2, x)
			return 105 if KGL2_Impl.shadowbuffer.nil?
			KGL2_Impl.shadowbuffer._kgl_shadow_shader_v(y1, y2, x, true, KGL2_Impl.soft_shadows)
		end
	end
end

class Win32API
	alias_method :kgl2_native_initialize, :initialize
	def initialize(dll, func, *args)
		@dll = dll
		@func = func

		func[0] = func[0].upcase

		begin
			if (dll == 'KGL2.klib' || dll.end_with?('/KGL2.klib') || dll.end_with?('\KGL2.klib')) && KGL2_Impl.const_defined?(func)
				@kgl2_wrap_impl = KGL2_Impl.const_get(func).new
				return
			end
		rescue Exception
		end

		kgl2_native_initialize(@dll, @func, *args)
	end

	alias_method :kgl2_native_call, :call
	def call(*args)
		if @kgl2_wrap_impl
			return @kgl2_wrap_impl.call(*args)
		end

		return kgl2_native_call(*args)
	end
end

end
