# Test suite for mkxp-z stretch_blt.
# Copyright 2024 Splendide Imaginarius.
# License GPLv2+.
# Test images are from https://github.com/xinntao/Real-ESRGAN/
#
# Run the suite via the "customScript" field in mkxp.json.
# Use RGSS v3 for best results.

def dump(bmp, spr, desc)
	spr.bitmap = bmp
	Graphics.wait(1)
	bmp.to_file("test-results/" + desc + ".png")
	System::puts("Finished " + desc)
end

# Setup graphics
Graphics.resize_screen(1920, 1080)

# Setup font
fnt = Font.new("Liberation Sans", 100)

# Setup splash screen
bmp = Bitmap.new(1920, 1080)
bmp.fill_rect(0, 0, 1920, 1080, Color.new(0, 0, 0))

bmp.font = fnt
bmp.draw_text(0, 0, 1920, 540, "stretch_blt Test Suite", 1)
bmp.draw_text(0, 540, 1920, 540, "Starting Now", 1)

spr = Sprite.new()
spr.bitmap = bmp

Graphics.wait(1 * 60)

# Tests start here

foreground = Bitmap.new("Graphics/Pictures/OST_009")
dump(foreground, spr, "foreground")

background = Bitmap.new(1920, 1080)
background.clear
dump(background, spr, "background")

composite = background.dup
composite.stretch_blt(Rect.new(0, 0, 1920, 1080), foreground, Rect.new(0, 0, 1920, 1080))
dump(composite, spr, "composite")

# Tests are finished, show exit screen

bmp = Bitmap.new(1920, 1080)
bmp.fill_rect(0, 0, 1920, 1080, Color.new(0, 0, 0))

fnt = Font.new("Liberation Sans", 100)

bmp.font = fnt
bmp.draw_text(0, 0, 1920, 540, "stretch_blt Test Suite", 1)
bmp.draw_text(0, 540, 1920, 540, "Has Finished", 1)
spr.bitmap = bmp

Graphics.wait(1 * 60)

exit
