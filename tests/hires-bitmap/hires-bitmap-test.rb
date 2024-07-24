# Test suite for mkxp-z high-res Bitmap replacement.
# Bitmap tests.
# Copyright 2023 Splendide Imaginarius.
# License GPLv2+.
# Test images are from https://github.com/xinntao/Real-ESRGAN/
#
# Run the suite via the "customScript" field in mkxp.json.
# Use RGSS v3 for best results.

def dump(bmp, spr, desc)
	spr.bitmap = bmp
	Graphics.wait(1)
	bmp.to_file("test-results/" + desc + "-lo.png")
	if !bmp.hires.nil?
		bmp.hires.to_file("test-results/" + desc + "-hi.png")
	end
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
bmp.draw_text(0, 0, 640, 240, "High-Res Test Suite", 1)
bmp.draw_text(0, 240, 640, 240, "Starting Now", 1)

spr = Sprite.new()
spr.bitmap = bmp

Graphics.wait(1 * 60)

# Tests start here

bmp = Bitmap.new("Graphics/Pictures/children-alpha")
dump(bmp, spr, "constructor-filename")

# TODO: Filename GIF constructor

bmp = Bitmap.new(640, 480)
bmp.clear
dump(bmp, spr, "constructor-dimensions")

# TODO: Animation constructor

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit")
bmp.clear
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha-lo")
bmp.stretch_blt(bmp.rect, bmp2, bmp2.rect)
dump(bmp, spr, "stretch-blt-clear-tree-lo-children-full-opaque")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit")
bmp.clear
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha-lo")
bmp.stretch_blt(bmp.rect, bmp2, bmp2.rect, 127)
dump(bmp, spr, "stretch-blt-clear-tree-lo-children-full-semitransparent")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit")
bmp.clear
rect = bmp.rect
rect.width /= 2
rect.height /= 2
rect.x = rect.width
rect.y = rect.height
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha-lo")
rect2 = bmp2.rect
rect2.width /= 2
rect2.height /= 2
rect2.x = rect2.width
bmp.stretch_blt(rect, bmp2, rect2, 127)
dump(bmp, spr, "stretch-blt-clear-tree-lo-children-quarter-semitransparent")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit")
bmp.clear
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha")
bmp.stretch_blt(bmp.rect, bmp2, bmp2.rect)
dump(bmp, spr, "stretch-blt-clear-tree-hi-children-full-opaque")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit")
bmp.clear
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha")
bmp.stretch_blt(bmp.rect, bmp2, bmp2.rect, 127)
dump(bmp, spr, "stretch-blt-clear-tree-hi-children-full-semitransparent")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit")
bmp.clear
rect = bmp.rect
rect.width /= 2
rect.height /= 2
rect.x = rect.width
rect.y = rect.height
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha")
rect2 = bmp2.rect
rect2.width /= 2
rect2.height /= 2
rect2.x = rect2.width
bmp.stretch_blt(rect, bmp2, rect2, 127)
dump(bmp, spr, "stretch-blt-clear-tree-hi-children-quarter-semitransparent")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit")
bmp.fill_rect(bmp.rect, Color.new(0, 0, 0))
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha-lo")
bmp.stretch_blt(bmp.rect, bmp2, bmp2.rect)
dump(bmp, spr, "stretch-blt-black-tree-lo-children-full-opaque")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit")
bmp.fill_rect(bmp.rect, Color.new(0, 0, 0))
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha-lo")
bmp.stretch_blt(bmp.rect, bmp2, bmp2.rect, 127)
dump(bmp, spr, "stretch-blt-black-tree-lo-children-full-semitransparent")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit")
bmp.fill_rect(bmp.rect, Color.new(0, 0, 0))
rect = bmp.rect
rect.width /= 2
rect.height /= 2
rect.x = rect.width
rect.y = rect.height
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha-lo")
rect2 = bmp2.rect
rect2.width /= 2
rect2.height /= 2
rect2.x = rect2.width
bmp.stretch_blt(rect, bmp2, rect2, 127)
dump(bmp, spr, "stretch-blt-black-tree-lo-children-quarter-semitransparent")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit")
bmp.fill_rect(bmp.rect, Color.new(0, 0, 0))
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha")
bmp.stretch_blt(bmp.rect, bmp2, bmp2.rect)
dump(bmp, spr, "stretch-blt-black-tree-hi-children-full-opaque")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit")
bmp.fill_rect(bmp.rect, Color.new(0, 0, 0))
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha")
bmp.stretch_blt(bmp.rect, bmp2, bmp2.rect, 127)
dump(bmp, spr, "stretch-blt-black-tree-hi-children-full-semitransparent")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit")
bmp.fill_rect(bmp.rect, Color.new(0, 0, 0))
rect = bmp.rect
rect.width /= 2
rect.height /= 2
rect.x = rect.width
rect.y = rect.height
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha")
rect2 = bmp2.rect
rect2.width /= 2
rect2.height /= 2
rect2.x = rect2.width
bmp.stretch_blt(rect, bmp2, rect2, 127)
dump(bmp, spr, "stretch-blt-black-tree-hi-children-quarter-semitransparent")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit-lo")
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha-lo")
bmp.stretch_blt(bmp.rect, bmp2, bmp2.rect)
dump(bmp, spr, "stretch-blt-lo-tree-lo-children-full-opaque")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit-lo")
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha-lo")
bmp.stretch_blt(bmp.rect, bmp2, bmp2.rect, 127)
dump(bmp, spr, "stretch-blt-lo-tree-lo-children-full-semitransparent")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit-lo")
rect = bmp.rect
rect.width /= 2
rect.height /= 2
rect.x = rect.width
rect.y = rect.height
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha-lo")
rect2 = bmp2.rect
rect2.width /= 2
rect2.height /= 2
rect2.x = rect2.width
bmp.stretch_blt(rect, bmp2, rect2, 127)
dump(bmp, spr, "stretch-blt-lo-tree-lo-children-quarter-semitransparent")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit-lo")
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha")
bmp.stretch_blt(bmp.rect, bmp2, bmp2.rect)
dump(bmp, spr, "stretch-blt-lo-tree-hi-children-full-opaque")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit-lo")
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha")
bmp.stretch_blt(bmp.rect, bmp2, bmp2.rect, 127)
dump(bmp, spr, "stretch-blt-lo-tree-hi-children-full-semitransparent")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit-lo")
rect = bmp.rect
rect.width /= 2
rect.height /= 2
rect.x = rect.width
rect.y = rect.height
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha")
rect2 = bmp2.rect
rect2.width /= 2
rect2.height /= 2
rect2.x = rect2.width
bmp.stretch_blt(rect, bmp2, rect2, 127)
dump(bmp, spr, "stretch-blt-lo-tree-hi-children-quarter-semitransparent")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit")
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha-lo")
bmp.stretch_blt(bmp.rect, bmp2, bmp2.rect)
dump(bmp, spr, "stretch-blt-hi-tree-lo-children-full-opaque")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit")
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha-lo")
bmp.stretch_blt(bmp.rect, bmp2, bmp2.rect, 127)
dump(bmp, spr, "stretch-blt-hi-tree-lo-children-full-semitransparent")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit")
rect = bmp.rect
rect.width /= 2
rect.height /= 2
rect.x = rect.width
rect.y = rect.height
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha-lo")
rect2 = bmp2.rect
rect2.width /= 2
rect2.height /= 2
rect2.x = rect2.width
bmp.stretch_blt(rect, bmp2, rect2, 127)
dump(bmp, spr, "stretch-blt-hi-tree-lo-children-quarter-semitransparent")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit")
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha")
bmp.stretch_blt(bmp.rect, bmp2, bmp2.rect)
dump(bmp, spr, "stretch-blt-hi-tree-hi-children-full-opaque")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit")
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha")
bmp.stretch_blt(bmp.rect, bmp2, bmp2.rect, 127)
dump(bmp, spr, "stretch-blt-hi-tree-hi-children-full-semitransparent")

