# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# The engine keeps a text of draw_text on the CPU when the text is on
# drawn pixels, and sends the texts to the texture before the next use
# of the bitmap. A game must not see this: each text is in the bitmap
# for the next call, in the order of the calls. Many texts on drawn
# pixels in one frame must not make the frame wait for the GPU.
#
#   tools/run-engine-tests.sh --suite text_draw.rb
#
# This file must parse on Ruby 1.8, 1.9, and 3.x.

unless defined?(EngineTest)
  harness_path = File.join(File.dirname(__FILE__), 'harness.rb')
  eval(File.read(harness_path), TOPLEVEL_BINDING, harness_path) # rubocop:disable Security/Eval
end

EngineTest.suite('text-draw', 21)

BACK = Color.new(40, 90, 200, 255)
THIN = Color.new(200, 60, 30, 128)
INK = Color.new(255, 230, 40, 200)
COVER = Color.new(10, 200, 60, 255)
WIDTH = 240
HEIGHT = 96
LINE = 28

def drawn_bitmap(width = WIDTH, height = HEIGHT)
  bitmap = Bitmap.new(width, height)
  bitmap.fill_rect(0, 0, width / 2, height, BACK)
  bitmap.fill_rect(width / 2, 0, width / 2, height, THIN)
  bitmap.font.size = 22
  bitmap.font.color = INK
  bitmap
end

def with_bitmaps(*bitmaps)
  yield(*bitmaps)
ensure
  bitmaps.each { |bitmap| bitmap.dispose unless bitmap.disposed? }
end

# A pixel with no opacity shows no color, and SDL_ttf gives such a
# pixel the color of the text.
def largest_difference(one, other)
  a = one.unpack('C*')
  b = other.unpack('C*')
  largest = 0
  a.each_index do |i|
    next if a[i | 3].zero? && b[i | 3].zero?

    difference = (a[i] - b[i]).abs
    largest = difference if difference > largest
  end
  largest
end

def text_pixels(bitmap, back)
  count = 0
  bitmap.raw_data.unpack('C*').each_slice(4) { |pixel| count += 1 unless pixel == back }
  count
end

def assert_same_pixels(expected, actual, label)
  EngineTest.assert(expected == actual, "#{label}: the pixels differ")
end

def assert_other_pixels(unexpected, actual, label)
  EngineTest.assert(unexpected != actual, "#{label}: the pixels are the same")
end

# The picture draw of a text from a cleared bitmap is the same blend
# as the text draw. The two differ only in the rounding of a channel.
def assert_same_picture(expected, actual, label, limit = 2)
  EngineTest.assert_equal(expected.raw_data.size, actual.raw_data.size, "#{label}, size")
  difference = largest_difference(expected.raw_data, actual.raw_data)
  EngineTest.assert(difference <= limit, "#{label}: a channel differs by #{difference}")
end

def draw_as_picture(bitmap, x, y, width, text)
  with_bitmaps(Bitmap.new(width, LINE)) do |part|
    part.font.size = bitmap.font.size
    part.font.color = bitmap.font.color
    part.draw_text(0, 0, width, LINE, text)
    bitmap.blt(x, y, part, part.rect)
  end
end

BACK_PIXEL = [40, 90, 200, 255].freeze

def compare_with_pictures(texts, pictures, lines, label)
  lines.each do |x, y, width, text|
    texts.draw_text(x, y, width, LINE, text)
    draw_as_picture(pictures, x, y, width, text)
  end
  assert_same_picture(pictures, texts, label)
end

def half_drawn_bitmap
  bitmap = Bitmap.new(WIDTH, HEIGHT)
  bitmap.fill_rect(0, 0, WIDTH / 2, HEIGHT, BACK)
  bitmap.font.size = 22
  bitmap.font.color = INK
  bitmap
end

def cleared_bitmap
  bitmap = Bitmap.new(WIDTH, HEIGHT)
  bitmap.font.size = 22
  bitmap.font.color = INK
  bitmap
end

def edge_lines(bitmap)
  lines = [[-20, 2, 'Left edge gjpqy'], [WIDTH - 60, 30, 'Right edge gjpqy'],
           [70, HEIGHT - 12, 'Bottom edge gjpqy'], [130, -10, 'Top edge'],
           [-20, -10, 'Top left gjpqy'], [WIDTH - 60, HEIGHT - 12, 'Bottom right gjpqy']]
  lines.map { |x, y, text| [x, y, bitmap.text_size(text).width + 2, text] }
end

