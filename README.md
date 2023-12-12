# mkxp-z

<p align="center"><b>
  <a href="https://github.com/mkxp-z/mkxp-z/actions/workflows/autobuild.yml?query=event%3Apush">Automatic Builds</a>
  ・
  <a href="https://github.com/mkxp-z/mkxp-z/wiki">Documentation</a>
</b></p>

I don't like the idea of doing straight "releases" anymore, since mkxp-z was never something I considered 'stable' to begin with, and the way I have to do things usually means I don't get to have things stress-tested until a build has already been posted and I eventually find out something is wrong. Automated builds are retained for 60 days and require logging in to access. Past that, you're probably on your own (though I've tried to make sure that building mkxp-z yourself is [as easy as possible](https://github.com/mkxp-z/mkxp-z/wiki/Compilation))

I'm usually *very* slow with responding to things on Github, so if you have something you want to say and you want a faster response, you're probably better off asking in [Ancurio's Discord server](https://discord.gg/A8xHE8P). I don't have my own.
The place is basically a ghost town haunted by myself and a few others, so expect me to pipe up if no one else does. I do not currently frequent anyplace else that you might care about.

I wouldn't expect too much activity from me from now on. I'm basically quitting, but I'm still willing to answer questions, take pull requests, that kind of thing. I'm still willing to hunt down bugs, but given the vast majority of my past troubleshooting came from trying to search through forum threads and snooping through Discord logs, chances are that if it's not something that I just broke, it's probably not a thing that I have the resources or help to fix. I'm not doing second-hand customer service anymore.

-----------

<p align=center>
    <img src="screenshot.png?raw=true" width=512 height=412>
</p>

This is a fork of mkxp intended to be a little more than just a barebones recreation of RPG Maker. The original goal was successfully running games based on Pokemon Essentials, which is notoriously dependent on Windows APIs. I'd consider that mission accomplished.

Despite the fact that it was made with Essentials games in mind, there is nothing connected to it contained in this repository, and it should still be compatible with anything that runs in the upstream version of MKXP. You can think of it as MKXP but a bit supercharged --  it should be able to run all but the most demanding of RGSS projects, given a bit of porting work.

It supports Windows, Linux and both Intel and Apple Silicon versions of macOS.

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
  * Modern GPU's tend to have a limit of 32 kibipixels for NVIDIA, 16 kibipixels for AMD, Intel, Apple, and LLVMpipe, and 8 kibipixels for Mali and PowerVR. You should check the above database to be sure.
  * There is an exception to this, called *mega surface*. When a Bitmap bigger than the texture limit is created from a file, it is not stored in VRAM, but regular RAM. Its sole purpose is to be used as a tileset bitmap. Any other operation to it (besides blitting to a regular Bitmap) will result in an error.
 
## Notable Thanks

+ Ancurio, who wrote mkxp in the first place
+ Savordez and Aeodyn for making stuff work on Windows
+ Eblo for the `Graphics.play_movie` implementation
+ basically anyone else with commits in here or that reported problems to me