bmp = Bitmap.new("Graphics/Pictures/tree_alpha_16bit")
rect = bmp.rect
rect.width /= 2
rect.height /= 2
rect.x = rect.width
rect.y = rect.height
bmp2 = Bitmap.new("Graphics/Pictures/children-alpha")
rect2 = bmp2.rect
rect2.width /= 2
rect2.height /= 2
rect2.x = rect2.width
bmp.stretch_blt(rect, bmp2, rect2, 127)
dump(bmp, spr, "stretch-blt-hi-tree-hi-children-quarter-semitransparent")

bmp = Bitmap.new(640, 480)
bmp.fill_rect(100, 200, 450, 300, Color.new(0, 0, 0))
bmp.fill_rect(50, 100, 220, 150, Color.new(255, 0, 0))
dump(bmp, spr, "fill-rect")

bmp = Bitmap.new(640, 480)
bmp.gradient_fill_rect(100, 200, 450, 300, Color.new(0, 0, 0), Color.new(0, 0, 255))
bmp.gradient_fill_rect(50, 100, 220, 150, Color.new(255, 0, 0), Color.new(255, 255, 0))
dump(bmp, spr, "gradient-fill-rect-horizontal")

bmp = Bitmap.new(640, 480)
bmp.gradient_fill_rect(100, 200, 450, 300, Color.new(0, 0, 0), Color.new(0, 0, 255), true)
bmp.gradient_fill_rect(50, 100, 220, 150, Color.new(255, 0, 0), Color.new(255, 255, 0), true)
dump(bmp, spr, "gradient-fill-rect-vertical")

