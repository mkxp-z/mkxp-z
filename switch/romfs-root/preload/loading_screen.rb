# loading_screen.rb
# mkxp-z only presents a frame on Graphics.update, so the screen stays black
# while a game's scripts load, which takes minutes for big games (e.g. Pokemon
# Essentials with many plugins). Present one "Loading..." frame first; it stays
# on screen until the game draws its own.

begin
	width, height = Graphics.width, Graphics.height
	bitmap = Bitmap.new(width, height)
	bitmap.fill_rect(bitmap.rect, Color.new(0, 0, 0))
	bitmap.font.size = [height / 18, 16].max
	bitmap.font.color = Color.new(220, 220, 220)
	title = System.game_title.to_s
	text = title.empty? ? "Loading..." : "Loading #{title}..."
	line = bitmap.font.size * 2
	bitmap.draw_text(0, (height - line) / 2, width, line, text, 1)

	sprite = Sprite.new
	sprite.bitmap = bitmap
	sprite.z = 0x7fffffff
	Graphics.update
	sprite.dispose
	bitmap.dispose
rescue StandardError => e
	System.puts("[loading_screen] #{e.class}: #{e.message}")
end