EngineTest.test('texts that overlap on drawn pixels are the picture draws of the same texts') do
  with_bitmaps(drawn_bitmap, drawn_bitmap) do |texts, pictures|
    lines = [[4, 2, 'Earthquake gjpqy'], [30, 14, 'Swords Dance'], [60, 26, 'Stone Edge gjpqy'],
             [100, 60, 'Protect']]
    lines = lines.map { |x, y, text| [x, y, texts.text_size(text).width + 2, text] }
    compare_with_pictures(texts, pictures, lines, 'four texts')
    on_back = text_pixels(texts, BACK_PIXEL) - (WIDTH * HEIGHT / 2)
    EngineTest.assert(on_back > 200, "the texts changed #{on_back} pixels of the left half")
  end
end

EngineTest.test('a narrow text between two texts keeps its place in the order') do
  with_bitmaps(drawn_bitmap, drawn_bitmap) do |texts, pictures|
    full = texts.text_size('Dragon Dance gjpqy').width + 2
    lines = [[4, 20, full], [20, 26, full * 2 / 3], [36, 32, full]]
    lines = lines.map { |x, y, width| [x, y, width, 'Dragon Dance gjpqy'] }
    compare_with_pictures(texts, pictures, lines, 'three texts')
  end
end

EngineTest.test('a narrow text on cleared pixels keeps its place between texts on drawn pixels') do
  with_bitmaps(half_drawn_bitmap, half_drawn_bitmap) do |texts, pictures|
    full = texts.text_size('Protect').width + 2
    lines = [[4, 2, full, 'Protect'], [(WIDTH / 2) + 8, 40, full * 2 / 3, 'Protect'],
             [70, 34, texts.text_size('Swords Dance').width + 2, 'Swords Dance']]
    compare_with_pictures(texts, pictures, lines, 'three texts')
  end
end

EngineTest.test('texts on cleared pixels and across the edges are the picture draws') do
  with_bitmaps(cleared_bitmap, cleared_bitmap) do |texts, pictures|
    compare_with_pictures(texts, pictures, edge_lines(texts), 'six texts')
    EngineTest.assert(text_pixels(texts, [0, 0, 0, 0]) > 200, 'the texts left no pixels')
  end
end

EngineTest.test('texts on drawn and on cleared pixels in one frame are the picture draws') do
  with_bitmaps(half_drawn_bitmap, half_drawn_bitmap) do |texts, pictures|
    compare_with_pictures(texts, pictures, edge_lines(texts), 'six texts')
  end
end

# The engine sends texts that are far apart to the texture in more
# than one step. The third and the fourth text overlap the first two.
EngineTest.test('texts far apart on a large bitmap are the picture draws') do
  with_bitmaps(drawn_bitmap(1024, 768), drawn_bitmap(1024, 768)) do |texts, pictures|
    lines = [[4, 2, 'Earthquake gjpqy'], [850, 720, 'Swords Dance'], [30, 14, 'Stone Edge gjpqy'],
             [870, 728, 'Protect gjpqy']]
    lines = lines.map { |x, y, text| [x, y, texts.text_size(text).width + 2, text] }
    compare_with_pictures(texts, pictures, lines, 'four texts')
  end
end

# The engine sends the two texts to the texture as one draw of the box
# around them. The pixel between the texts is in the box. A fill with
# no opacity keeps its color in the pixel.
EngineTest.test('two texts keep a pixel with a color and no opacity between them') do
  with_bitmaps(cleared_bitmap) do |bitmap|
    filled = [200, 60, 30, 0]
    bitmap.fill_rect(100, 0, 20, 12, Color.new(*filled))
    bitmap.fill_rect(0, 0, 60, 12, BACK)
    bitmap.fill_rect(160, 0, 60, 12, BACK)
    between = [110, 6]
    EngineTest.assert_equal(filled, EngineTest.pixel(bitmap, between), 'the pixel after the fills')
    bitmap.draw_text(0, 0, 60, 12, 'a')
    bitmap.draw_text(160, 0, 60, 12, 'b')
    EngineTest.assert_equal(filled, EngineTest.pixel(bitmap, between), 'the pixel between the texts')
  end
end

MOVED = [80, 25].freeze

# A blur moves pixels to places where no call drew. The text starts
# with spaces, so it has no ink at the pixel.
def assert_text_keeps_moved_pixel(label)
  with_bitmaps(cleared_bitmap) do |bitmap|
    bitmap.fill_rect(40, 12, 40, 30, BACK)
    yield bitmap
    moved = EngineTest.pixel(bitmap, MOVED)
    EngineTest.assert(moved[3] > 0, "#{label} left no pixel at #{MOVED.inspect}")
    bitmap.draw_text(80, 12, 150, LINE, '      i')
    EngineTest.assert_equal(moved, EngineTest.pixel(bitmap, MOVED), "the pixel under the text after #{label}")
  end