bmp = Bitmap.new("Graphics/Pictures/children-alpha-lo")
bmp.clear_rect(300, 175, 100, 150)
dump(bmp, spr, "clear-rect-lo-children")

bmp = Bitmap.new("Graphics/Pictures/children-alpha")
bmp.clear_rect(300, 175, 100, 150)
dump(bmp, spr, "clear-rect-hi-children")

# TODO: linear-blur is arguably passing but maybe should have stronger blur?
bmp = Bitmap.new("Graphics/Pictures/children-alpha")
bmp.blur
dump(bmp, spr, "linear-blur")

bmp = Bitmap.new("Graphics/Pictures/children-alpha-lo")
bmp.radial_blur(0, 10)
dump(bmp, spr, "radial-blur-0-lo-children")

bmp = Bitmap.new("Graphics/Pictures/children-alpha")
bmp.radial_blur(0, 10)
dump(bmp, spr, "radial-blur-0-hi-children")

bmp = Bitmap.new("Graphics/Pictures/children-alpha-lo")
bmp.radial_blur(3, 10)
dump(bmp, spr, "radial-blur-3-lo-children")

bmp = Bitmap.new("Graphics/Pictures/children-alpha")
bmp.radial_blur(3, 10)
dump(bmp, spr, "radial-blur-3-hi-children")

bmp = Bitmap.new("Graphics/Pictures/children-alpha")
bmp.clear
dump(bmp, spr, "clear-full")

bmp2 = Bitmap.new("Graphics/Pictures/children-alpha")
bmp = Bitmap.new(bmp2.width, bmp2.height)
for x in (0...bmp2.width)
	for y in (0...bmp2.height)
		bmp.set_pixel(x, y, bmp2.get_pixel(x, y))
	end
end
dump(bmp, spr, "get-set-pixel-dimensions")

bmp2 = Bitmap.new("Graphics/Pictures/children-alpha")
bmp = Bitmap.new("Graphics/Pictures/children-alpha")
bmp.clear
for x in (0...bmp2.width)
	for y in (0...bmp2.height)
		bmp.set_pixel(x, y, bmp2.get_pixel(x, y))
	end
end
dump(bmp, spr, "get-set-pixel-clear")

bmp2 = Bitmap.new("Graphics/Pictures/children-alpha")
bmp = Bitmap.new("Graphics/Pictures/children-alpha")
bmp.hires.clear
for x in (0...bmp2.hires.width)
	for y in (0...bmp2.hires.height)
		bmp.hires.set_pixel(x, y, bmp2.hires.get_pixel(x, y))
	end
