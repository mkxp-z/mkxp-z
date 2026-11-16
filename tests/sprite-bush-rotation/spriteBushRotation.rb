b = Bitmap.new(400, 400)
s = Sprite.new
s.bitmap = b

s.src_rect = Rect.new(100, 100, 200, 200)
b.gradient_fill_rect(s.src_rect, Color.new(255,0,0), Color.new(0,0,255))

s.x = Graphics.width / 2
s.y = Graphics.height / 2
s.ox = s.src_rect.width / 2
s.oy = s.src_rect.height / 2

s.bush_depth = 100
s.bush_opacity = 128

s.zoom_x = 2

def rotate(s)
	seconds_per_rotation = 12
	s.angle += (360.0 / Graphics.frame_rate) / seconds_per_rotation
end

loop {
	Graphics.update
	Input.update
	
	rotate(s)
}
