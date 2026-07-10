# Test suite for mkxp-z Mega Bitmaps.
# Bitmap tests.
# Copyright 2023-2026 Splendide Imaginarius.
# License GPLv2+.
# Test images are from https://github.com/xinntao/Real-ESRGAN/
#
# Run the suite via the "customScript" field in mkxp.json.
# Use RGSS v3 for best results.
# Set "maxTextureSize": 8192 during testing to force Mega Surface use.

def dump(bmp, spr, desc)
	spr.bitmap = bmp
	Graphics.wait(1 * 60)
	bmp.to_file("test-results/" + desc + ".png")
	System::puts("Finished " + desc)
end

# Setup graphics
Graphics.resize_screen(640, 480)

# Setup font
fnt = Font.new("Liberation Sans", 100)

# Setup splash screen
bmp = Bitmap.new(640, 480)
bmp.fill_rect(0, 0, 640, 480, Color.new(0, 0, 0))

bmp.font = fnt
bmp.draw_text(0, 0, 640, 240, "Mega Bitmap Test Suite", 1)
bmp.draw_text(0, 240, 640, 240, "Starting Now", 1)

spr = Sprite.new()
spr.bitmap = bmp

Graphics.wait(1 * 60)

# Tests start here

# TODO: set to 32767 and 65535 to test https://github.com/libsdl-org/SDL/issues/8878
#large_dim = 65535
#large_dim = 32767
large_dim = 16383

# stretch_blt

bmp_small = Bitmap.new("Graphics/Pictures/children-alpha")
bmp_large = Bitmap.new(large_dim, bmp_small.height * large_dim / bmp_small.width)
bmp_large.stretch_blt(bmp_large.rect, bmp_small, bmp_small.rect)
dump(bmp_large, spr, "stretch-blt-mega-target")

bmp_small_2 = Bitmap.new(bmp_small.width, bmp_small.height)
bmp_small_2.stretch_blt(bmp_small_2.rect, bmp_large, bmp_large.rect)
dump(bmp_small_2, spr, "stretch-blt-mega-source")

# fill_rect

bmp_large.fill_rect(bmp_large.width*1/10, bmp_large.height*2/10, bmp_large.width*3/10, bmp_large.height*4/10, Color.new(255, 0, 255))
dump(bmp_large, spr, "fill-rect-mega")

bmp_small_2.fill_rect(bmp_small_2.width*1/10, bmp_small_2.height*2/10, bmp_small_2.width*3/10, bmp_small_2.height*4/10, Color.new(255, 0, 255))
dump(bmp_small_2, spr, "fill-rect-small")

# gradient_fill_rect

bmp_large.gradient_fill_rect(bmp_large.width*6/10, bmp_large.height*4/10, bmp_large.width*3/10, bmp_large.height*4/10, Color.new(0, 255, 0), Color.new(0, 0, 255))
dump(bmp_large, spr, "gradient-fill-rect-horizontal-mega")

bmp_small_2.gradient_fill_rect(bmp_small_2.width*6/10, bmp_small_2.height*4/10, bmp_small_2.width*3/10, bmp_small_2.height*4/10, Color.new(0, 255, 0), Color.new(0, 0, 255))
dump(bmp_small_2, spr, "gradient-fill-rect-horizontal-small")

bmp_large.gradient_fill_rect(bmp_large.width*6/10, bmp_large.height*4/10, bmp_large.width*3/10, bmp_large.height*4/10, Color.new(0, 255, 0), Color.new(0, 0, 255), true)
dump(bmp_large, spr, "gradient-fill-rect-vertical-mega")

bmp_small_2.gradient_fill_rect(bmp_small_2.width*6/10, bmp_small_2.height*4/10, bmp_small_2.width*3/10, bmp_small_2.height*4/10, Color.new(0, 255, 0), Color.new(0, 0, 255), true)
dump(bmp_small_2, spr, "gradient-fill-rect-vertical-small")

# clone, clear

bmp_large_2 = bmp_large.clone
dump(bmp_large_2, spr, "clone-mega")

bmp_small_3 = bmp_small_2.clone
dump(bmp_small_3, spr, "clone-small")

bmp_large_2.clear
dump(bmp_large_2, spr, "clear-mega")

bmp_small_3.clear
dump(bmp_small_3, spr, "clear-small")

# get_pixel, set_pixel

for x in (0...bmp_large_2.width)
	for y in (0...bmp_large_2.height)
		bmp_large_2.set_pixel(x, y, bmp_large.get_pixel(x, y))
	end
end
dump(bmp_large_2, spr, "get-set-pixel-mega")

for x in (0...bmp_small_3.width)
	for y in (0...bmp_small_3.height)
		bmp_small_3.set_pixel(x, y, bmp_small_2.get_pixel(x, y))
	end
end
dump(bmp_small_3, spr, "get-set-pixel-small")

# hue_change

bmp_large_2.hue_change(180)
dump(bmp_large_2, spr, "hue-change-mega")

bmp_small_3.hue_change(180)
dump(bmp_small_3, spr, "hue-change-small")

# blur

bmp_large.blur
dump(bmp_large, spr, "blur-mega")

bmp_small_2.blur
dump(bmp_small_2, spr, "blur-small")

# Tests are finished, show exit screen

bmp = Bitmap.new(640, 480)
bmp.fill_rect(0, 0, 640, 480, Color.new(0, 0, 0))

fnt = Font.new("Liberation Sans", 100)

bmp.font = fnt
bmp.draw_text(0, 0, 640, 240, "Mega Bitmap Test Suite", 1)
bmp.draw_text(0, 240, 640, 240, "Has Finished", 1)
spr.bitmap = bmp

Graphics.wait(1 * 60)

exit
