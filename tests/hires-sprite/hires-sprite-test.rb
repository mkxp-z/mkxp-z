# Test suite for mkxp-z high-res Bitmap replacement.
# Sprite tests.
# Copyright 2023 Splendide Imaginarius.
# License GPLv2+.
# Test images are from https://github.com/xinntao/Real-ESRGAN/
#
# Run the suite via the "customScript" field in mkxp.json.
# Use RGSS v3 for best results.

def dump2(bmp, spr, desc)
	spr.bitmap = bmp
	Graphics.wait(1)
	#Graphics.wait(5*60)
	#Graphics.screenshot("test-results/" + desc + ".png")
	shot = Graphics.snap_to_bitmap
	shot.to_file("test-results/" + desc + "-lo.png")
	if !shot.hires.nil?
		shot.hires.to_file("test-results/" + desc + "-hi.png")
	end
	System::puts("Finished " + desc)
end

def dump(bmp, spr, desc)
	spr.viewport = nil
	dump2(bmp, spr, desc + "-direct")
    spr.tone.gray = 128
	dump2(bmp, spr, desc + "-directtonegray")
	spr.tone.gray = 0
	$vp.ox = 0
	spr.viewport = $vp
	dump2(bmp, spr, desc + "-viewport")
	$vp.ox = 250
	dump2(bmp, spr, desc + "-viewportshift")
	$vp.ox = 0
	$vp.rect.width = 320
	dump2(bmp, spr, desc + "-viewportsquash")
	$vp.rect.width = 640
	$vp.tone.green = -128
	dump2(bmp, spr, desc + "-viewporttonegreen")
	$vp.tone.green = 0
	$vp.tone.gray = 128
	dump2(bmp, spr, desc + "-viewporttonegray")
	$vp.tone.gray = 0
end

# Setup graphics
Graphics.resize_screen(448, 640)

$vp = Viewport.new()

spr = Sprite.new()

bmp = Bitmap.new("Graphics/Pictures/OST_009-Small")
spr.zoom_x = 1.0
spr.zoom_y = 1.0
dump(bmp, spr, "Small")

bmp = Bitmap.new("Graphics/Pictures/OST_009-Big")
spr.zoom_x = 448.0 / 1792.0
spr.zoom_y = 448.0 / 1792.0
dump(bmp, spr, "Big")

bmp = Bitmap.new("Graphics/Pictures/OST_009")
spr.zoom_x = 1.0
spr.zoom_y = 1.0
dump(bmp, spr, "Substituted")

bmp = Bitmap.new("Graphics/Pictures/OST_009")
spr.zoom_x = 1.5
spr.zoom_y = 1.5
dump(bmp, spr, "Substituted-Zoomed")

bmp = Bitmap.new("Graphics/Pictures/OST_009")
spr.zoom_x = 448.0 / 1792.0
spr.zoom_y = 448.0 / 1792.0
dump(bmp.hires, spr, "Substituted-Explicit")

# Test for null pointer
spr_null = Sprite.new()
spr_null.src_rect = Rect.new(0, 0, 448, 640)

exit
