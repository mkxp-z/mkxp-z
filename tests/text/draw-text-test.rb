# Test suite for mkxp-z draw_text.
# Copyright 2025 Splendide Imaginarius.
# License LGPLv2+.
#
# Run the suite via the "customScript" field in mkxp.json.
# Use RGSS v3 for best results. Was compared with Enterbrain VX Ace Runtime.

def dump(bmp, spr, desc)
	spr.bitmap = bmp
	Graphics.wait(5 * 60)

	# Comment out these 6 lines for Enterbrain runtime
	if bmp.hires.nil?
		bmp.to_file("test-results/" + desc + ".png")
	else
		bmp.hires.to_file("test-results/" + desc + ".png")
	end
	System::puts("Finished " + desc)
end

# Setup graphics
Graphics.resize_screen(640, 480)

# Setup font
fnt = Font.new("Liberation Sans", 72)
fnt_small = Font.new("Liberation Sans", Font.default_size)
fnt_mashq = Font.new("Mashq", 15)
fnt_mashq.bold = true
fnt_mashq.italic = false
fnt_mashq.outline = true
fnt_mashq.shadow = true


# Setup splash screen
bmp = Bitmap.new(640, 480)
bmp.fill_rect(0, 0, 640, 480, Color.new(0, 0, 0))

bmp.font = fnt
bmp.draw_text(0, 0, 640, 480/3, "draw_text Test Suite", 1)
bmp.draw_text(0, 480/3, 640, 480/3, "Starting Now", 1)
bmp.draw_text(0, 2*480/3, 640, 480/3, "(Make sure the fonts are installed!)", 1)

spr = Sprite.new()
spr.bitmap = bmp

Graphics.wait(5 * 60)

# Tests start here

bmp = Bitmap.new(640, 480)
bmp.fill_rect(0, 0, 640, 480, Color.new(0, 0, 0))
bmp.font = fnt
bmp.draw_text(0, 0, 640, 240, "Forwards", 1)
bmp.draw_text(0, 240, 640, 240, "Forwards", 1)
dump(bmp, spr, "forwards")

bmp = Bitmap.new(640, 480)
bmp.fill_rect(0, 0, 640, 480, Color.new(0, 0, 0))
bmp.font = fnt
bmp.draw_text(0, 0, 640, 240, "Negative Width", 1)
bmp.draw_text(640, 240, -640, 240, "Negative Width", 1)
dump(bmp, spr, "negative-width")

bmp = Bitmap.new(640, 480)
bmp.fill_rect(0, 0, 640, 480, Color.new(0, 0, 0))
bmp.font = fnt
bmp.draw_text(0, 0, 640, 240, "Negative Height", 1)
bmp.draw_text(0, 480, 640, -240, "Negative Height", 1)
dump(bmp, spr, "negative-height")

bmp = Bitmap.new(640, 480)
bmp.fill_rect(0, 0, 640, 480, Color.new(0, 0, 0))
bmp.font = fnt_small
bmp.draw_text(0, 0, 640, 240, "Character by Character", 1)
s = "The quick brown fox jumps over the lazy dog"
pos = Rect.new(0, 240, 640, 240)
bmp.draw_text(pos, s)
pos.y += 20
s.each_char do |char|
	size = bmp.text_size(char)
	bmp.draw_text(pos, char)
	pos.x += size.width
end
dump(bmp, spr, "char-by-char")

bmp = Bitmap.new(640, 480)
bmp.fill_rect(0, 0, 640, 480, Color.new(0, 0, 0))
bmp.font = fnt_mashq
bmp.draw_text(0, 0, 640, 240, "Character by Character (Mashq)", 1)
s = "The quick brown fox jumps over the lazy dog"
pos = Rect.new(0, 240, 640, 240)
bmp.draw_text(pos, s)
pos.y += 20
s.each_char do |char|
	size = bmp.text_size(char)
	bmp.draw_text(pos, char)
	pos.x += size.width
end
dump(bmp, spr, "char-by-char-mashq")

bmp = Bitmap.new(640, 480)
bmp.fill_rect(0, 0, 640, 480, Color.new(255, 0, 255))
fnt.outline = false
fnt.shadow = false
bmp.font = fnt
bmp.draw_text(0, 0, 640, 240, "Outline False / Shadow False", 1)
bmp.draw_text(0, 240, 640, 240, "Hello World", 1)
fnt.outline = Font.default_outline
fnt.shadow = Font.default_shadow
dump(bmp, spr, "outline-false-shadow-false")

bmp = Bitmap.new(640, 480)
bmp.fill_rect(0, 0, 640, 480, Color.new(255, 0, 255))
fnt.color = Color.new(255, 255, 255, 50)
fnt.outline = false
fnt.shadow = false
bmp.font = fnt
bmp.draw_text(0, 0, 640, 240, "Outline False / Shadow False / Trans", 1)
bmp.draw_text(0, 240, 640, 240, "Hello World", 1)
fnt.color = Font.default_color
fnt.outline = Font.default_outline
fnt.shadow = Font.default_shadow
dump(bmp, spr, "outline-false-shadow-false-trans")