end

EngineTest.test('a text keeps the pixels that a blur moved under it') do
  assert_text_keeps_moved_pixel('the blur', &:blur)
  assert_text_keeps_moved_pixel('the radial blur') { |bitmap| bitmap.radial_blur(30, 4) }
end

# The first read makes a copy of the pixels on the CPU. The second
# read must not use that copy.
EngineTest.test('a text is in the bitmap for the next read of a pixel') do
  with_bitmaps(drawn_bitmap) do |bitmap|
    EngineTest.assert_equal(BACK_PIXEL, EngineTest.pixel(bitmap, [4, 44]), 'the pixel before the text')
    bitmap.draw_text(4, 30, 110, LINE, 'MMMMMM')
    changed = (4...100).reject { |x| EngineTest.pixel(bitmap, [x, 44]) == BACK_PIXEL }
    EngineTest.assert(!changed.empty?, 'get_pixel found no pixel of the text')
  end
end

# The block does the same call on a bitmap that holds a text and on a
# bitmap with no text.
def assert_covers_text(label)
  with_bitmaps(drawn_bitmap, drawn_bitmap) do |bitmap, plain|
    bitmap.draw_text(4, 30, 200, LINE, 'Under gjpqy')
    yield bitmap
    yield plain
    assert_same_pixels(plain.raw_data, bitmap.raw_data, label)
  end
end

EngineTest.test('a call that covers the bitmap after a text covers the text') do
  assert_covers_text('after the fill') { |bitmap| bitmap.fill_rect(0, 0, WIDTH, HEIGHT, BACK) }
  assert_covers_text('after the gradient fill') do |bitmap|
    bitmap.gradient_fill_rect(0, 0, WIDTH, HEIGHT, BACK, COVER)
  end
  assert_covers_text('after the clear of a rectangle') { |bitmap| bitmap.clear_rect(0, 0, WIDTH, HEIGHT) }
  with_bitmaps(Bitmap.new(WIDTH, HEIGHT)) do |cover|
    cover.fill_rect(0, 0, WIDTH, HEIGHT, COVER)
    assert_covers_text('after the picture draw') { |bitmap| bitmap.blt(0, 0, cover, cover.rect) }
    assert_covers_text('after the stretched picture draw') do |bitmap|
      bitmap.stretch_blt(bitmap.rect, cover, Rect.new(0, 0, 8, 8))
    end
    assert_covers_text('after new raw pixels') { |bitmap| bitmap.raw_data = cover.raw_data }
  end
end

EngineTest.test('a pixel write after a text stays on top of the text') do
  with_bitmaps(drawn_bitmap) do |bitmap|
    bitmap.draw_text(4, 30, 110, LINE, 'MMMMMM')
    (4...100).each { |x| bitmap.set_pixel(x, 44, COVER) }
    row = bitmap.raw_data[((44 * WIDTH) + 4) * 4, 96 * 4]
    assert_same_pixels([10, 200, 60, 255].pack('C*') * 96, row, 'the row of the pixel writes')
  end
end

# The block changes the pixels of a bitmap. The change must include a
# text, as it includes the picture draw of the same text.
def assert_changes_text(label, limit, &change)
  with_bitmaps(drawn_bitmap, drawn_bitmap, drawn_bitmap) do |texts, pictures, plain|
    texts.draw_text(4, 30, 200, LINE, 'Earthquake gjpqy')
    draw_as_picture(pictures, 4, 30, 200, 'Earthquake gjpqy')
    [texts, pictures, plain].each(&change)
    assert_other_pixels(plain.raw_data, texts.raw_data, "#{label} and a bitmap without text")
    assert_same_picture(pictures, texts, label, limit)
  end
end

EngineTest.test('a hue change or a blur after a text changes the text') do
  assert_changes_text('the hue change', 6) { |bitmap| bitmap.hue_change(120) }
  assert_changes_text('the blur', 4, &:blur)
  assert_changes_text('the radial blur', 4) { |bitmap| bitmap.radial_blur(30, 4) }
end

