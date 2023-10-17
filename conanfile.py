from conan import ConanFile

class MkxpzConanfile(ConanFile):

    def requirements(self):
        #self.requires("freetype/2.13.0")
        #self.requires("libpng/1.6.40")
        #self.requires("ogg/1.3.5")
        #self.requires("openal/1.22.2")
        #self.requires("openssl/3.1.3")
        #self.requires("physfs/3.0.2")
        #self.requires("ruby/3.1.0")
        self.requires("sdl/2.28.2", override=True)
        self.requires("sdl_image/2.0.5")
        self.requires("sdl_ttf/2.20.2")
        #self.requires("theora/1.1.1")
        #self.requires("uchardet/0.0.7")
        self.requires("zlib/1.2.13", override=True)