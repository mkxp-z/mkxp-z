# Test suite for mkxp-z.
# Copyright 2023-2024 Splendide Imaginarius.
# License GPLv2+.

src = Bitmap.new("Graphics/Pictures/lime")

starttime = System.uptime

for i in 1..5 do
	dst1 = src.dup
	dst2 = src.dup
	dst3 = src.dup
	dst4 = src.dup
	dst5 = src.dup
	dst6 = src.dup
	dst7 = src.dup
	dst8 = src.dup
	dst9 = src.dup
	dst10 = src.dup

	dst1.dispose
	dst2.dispose
	dst3.dispose
	dst4.dispose
	dst5.dispose
	dst6.dispose
	dst7.dispose
	dst8.dispose
	dst9.dispose
	dst10.dispose

	GC.start
	Graphics.wait(1)
end

endtime = System.uptime

System::puts("\n\nTotal dup time: %s\n\n" % [endtime - starttime])

exit