EngineTest.test('a clear after a text removes the text, and a later text stays') do
  with_bitmaps(drawn_bitmap) do |bitmap|
    bitmap.draw_text(4, 30, 200, LINE, 'Before the clear')
    bitmap.clear
    EngineTest.assert_equal(0, text_pixels(bitmap, [0, 0, 0, 0]), 'pixels after the clear')
    bitmap.draw_text(4, 30, 200, LINE, 'After the clear')
    EngineTest.assert(text_pixels(bitmap, [0, 0, 0, 0]) > 100, 'the later text left no pixels')
  end
end

EngineTest.test('each of two bitmaps gets its own texts') do
  with_bitmaps(drawn_bitmap, drawn_bitmap, drawn_bitmap) do |top, bottom, plain|
    3.times do |i|
      top.draw_text(4 + (i * 30), 4, 100, LINE, 'Top')
      bottom.draw_text(4 + (i * 30), 60, 100, LINE, 'Bottom')
    end
    half = WIDTH * (HEIGHT / 2) * 4
    plain_data = plain.raw_data
    top_data = top.raw_data
    bottom_data = bottom.raw_data
    assert_same_pixels(plain_data[half, half], top_data[half, half], 'lower half of the first bitmap')
    assert_same_pixels(plain_data[0, half], bottom_data[0, half], 'upper half of the second bitmap')
    assert_other_pixels(plain_data[0, half], top_data[0, half], 'upper half of the first bitmap')
    assert_other_pixels(plain_data[half, half], bottom_data[half, half], 'lower half of the second bitmap')
  end
end

EngineTest.test('a copy of a bitmap has its text') do
  with_bitmaps(drawn_bitmap, drawn_bitmap) do |bitmap, plain|
    bitmap.draw_text(4, 30, 200, LINE, 'Source gjpqy')
    with_bitmaps(bitmap.dup) do |copy|
      assert_other_pixels(plain.raw_data, copy.raw_data, 'the copy and a bitmap without text')
      assert_same_pixels(bitmap.raw_data, copy.raw_data, 'the copy')
    end
  end
end

EngineTest.test('a picture draw from a bitmap has its text') do
  with_bitmaps(drawn_bitmap, drawn_bitmap, Bitmap.new(WIDTH, HEIGHT)) do |bitmap, plain, target|
    bitmap.draw_text(4, 30, 200, LINE, 'Source gjpqy')
    target.blt(0, 0, bitmap, bitmap.rect)
    assert_other_pixels(plain.raw_data, target.raw_data, 'the picture draw and a bitmap without text')
    assert_same_pixels(bitmap.raw_data, target.raw_data, 'the picture draw')
  end
end

def corner_texts(bitmap)
  bitmap.draw_text(2, 2, 120, LINE, 'Earthquake')
  bitmap.draw_text(bitmap.width - 122, bitmap.height - LINE - 2, 120, LINE, 'Protect gjpqy')
end

def corner_text_pixels
  with_bitmaps(drawn_bitmap(WIDTH + 40)) do |bitmap|
    corner_texts(bitmap)
    bitmap.raw_data
  end
end

# The later bitmaps have a different width than the bitmap that lost
# its text.
EngineTest.test('a text that a dispose or a clear removed is not in the texts of a later bitmap') do
  expected = corner_text_pixels
  disposed = drawn_bitmap
  disposed.draw_text(4, 30, 200, LINE, 'Gone gjpqy')
  disposed.dispose
  assert_same_pixels(expected, corner_text_pixels, 'after the dispose')
  with_bitmaps(drawn_bitmap) do |cleared|
    cleared.draw_text(4, 30, 200, LINE, 'Gone gjpqy')
    cleared.clear
    assert_same_pixels(expected, corner_text_pixels, 'after the clear')
  end
end

EngineTest.test('the screen shows a text without one more call on the bitmap') do
  viewport = Viewport.new(0, 0, Graphics.width, Graphics.height)
  viewport.z = 99_999
  sprite = Sprite.new(viewport)
  sprite.bitmap = Bitmap.new(WIDTH, HEIGHT)
  sprite.bitmap.fill_rect(0, 0, WIDTH, HEIGHT, BACK)
  sprite.bitmap.font.size = 22
  begin
    Graphics.update
    with_bitmaps(Graphics.snap_to_bitmap) do |before|
      sprite.bitmap.draw_text(4, 30, 200, LINE, 'MMMMMM')
      with_bitmaps(Graphics.snap_to_bitmap) do |after|
        assert_other_pixels(before.raw_data, after.raw_data, 'the screen after the text')
      end
    end
  ensure
    sprite.bitmap.dispose
    sprite.dispose
    viewport.dispose
  end
