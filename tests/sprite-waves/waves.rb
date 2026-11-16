b = Bitmap.new("Graphics/Pictures/OST_009")
s = Sprite.new
s.bitmap = b

s.src_rect = Rect.new(100, 100, 200, 200)

s.x = 0
s.y = Graphics.height / 2
s.ox = -25
s.oy = Graphics.height / 2 - 100

swave = Sprite.new
swave.bitmap = b

swave.src_rect = Rect.new(100, 100, 200, 200)

swave.x = Graphics.width / 2
swave.y = Graphics.height / 2
swave.ox = -50
swave.oy = Graphics.height / 2 - 100

swave.wave_amp = 4
swave.wave_length = 10

def update_src_rect(s)
	seconds_per_anim = 3
	s.src_rect.x -= ((300.0 / Graphics.frame_rate) / seconds_per_anim).to_int
	s.src_rect.y -= ((300.0 / Graphics.frame_rate) / seconds_per_anim).to_int
	s.src_rect.x = 100 if s.src_rect.x < -200
	s.src_rect.y = 100 if s.src_rect.y < -200
end

loop {
	Graphics.update
	Input.update
	
	update_src_rect(s)
	update_src_rect(swave)
}