end
dump(bmp, spr, "get-set-pixel-direct")

bmp = Bitmap.new("Graphics/Pictures/children-alpha-lo")
bmp.hue_change(180)
dump(bmp, spr, "hue-change-lo-children")

bmp = Bitmap.new("Graphics/Pictures/children-alpha")
bmp.hue_change(180)
dump(bmp, spr, "hue-change-hi-children")

bmp = Bitmap.new(640, 480)
fnt = Font.new("Liberation Sans", 100)
bmp.font = fnt
bmp.draw_text(100, 200, 450, 300, "We <3 Real-ESRGAN", 1)
dump(bmp, spr, "draw-text-plain")

bmp = Bitmap.new(640, 480)
fnt = Font.new("Liberation Sans", 15)
bmp.font = fnt
bmp.draw_text(100, 200, 450, 300, "We <3 Real-ESRGAN", 0)
dump(bmp, spr, "draw-text-left")

bmp = Bitmap.new(640, 480)
fnt = Font.new("Liberation Sans", 15)
bmp.font = fnt
bmp.draw_text(100, 200, 450, 300, "We <3 Real-ESRGAN", 2)
dump(bmp, spr, "draw-text-right")

bmp = Bitmap.new(640, 480)
fnt = Font.new("Liberation Sans", 100)
fnt.bold = true
bmp.font = fnt
bmp.draw_text(100, 200, 450, 300, "We <3 Real-ESRGAN", 1)
dump(bmp, spr, "draw-text-bold")

bmp = Bitmap.new(640, 480)
fnt = Font.new("Liberation Sans", 100)
fnt.italic = true
bmp.font = fnt
bmp.draw_text(100, 200, 450, 300, "We <3 Real-ESRGAN", 1)
dump(bmp, spr, "draw-text-italic")

bmp = Bitmap.new(640, 480)
fnt = Font.new("Liberation Sans", 100)
fnt.color = Color.new(255, 0, 0)
fnt.outline = false
bmp.font = fnt
bmp.draw_text(100, 200, 450, 300, "We <3 Real-ESRGAN", 1)
dump(bmp, spr, "draw-text-red-no-outline")

bmp = Bitmap.new(640, 480)
fnt = Font.new("Liberation Sans", 100)
fnt.color = Color.new(255, 127, 127)
fnt.shadow = true
bmp.font = fnt
bmp.draw_text(100, 200, 450, 300, "We <3 Real-ESRGAN", 1)
dump(bmp, spr, "draw-text-pink-shadow")

bmp = Bitmap.new(640, 480)
fnt = Font.new("Liberation Sans", 100)
fnt.out_color = Color.new(0, 255, 0)
bmp.font = fnt
bmp.draw_text(100, 200, 450, 300, "We <3 Real-ESRGAN", 1)
dump(bmp, spr, "draw-text-green-outline")

# TODO: Animation tests, if we can find a good way to test them.

bmp = Bitmap.new(640, 480)
fnt = Font.new("Liberation Sans", 100)
fnt.out_color = Color.new(0, 255, 0)
bmp.font = fnt
bmp.draw_text(100, 200, 450, 300, "Now testing dispose for memory leaks...", 1)
dump(bmp, spr, "memory-leak")

def allocate()
	bmp_allocate = Bitmap.new("Graphics/Pictures/children-alpha")
	bmp_allocate.dispose
end

for i in (1...100)
	for j in (1...10)
		allocate
	end
	System::puts("Memory leak test #{i}/100")
end

# Tests are finished, show exit screen

bmp = Bitmap.new(640, 480)
bmp.fill_rect(0, 0, 640, 480, Color.new(0, 0, 0))

fnt = Font.new("Liberation Sans", 100)

bmp.font = fnt
bmp.draw_text(0, 0, 640, 240, "High-Res Test Suite", 1)
bmp.draw_text(0, 240, 640, 240, "Has Finished", 1)
spr.bitmap = bmp

Graphics.wait(1 * 60)

exit
