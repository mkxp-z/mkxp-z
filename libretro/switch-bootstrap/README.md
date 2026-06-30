This document describes how to make libretro builds of mkxp-z for Nintendo Switch that are bundled with a game, suitable for homebrew game releases.

First, you need to build the libretro Nintendo Switch build normally with the patch file switch-bootstrap.patch in this directory applied to RetroArch. You can also use the automatic builds from GitHub Actions whose names contain "mkxp-z_libretro.switch.bootstrap".

Put your game into a zip archive using the store compression algorithm (this part is important: if you use the wrong compression algorithm, the game may or may not run extremely slowly) and rename the zip archive to "Game.mkxpz". Make sure the game is located in the root directory of the zip archive and not in a subdirectory of the archive. On macOS or Linux, you may create a suitable zip archive of your game with store compression by running this command from the game directory:

```bash
zip -r0 ../Game.mkxpz *
```

Replace the example Game.mkxpz already present in the romfs directory with your version of Game.mkxpz.

You may wish to modify the romfs/retroarch directory. For example, you can change the RetroArch configuration in romfs/retroarch/retroarch.cfg, the core options in romfs/retroarch/config/mkxp-z/mkxp-z.opt or add RTPs your game needs to romfs/retroarch/cores/system/mkxp-z/RTP (consult the [core documentation](https://docs.libretro.com/library/mkxp-z/) for instructions on how to add RTPs).

To complete the build, run these commands, using `nacptool` and `elf2nro` from [switch-tools](https://github.com/switchbrew/switch-tools):

```bash
nacptool --create <game name> <game author> <game version> control.nacp
elf2nro <path to the .elf file from the mkxp-z libretro build> <path where the .nro file should be output to> --romfsdir=<path to the romfs directory> --nacp=control.nacp
```

If you experience any issues running this .nro file, you may wish to try again with the included example Game.mkxpz rather than your own version of Game.mkxpz to see whether the problem is in the mkxp-z build or due to your Game.mkxpz being incorrectly formatted.
