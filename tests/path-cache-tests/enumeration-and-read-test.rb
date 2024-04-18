# I think 30k files is a decent sweet spot between showing the difference PR #93 makes
# and not waiting too long for the test to finish.
m1 = System.delta
enum = "archives/EnumerationTest_30k.7z"
System.mount(enum,"",false)
m2 = System.delta

System.mount("archives/ReadTest_1.7z","",false)

t1 = System.delta
# Everything in the EnumerationTest archive will be parsed 4 times without PR #93
System.reload_cache()
t2 = System.delta

# Everything in the EnumerationTest archive will be parsed 6 times without PR #93
# With the PR, it will only enumerate this archive, which will pretty much only take
# the time it takes to mount it since there's just one file and two directories in it.
System.mount("archives/ReadTest_2.7z","",true)
t3 = System.delta

# Get the path to "tests/read/file.txt" that's stored in the path cache.
# It should be "Tests/Read/file.txt", which is found in "archives/ReadTest_1.7z"
# Without PR #93, it will instead be "Tests/Read/File.txt", which is found in "archives/ReadTest_2.7z"
desensitizeTest = System.desensitize("tests/read/file.txt")

# Whithout PR #93, load_data doesn't utilize the path cache, which means
# loading data from archives is always case sensitive
# With it, we only fall back to a case sensitive search if the file isn't in the
# path cache at all.
begin
	loaddataTest = load_data("Tests/Read/File.txt", true)
rescue Exception
	loaddataTest = "nonexistant"
end

System.puts ""
System.puts "#{enum} took #{m2 - m1} seconds to mount"
System.puts "Generating the path cache took #{t2 - t1} seconds"
System.puts "Mounting ReadTest_2.7z and adding it to the cache took #{t3 - t2} seconds"
System.puts "The desensitized path to \"tests/read/file.txt\" is \"#{desensitizeTest}\"."
System.puts "The contents of Tests/Read/File.txt are #{loaddataTest}."
System.puts ""

if System.is_mac?
	def umount(path)
		System.unmount(path, false)
	rescue Exception
	end
	
	# Unmount all of the archives, especially the enumeration test.
	Dir["archives/*"].each do |filename|
		umount(filename)
	end
	System.mount("archives/NFCNFD.zip", "", true)
	
	def tryDesensitize(path)
		# Step 1: try with the NFC character
		str = System.desensitize("tests/#{path}/ヴ.txt")
		if str[0] == "T"
			System.puts "Tests/#{path}/ヴ.txt desensitized via NFC to #{str.size == 15 ? "NFC" : "NFD"} form"
		else
			System.puts "Tests/#{path}/ヴ.txt not desensitized via NFC"
		end
		# Step 2: try with the NFD character
		str = System.desensitize("tests/#{path}/ヴ.txt")
		if str[0] == "T"
			System.puts "Tests/#{path}/ヴ.txt desensitized via NFD to #{str.size == 15 ? "NFC" : "NFD"} form"
		else
			System.puts "Tests/#{path}/ヴ.txt not desensitized via NFD"
		end
		System.puts ""
	end
	
	tryDesensitize("NFC")
	tryDesensitize("NFD")
	
	def tryLoad(path)
		ret = false
		begin
			load_data("Tests/#{path}/ヴ.txt", true)
			ret = "found via NFC"
		rescue
		end
		begin
			load_data("Tests/#{path}/ヴ.txt", true)
			if !ret
				ret = "found via NFD"
			else
				ret += " and NFD"
			end
		rescue
		end
		if !ret
			ret = "not found"
		end
		return ret
	end
	
	System.puts "Testing load_data"
	System.puts "Tests/NFC/ヴ.txt #{tryLoad("NFC")}"
	System.puts "Tests/NFD/ヴ.txt #{tryLoad("NFD")}"
	
	System.puts ""
	
	def tryBitmap(path)
		ret = false
		begin
			Bitmap.new("Tests/#{path}/ヴ.txt")
			raise SDLError.new()
		rescue SDLError
			ret = "found via NFC"
		rescue Exception
		end
		begin
			Bitmap.new("Tests/#{path}/ヴ.txt")
			raise SDLError.new()
		rescue SDLError => e
			if !ret
				ret = "found via NFD"
			else
				ret += " and NFD"
			end
		rescue Exception
		end
		if !ret
			ret = "not found"
		end
		return ret
	end
	
	System.puts "Testing Bitmap constructor"
	System.puts "Tests/NFC/ヴ.txt #{tryBitmap("NFC")}"
	System.puts "Tests/NFD/ヴ.txt #{tryBitmap("NFD")}"
	
	System.puts ""
end