bmp = Bitmap.new(640, 480)
bmp.fill_rect(0, 0, 640, 480, Color.new(255, 0, 255))
fnt.outline = true
fnt.shadow = false
bmp.font = fnt
bmp.draw_text(0, 0, 640, 240, "Outline True / Shadow False", 1)
bmp.draw_text(0, 240, 640, 240, "Hello World", 1)
fnt.outline = Font.default_outline
fnt.shadow = Font.default_shadow
dump(bmp, spr, "outline-true-shadow-false")

bmp = Bitmap.new(640, 480)
bmp.fill_rect(0, 0, 640, 480, Color.new(255, 0, 255))
fnt.outline = true
fnt.out_color = Color.new(0, 255, 0, 128)
fnt.shadow = false
bmp.font = fnt
bmp.draw_text(0, 0, 640, 240, "Outline Green / Shadow False", 1)
bmp.draw_text(0, 240, 640, 240, "Hello World", 1)
fnt.outline = Font.default_outline
fnt.out_color = Font.default_out_color
fnt.shadow = Font.default_shadow
dump(bmp, spr, "outline-green-shadow-false")

bmp = Bitmap.new(640, 480)
bmp.fill_rect(0, 0, 640, 480, Color.new(255, 0, 255))
fnt.color = Color.new(255, 255, 255, 50)
fnt.outline = true
fnt.out_color = Color.new(0, 255, 0, 128)
fnt.shadow = false
bmp.font = fnt
bmp.draw_text(0, 0, 640, 240, "Outline Green / Shadow False / Trans", 1)
bmp.draw_text(0, 240, 640, 240, "Hello World", 1)
fnt.color = Font.default_color
fnt.outline = Font.default_outline
fnt.out_color = Font.default_out_color
fnt.shadow = Font.default_shadow
dump(bmp, spr, "outline-green-shadow-false-trans")

bmp = Bitmap.new(640, 480)
bmp.fill_rect(0, 0, 640, 480, Color.new(255, 0, 255))
fnt.outline = false
fnt.shadow = true
bmp.font = fnt
bmp.draw_text(0, 0, 640, 240, "Outline False / Shadow True", 1)
bmp.draw_text(0, 240, 640, 240, "Hello World", 1)
fnt.outline = Font.default_outline
fnt.shadow = Font.default_shadow
dump(bmp, spr, "outline-false-shadow-true")

bmp = Bitmap.new(640, 480)
bmp.fill_rect(0, 0, 640, 480, Color.new(255, 0, 255))
fnt.color = Color.new(255, 255, 255, 50)
fnt.outline = false
fnt.shadow = true
bmp.font = fnt
bmp.draw_text(0, 0, 640, 240, "Outline False / Shadow True / Trans", 1)
bmp.draw_text(0, 240, 640, 240, "Hello World", 1)
fnt.color = Font.default_color
fnt.outline = Font.default_outline
fnt.shadow = Font.default_shadow
dump(bmp, spr, "outline-false-shadow-true-trans")

bmp = Bitmap.new(640, 480)
bmp.fill_rect(0, 0, 640, 480, Color.new(255, 0, 255))
fnt.outline = true
fnt.shadow = true
bmp.font = fnt
bmp.draw_text(0, 0, 640, 240, "Outline True / Shadow True", 1)
bmp.draw_text(0, 240, 640, 240, "Hello World", 1)
fnt.outline = Font.default_outline
fnt.shadow = Font.default_shadow
dump(bmp, spr, "outline-true-shadow-true")

bmp = Bitmap.new(640, 480)
bmp.fill_rect(0, 0, 640, 480, Color.new(255, 0, 255))
fnt.outline = true
fnt.out_color = Color.new(0, 255, 0, 128)
fnt.shadow = true
bmp.font = fnt
bmp.draw_text(0, 0, 640, 240, "Outline Green / Shadow True", 1)
bmp.draw_text(0, 240, 640, 240, "Hello World", 1)
fnt.outline = Font.default_outline
fnt.out_color = Font.default_out_color
fnt.shadow = Font.default_shadow
dump(bmp, spr, "outline-green-shadow-true")

bmp = Bitmap.new(640, 480)
bmp.fill_rect(0, 0, 640, 480, Color.new(255, 0, 255))
fnt.color = Color.new(255, 255, 255, 50)
fnt.outline = true
fnt.out_color = Color.new(0, 255, 0, 128)
fnt.shadow = true
bmp.font = fnt
bmp.draw_text(0, 0, 640, 240, "Outline Green / Shadow True / Trans", 1)
bmp.draw_text(0, 240, 640, 240, "Hello World", 1)
fnt.color = Font.default_color
fnt.outline = Font.default_outline
fnt.out_color = Font.default_out_color
fnt.shadow = Font.default_shadow
dump(bmp, spr, "outline-green-shadow-true-trans")

# Tests are finished, show exit screen

bmp = Bitmap.new(640, 480)
bmp.fill_rect(0, 0, 640, 480, Color.new(0, 0, 0))

bmp.font = fnt
bmp.draw_text(0, 0, 640, 240, "draw_text Test Suite", 1)
bmp.draw_text(0, 240, 640, 240, "Has Finished", 1)
spr.bitmap = bmp

Graphics.wait(5 * 60)

exit
