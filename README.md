# mkxp-z

<p align="center"><b>
  <a href="https://github.com/mkxp-z/mkxp-z/actions/workflows/autobuild.yml?query=event%3Apush">Automatic Builds</a>
  ・
  <a href="https://github.com/mkxp-z/mkxp-z/wiki">Documentation</a>
  ・
  <a href="https://matrix.to/#/#rpgmaker:mapleshrine.eu">Matrix Space</a>
  ・
  <a href="https://discord.gg/A8xHE8P">Discord server</a>
</b></p>

<p align=center>
    <img src="screenshot.png?raw=true" width=512 height=412>
</p>

This is a fork of mkxp intended to be a little more than just a barebones recreation of RPG Maker. The original goal was successfully running games based on Pokemon Essentials, which is notoriously dependent on Windows APIs. I'd consider that mission accomplished.

Despite the fact that it was made with Essentials games in mind, there is nothing connected to it contained in this repository, and it should still be compatible with anything that runs in the upstream version of MKXP. You can think of it as MKXP but a bit supercharged --  it should be able to run all but the most demanding of RGSS projects, given a bit of porting work.

It supports Windows, Linux (x86, ARM, and POWER), and both Intel and Apple Silicon versions of macOS.

mkxp-z is licensed under the GNU General Public License v2+. However, if you build mkxp-z with the `enable-https` option turned on (which is the default), you will also need to comply with OpenSSL's Apache v2 license, which in practice means that the resulting binaries are licensed under GPLv3.

## Bindings
Bindings provide the glue code for an interpreted language environment to run game scripts in. mkxp-z focuses on MRI and as such the mruby and null bindings are not included.

## Midi music

mkxp doesn't come with a soundfont by default, so you will have to supply it yourself (set its path in the config). Playback has been tested and should work reasonably well with all RTP assets.

You can use this public domain soundfont: [GMGSx.sf2](https://www.dropbox.com/s/qxdvoxxcexsvn43/GMGSx.sf2?dl=0)

## macOS Controller Support

Binding controller buttons on macOS is slightly different depending on which version you are running. Binding specific buttons requires different versions of the operating system:

+ **Thumbstick Button (L3/R3, LS/RS, L↓/R↓)**: macOS Mojave 10.14.1+
+ **Start/Select (Options/Share, Menu/Back, Plus/Minus)**: macOS Catalina 10.15+
+ **Home (Guide, PS)**: macOS Big Sur 11.0+

Technically, while SDL itself might support these buttons, the keybinding menu had to be rewritten in Cocoa in a hurry, as switching away from native OpenGL broke the original keybinding menu. (ANGLE is used instead, to prevent crashing on Apple Silicon releases of macOS, and to help mkxp switch to Metal)

## Fonts

In the RMXP version of RGSS, fonts are loaded directly from system specific search paths (meaning they must be installed to be available to games). Because this whole thing is a giant platform-dependent headache, Ancurio decided to implement the behavior Enterbrain thankfully added in VX Ace: loading fonts will automatically search a folder called "Fonts", which obeys the default searchpath behavior (ie. it can be located directly in the game folder, or an RTP).

If a requested font is not found, no error is generated. Instead, a built-in font is used. By default, this font is Liberation Sans.

## What doesn't work

* wma audio files
* Creating Bitmaps with sizes greater than your hardware's texture size limit.
  * To find the limit of various GPU's, [the OpenGL Hardware Database](https://opengl.gpuinfo.org/displaycapability.php?name=GL_MAX_TEXTURE_SIZE) is useful.
  * Modern GPU's tend to have a limit of 32 kibipixels for AMD and NVIDIA, 16 kibipixels for Mali, Intel, Apple, and LLVMpipe, and 8 kibipixels for PowerVR. You should check the above database to be sure.
  * There is an exception to this, called *mega surface*. When a Bitmap bigger than the texture limit is created from a file, it is not stored in VRAM, but regular RAM. Its sole purpose is to be used as a tileset bitmap. Any other operation to it (besides blitting to a regular Bitmap) will result in an error.
 
## Notable Thanks

+ Ancurio, who wrote mkxp in the first place
+ Savordez and Aeodyn for making stuff work on Windows
+ Eblo for the `Graphics.play_movie` implementation
+ basically anyone else with commits in here or that reported problems to me