end

TEXTS = 248

def texts_ms(bitmap, width)
  start = Time.now
  TEXTS.times { |i| bitmap.draw_text((i % 16) * width, (i / 16) * 16, width, 16, 'Earthquake') }
  bitmap.get_pixel(0, 0)
  (Time.now - start) * 1000
end

# A battle menu of Pokemon Essentials draws 248 texts on drawn pixels
# in one frame. The time is not the same on each computer, so the
# test compares it with the time of the same texts on cleared pixels.
# On an iPhone 13 Pro, the texts on drawn pixels took 10 times that
# with one GPU draw for each text.
EngineTest.test('many texts on drawn pixels take the time of the texts on cleared pixels') do
  with_bitmaps(Bitmap.new(8, 8)) do |probe|
    probe.font.size = 12
    width = probe.text_size('Earthquake').width + 2
    with_bitmaps(Bitmap.new(width * 16, 256), Bitmap.new(width * 16, 256)) do |drawn, cleared|
      [drawn, cleared].each { |bitmap| bitmap.font.size = 12 }
      drawn_ms = []
      cleared_ms = []
      5.times do
        drawn.fill_rect(0, 0, drawn.width, drawn.height, BACK)
        drawn.get_pixel(0, 0)
        drawn_ms << texts_ms(drawn, width)
        cleared.clear
        cleared.get_pixel(0, 0)
        cleared_ms << texts_ms(cleared, width)
      end
      on_drawn = format('%.1f', drawn_ms.min)
      on_cleared = format('%.1f', cleared_ms.min)
      EngineTest.info('drawn_ms', on_drawn)
      EngineTest.info('cleared_ms', on_cleared)
      EngineTest.assert(drawn_ms.min < cleared_ms.min * 4,
                        "#{on_drawn} ms on drawn pixels, and #{on_cleared} ms on cleared pixels")
    end
  end
end

ROUNDS = 16

def rounds_ms(bitmap)
  start = Time.now
  ROUNDS.times do
    yield
    bitmap.fill_rect(0, 0, 1, 1, BACK)
  end
  (Time.now - start) * 1000
end

# The first block draws texts that are far apart, and the second block
# draws the same texts near each other.
def assert_far_time(bitmap, names, limit, far, near)
  far_ms = []
  near_ms = []
  5.times do
    far_ms << rounds_ms(bitmap, &far)
    near_ms << rounds_ms(bitmap, &near)
  end
  far_text = format('%.1f', far_ms.min)
  near_text = format('%.1f', near_ms.min)
  EngineTest.info("#{names[0]}_ms", far_text)
  EngineTest.info("#{names[1]}_ms", near_text)
  EngineTest.assert(far_ms.min < near_ms.min * limit, "#{far_text} ms far apart, and #{near_text} ms near")
end

# The engine sends the box around the texts to the texture. Two texts
# in opposite corners of a large bitmap must not make the box as large
# as the bitmap. On an iPhone 13 Pro, such a box took 80 times the
# time of two texts side by side, and two boxes take 2 to 7 times.
EngineTest.test('two texts far apart on a large bitmap take the time of two texts side by side') do
  with_bitmaps(drawn_bitmap(2048, 2048)) do |bitmap|
    far = proc { corner_texts(bitmap) }
    near = proc do
      bitmap.draw_text(2, 2, 120, LINE, 'Earthquake')
      bitmap.draw_text(130, 2, 120, LINE, 'Protect gjpqy')
    end
    assert_far_time(bitmap, %w[far near], 20, far, near)
  end
end

STEPS = 16

# The texts go down the left edge, then along a row. Each text of the
# row makes the box around the texts only a little larger. In the
# second block, a fill sends the texts of the left edge to the texture
# before the row.
EngineTest.test('texts along two edges of a large bitmap take the time of the same texts in two groups') do
  with_bitmaps(drawn_bitmap(2048, 2048)) do |bitmap|
    last = 120 * (STEPS - 1)
    column = proc { STEPS.times { |i| bitmap.draw_text(0, 120 * i, 120, LINE, 'Earthquake') } }
    row = proc { (1...STEPS).each { |i| bitmap.draw_text(120 * i, last, 120, LINE, 'Earthquake') } }
    edges = proc do
      column.call
      row.call
    end
    groups = proc do
      column.call
      bitmap.fill_rect(0, 0, 1, 1, BACK)
      row.call
    end
    assert_far_time(bitmap, %w[edges groups], 2, edges, groups)
  end
end

EngineTest.finish
exit
