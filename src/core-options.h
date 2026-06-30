/*
** core-options.h
**
** This file is part of mkxp.
**
** Copyright (C) 2025 - 2026 The mkxp-z authors
**
** mkxp is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 of the License, or
** (at your option) any later version.
**
** mkxp is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MKXPZ_CORE_OPTIONS_H
#define MKXPZ_CORE_OPTIONS_H

#include <libretro.h>

static const struct retro_core_option_v2_category core_option_categories[] = {
    {
        "runtime",
        "Runtime",
        "Configure the execution environment for games.",
    },
    {
        "video",
        "Video",
        "Configure the game engine's graphics.",
    },
    {
        "audio",
        "Audio",
        "Configure the game engine's sound.",
    },
    {
        "text",
        "Text",
        "Configure fonts and how text is rendered on the screen.",
    },
    {
        "button",
        "Button Bindings",
        "Choose the RetroPad inputs that are mapped to each RGSS button.",
    },
    {
        "scancode",
        "Scancode Bindings",
        "Choose the RetroPad inputs that are mapped to each keyboard key.",
    },
    {
        "controller",
        "Controller Bindings",
        "Choose the RetroPad inputs that are mapped to each controller button.",
    },
    {
        "preload",
        "Preload Scripts",
        "Choose preload scripts to load on game startup. More preload scripts may be added to the mkxp-z/Scripts/Preload subdirectory of the libretro system directory.",
    },
    {
        "postload",
        "Postload Scripts",
        "Choose postload scripts to load on game startup. More postload scripts may be added to the mkxp-z/Scripts/Postload subdirectory of the libretro system directory.",
    },
    {
        nullptr,
        nullptr,
        nullptr,
    },
};

#define BUTTON_OPTIONS \
    {"button-13", "Same as Input::C"}, \
    {"button-12", "Same as Input::B"}, \
    {"button-11", "Same as Input::A"}, \
    {"button-14", "Same as Input::X"}, \
    {"button-15", "Same as Input::Y"}, \
    {"button-16", "Same as Input::Z"}, \
    {"button-17", "Same as Input::L"}, \
    {"button-18", "Same as Input::R"}, \
    {"button-21", "Same as Input::SHIFT"}, \
    {"button-22", "Same as Input::CTRL"}, \
    {"button-23", "Same as Input::ALT"}, \
    {"button-25", "Same as Input::F5"}, \
    {"button-26", "Same as Input::F6"}, \
    {"button-27", "Same as Input::F7"}, \
    {"button-28", "Same as Input::F8"}, \
    {"button-29", "Same as Input::F9"}, \
    {"button-38", "Same as Input::MOUSELEFT"}, \
    {"button-39", "Same as Input::MOUSEMIDDLE"}, \
    {"button-40", "Same as Input::MOUSERIGHT"}, \
    {"button-41", "Same as Input::MOUSEX1"}, \
    {"button-42", "Same as Input::MOUSEX2"}, \
    {"button-2", "Same as Input::DOWN"}, \
    {"button-4", "Same as Input::LEFT"}, \
    {"button-6", "Same as Input::RIGHT"}, \
    {"button-8", "Same as Input::UP"}, \

#define RETROPAD_OPTIONS \
    {"joypad-8-0", "A on Port 1"}, \
    {"joypad-0-0", "B on Port 1"}, \
    {"joypad-9-0", "X on Port 1"}, \
    {"joypad-1-0", "Y on Port 1"}, \
    {"joypad-10-0", "L1 on Port 1"}, \
    {"joypad-11-0", "R1 on Port 1"}, \
    {"joypad-12-0", "L2 on Port 1"}, \
    {"joypad-13-0", "R2 on Port 1"}, \
    {"joypad-14-0", "L3 on Port 1"}, \
    {"joypad-15-0", "R3 on Port 1"}, \
    {"joypad-2-0", "Select on Port 1"}, \
    {"joypad-3-0", "Start on Port 1"}, \
    {"joypad-4-0", "Up on Port 1"}, \
    {"joypad-5-0", "Down on Port 1"}, \
    {"joypad-6-0", "Left on Port 1"}, \
    {"joypad-7-0", "Right on Port 1"}, \
    {"lightgun-2-0", "Gun Trigger on Port 1"}, \
    {"lightgun-16-0", "Gun Reload on Port 1"}, \
    {"lightgun-3-0", "Gun A on Port 1"}, \
    {"lightgun-4-0", "Gun B on Port 1"}, \
    {"lightgun-8-0", "Gun C on Port 1"}, \
    {"lightgun-7-0", "Gun Select on Port 1"}, \
    {"lightgun-6-0", "Gun Start on Port 1"}, \
    {"lightgun-9-0", "Gun Up on Port 1"}, \
    {"lightgun-10-0", "Gun Down on Port 1"}, \
    {"lightgun-11-0", "Gun Left on Port 1"}, \
    {"lightgun-12-0", "Gun Right on Port 1"}, \
    {"mouse-2-0", "Primary Mouse Button on Port 1"}, \
    {"mouse-3-0", "Secondary Mouse Button on Port 1"}, \
    {"mouse-6-0", "Middle Mouse Button on Port 1"}, \
    {"mouse-9-0", "Mouse Button 4 on Port 1"}, \
    {"mouse-10-0", "Mouse Button 5 on Port 1"}, \
    {"joypad-8-1", "A on Port 2"}, \
    {"joypad-0-1", "B on Port 2"}, \
    {"joypad-9-1", "X on Port 2"}, \
    {"joypad-1-1", "Y on Port 2"}, \
    {"joypad-10-1", "L1 on Port 2"}, \
    {"joypad-11-1", "R1 on Port 2"}, \
    {"joypad-12-1", "L2 on Port 2"}, \
    {"joypad-13-1", "R2 on Port 2"}, \
    {"joypad-14-1", "L3 on Port 2"}, \
    {"joypad-15-1", "R3 on Port 2"}, \
    {"joypad-2-1", "Select on Port 2"}, \
    {"joypad-3-1", "Start on Port 2"}, \
    {"joypad-4-1", "Up on Port 2"}, \
    {"joypad-5-1", "Down on Port 2"}, \
    {"joypad-6-1", "Left on Port 2"}, \
    {"joypad-7-1", "Right on Port 2"}, \
    {"lightgun-2-1", "Gun Trigger on Port 2"}, \
    {"lightgun-16-1", "Gun Reload on Port 2"}, \
    {"lightgun-3-1", "Gun A on Port 2"}, \
    {"lightgun-4-1", "Gun B on Port 2"}, \
    {"lightgun-8-1", "Gun C on Port 2"}, \
    {"lightgun-7-1", "Gun Select on Port 2"}, \
    {"lightgun-6-1", "Gun Start on Port 2"}, \
    {"lightgun-9-1", "Gun Up on Port 2"}, \
    {"lightgun-10-1", "Gun Down on Port 2"}, \
    {"lightgun-11-1", "Gun Left on Port 2"}, \
    {"lightgun-12-1", "Gun Right on Port 2"}, \
    {"mouse-2-1", "Primary Mouse Button on Port 2"}, \
    {"mouse-3-1", "Secondary Mouse Button on Port 2"}, \
    {"mouse-6-1", "Middle Mouse Button on Port 2"}, \
    {"mouse-9-1", "Mouse Button 4 on Port 2"}, \
    {"mouse-10-1", "Mouse Button 5 on Port 2"}, \
    {"joypad-8-2", "A on Port 3"}, \
    {"joypad-0-2", "B on Port 3"}, \
    {"joypad-9-2", "X on Port 3"}, \
    {"joypad-1-2", "Y on Port 3"}, \
    {"joypad-10-2", "L1 on Port 3"}, \
    {"joypad-11-2", "R1 on Port 3"}, \
    {"joypad-12-2", "L2 on Port 3"}, \
    {"joypad-13-2", "R2 on Port 3"}, \
    {"joypad-14-2", "L3 on Port 3"}, \
    {"joypad-15-2", "R3 on Port 3"}, \
    {"joypad-2-2", "Select on Port 3"}, \
    {"joypad-3-2", "Start on Port 3"}, \
    {"joypad-4-2", "Up on Port 3"}, \
    {"joypad-5-2", "Down on Port 3"}, \
    {"joypad-6-2", "Left on Port 3"}, \
    {"joypad-7-2", "Right on Port 3"}, \
    {"lightgun-2-2", "Gun Trigger on Port 3"}, \
    {"lightgun-16-2", "Gun Reload on Port 3"}, \
    {"lightgun-3-2", "Gun A on Port 3"}, \
    {"lightgun-4-2", "Gun B on Port 3"}, \
    {"lightgun-8-2", "Gun C on Port 3"}, \
    {"lightgun-7-2", "Gun Select on Port 3"}, \
    {"lightgun-6-2", "Gun Start on Port 3"}, \
    {"lightgun-9-2", "Gun Up on Port 3"}, \
    {"lightgun-10-2", "Gun Down on Port 3"}, \
    {"lightgun-11-2", "Gun Left on Port 3"}, \
    {"lightgun-12-2", "Gun Right on Port 3"}, \
    {"mouse-2-2", "Primary Mouse Button on Port 3"}, \
    {"mouse-3-2", "Secondary Mouse Button on Port 3"}, \
    {"mouse-6-2", "Middle Mouse Button on Port 3"}, \
    {"mouse-9-2", "Mouse Button 4 on Port 3"}, \
    {"mouse-10-2", "Mouse Button 5 on Port 3"}, \

static const struct retro_core_option_v2_definition core_option_definitions[] = {
    {
        "mkxp-z_rgssVersion",
        "RGSS Version",
        nullptr,
        (
            "Specify the RGSS version to run under."
            " By default, mkxp will try to guess the required version"
            " based on the game files."
            " If this fails, the version defaults to 1."
            " Changes will take effect after the core is reset."
        ),
        nullptr,
        "runtime",
        {
            {"inherit", "Inherit from mkxp.json"},
            {"default", "Default"},
            {"1", "1 (RPG Maker XP)"},
            {"2", "2 (RPG Maker VX)"},
            {"3", "3 (RPG Maker VX Ace)"},
            {nullptr, nullptr},
        },
        "inherit",
    },
    {
        "mkxp-z_saveStateSize",
        "Save State Size",
        nullptr,
        (
            "Maximum size of each save state, in mebibytes."
            " If the game uses more than this much memory, save state creation will fail."
            " Changes to this setting will not take effect until the core is unloaded."
        ),
        nullptr,
        "runtime",
        {
            {"64", "64"},
            {"66", "66"},
            {"68", "68"},
            {"70", "70"},
            {"72", "72"},
            {"74", "74"},
            {"76", "76"},
            {"78", "78"},
            {"80", "80"},
            {"82", "82"},
            {"84", "84"},
            {"86", "86"},
            {"88", "88"},
            {"90", "90"},
            {"92", "92"},
            {"94", "94"},
            {"96", "96"},
            {"98", "98"},
            {"100", "100"},
            {"102", "102"},
            {"104", "104"},
            {"106", "106"},
            {"108", "108"},
            {"110", "110"},
            {"112", "112"},
            {"114", "114"},
            {"116", "116"},
            {"118", "118"},
            {"120", "120"},
            {"122", "122"},
            {"124", "124"},
            {"126", "126"},
            {"128", "128"},
            {"132", "132"},
            {"136", "136"},
            {"140", "140"},
            {"144", "144"},
            {"148", "148"},
            {"152", "152"},
            {"156", "156"},
            {"160", "160"},
            {"164", "164"},
            {"168", "168"},
            {"172", "172"},
            {"176", "176"},
            {"180", "180"},
            {"184", "184"},
            {"188", "188"},
            {"192", "192"},
            {"196", "196"},
            {"200", "200"},
            {"204", "204"},
            {"208", "208"},
            {"212", "212"},
            {"216", "216"},
            {"220", "220"},
            {"224", "224"},
            {"228", "228"},
            {"232", "232"},
            {"236", "236"},
            {"240", "240"},
            {"244", "244"},
            {"248", "248"},
            {"252", "252"},
            {"256", "256"},
            {"264", "264"},
            {"272", "272"},
            {"280", "280"},
            {"288", "288"},
            {"296", "296"},
            {"304", "304"},
            {"312", "312"},
            {"320", "320"},
            {"328", "328"},
            {"336", "336"},
            {"344", "344"},
            {"352", "352"},
            {"360", "360"},
            {"368", "368"},
            {"376", "376"},
            {"384", "384"},
            {"392", "392"},
            {"400", "400"},
            {"408", "408"},
            {"416", "416"},
            {"424", "424"},
            {"432", "432"},
            {"440", "440"},
            {"448", "448"},
            {"456", "456"},
            {"464", "464"},
            {"472", "472"},
            {"480", "480"},
            {"488", "488"},
            {"496", "496"},
            {"504", "504"},
            {"512", "512"},
            {"544", "544"},
            {"576", "576"},
            {"608", "608"},
            {"640", "640"},
            {"672", "672"},
            {"704", "704"},
            {"736", "736"},
            {"768", "768"},
            {"800", "800"},
            {"832", "832"},
            {"864", "864"},
            {"896", "896"},
            {"928", "928"},
            {"960", "960"},
            {"992", "992"},
            {"1024", "1024"},
            {"1152", "1152"},
            {"1280", "1280"},
            {"1408", "1408"},
            {"1536", "1536"},
            {"1664", "1664"},
            {"1792", "1792"},
            {"1920", "1920"},
            {"2048", "2048"},
            {"2560", "2560"},
            {"3072", "3072"},
            {"3584", "3584"},
            {"4096", "4096"},
            {nullptr, nullptr},
        },
        "100",
    },
    {
        "mkxp-z_debug",
        "Debug",
        nullptr,
        (
            "Launch the game in debug mode."
            " Changes will take effect after the core is reset."
        ),
        nullptr,
        "runtime",
        {
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        "disabled",
    },
    {
        "mkxp-z_battleTest",
        "Battle Test",
        nullptr,
        (
            "Launch the game in battle test mode."
            " Changes will take effect after the core is reset."
        ),
        nullptr,
        "runtime",
        {
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        "disabled",
    },
    {
        "mkxp-z_frameSkip",
        "Frame Skip",
        nullptr,
        (
            "Skip (don't draw) frames when behind."
        ),
        nullptr,
        "video",
        {
            {"inherit", "Inherit from mkxp.json"},
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        "disabled",
    },
    {
        "mkxp-z_subImageFix",
        "Subimage Fix",
        nullptr,
        (
            "Work around buggy graphics drivers which don't"
            " properly synchronize texture access, most"
            " apparent when text doesn't show up or the map"
            " tileset doesn't render at all."
            " (default: enabled on systems using OpenGL ES and on Nintendo Switch, disabled on other systems)"
        ),
        nullptr,
        "video",
        {
            {"inherit", "Inherit from mkxp.json"},
            {"default", "Default"},
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_enableBlitting",
        "Framebuffer Blitting",
        nullptr,
        (
            "Enable framebuffer blitting if the driver is"
            " capable of it. Some drivers carry buggy"
            " implementations of this functionality, so"
            " disabling it can be used as a workaround."
            " (default: disabled on Windows, enabled on other systems)"
        ),
        nullptr,
        "video",
        {
            {"inherit", "Inherit from mkxp.json"},
            {"default", "Default"},
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_textureSync",
        "Texture Synchronization",
        nullptr,
        (
            "Controls how often GPU textures are copied to CPU memory."
            " Eager synchronization is less likely to cause graphical issues but may cause performance problems in some games."
            " Lazy synchronization is faster but may or may not,"
            " depending on which libretro frontend you're using and what version of it you're using,"
            " cause graphical artifacts when performing certain operations,"
            " such as toggling fullscreen or, on Android, opening the app switcher."
            " (default: lazy)"
        ),
        nullptr,
        "video",
        {
            {"default", "Default"},
            {"eager", "Eager"},
            {"lazy", "Lazy"},
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_threadedAudio",
        "Threaded Audio",
        nullptr,
        (
            "Use a worker thread for rendering the audio instead of"
            " rendering in the main thread, if possible. Reduces audio"
            " crackling, especially on systems with slow file system"
            " access speed. Changes to this setting will not take effect"
            " until the game is closed."
        ),
        nullptr,
        "audio",
        {
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        "enabled",
    },
    {
        "mkxp-z_midiChorus",
        "MIDI Chorus",
        nullptr,
        (
            "Activate \"chorus\" effect for midi playback."
        ),
        nullptr,
        "audio",
        {
            {"inherit", "Inherit from mkxp.json"},
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        "inherit",
    },
    {
        "mkxp-z_midiReverb",
        "MIDI Reverb",
        nullptr,
        (
            "Activate \"reverb\" effect for midi playback."
        ),
        nullptr,
        "audio",
        {
            {"inherit", "Inherit from mkxp.json"},
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        "inherit",
    },
    {
        "mkxp-z_SESourceCount",
        "SE Source Count",
        nullptr,
        (
            "Number of OpenAL sources to allocate for SE playback."
            " If there are a lot of sounds playing at the same time"
            " and audibly cutting each other off, try increasing"
            " this number."
            " Changes will take effect after the core is reset."
            " (if this value is also set in the game's mkxp.json,"
            " the maximum of the value set here and the value in"
            " mkxp.json will be used)"
        ),
        nullptr,
        "audio",
        {
            {"6", "6"},
            {"7", "7"},
            {"8", "8"},
            {"9", "9"},
            {"10", "10"},
            {"11", "11"},
            {"12", "12"},
            {"13", "13"},
            {"14", "14"},
            {"15", "15"},
            {"16", "16"},
            {"17", "17"},
            {"18", "18"},
            {"19", "19"},
            {"20", "20"},
            {"21", "21"},
            {"22", "22"},
            {"23", "23"},
            {"24", "24"},
            {"25", "25"},
            {"26", "26"},
            {"27", "27"},
            {"28", "28"},
            {"29", "29"},
            {"30", "30"},
            {"31", "31"},
            {"32", "32"},
            {"33", "33"},
            {"34", "34"},
            {"35", "35"},
            {"36", "36"},
            {"37", "37"},
            {"38", "38"},
            {"39", "39"},
            {"40", "40"},
            {"41", "41"},
            {"42", "42"},
            {"43", "43"},
            {"44", "44"},
            {"45", "45"},
            {"46", "46"},
            {"47", "47"},
            {"48", "48"},
            {"49", "49"},
            {"50", "50"},
            {"51", "51"},
            {"52", "52"},
            {"53", "53"},
            {"54", "54"},
            {"55", "55"},
            {"56", "56"},
            {"57", "57"},
            {"58", "58"},
            {"59", "59"},
            {"60", "60"},
            {"61", "61"},
            {"62", "62"},
            {"63", "63"},
            {"64", "64"},
            {nullptr, nullptr},
        },
        "6",
    },
    {
        "mkxp-z_fontScale",
        "Font Scale",
        nullptr,
        (

            "Scales the sizes of all fonts."
            " If you think text tends to be too large or too small,"
            " try fiddling with this."
            " (if this value is also set in the game's mkxp.json,"
            " the product of the value set here and the value in"
            " mkxp.json will be used)"
        ),
        nullptr,
        "text",
        {
            {"0.75", "0.75"},
            {"0.75625", "0.75625"},
            {"0.7625", "0.7625"},
            {"0.76875", "0.76875"},
            {"0.775", "0.775"},
            {"0.78125", "0.78125"},
            {"0.7875", "0.7875"},
            {"0.79375", "0.79375"},
            {"0.8", "0.8"},
            {"0.80625", "0.80625"},
            {"0.8125", "0.8125"},
            {"0.81875", "0.81875"},
            {"0.825", "0.825"},
            {"0.83125", "0.83125"},
            {"0.8375", "0.8375"},
            {"0.84375", "0.84375"},
            {"0.85", "0.85"},
            {"0.85625", "0.85625"},
            {"0.8625", "0.8625"},
            {"0.86875", "0.86875"},
            {"0.875", "0.875"},
            {"0.88125", "0.88125"},
            {"0.8875", "0.8875"},
            {"0.89375", "0.89375"},
            {"0.9", "0.9"},
            {"0.90625", "0.90625"},
            {"0.9125", "0.9125"},
            {"0.91875", "0.91875"},
            {"0.925", "0.925"},
            {"0.93125", "0.93125"},
            {"0.9375", "0.9375"},
            {"0.94375", "0.94375"},
            {"0.95", "0.95"},
            {"0.95625", "0.95625"},
            {"0.9625", "0.9625"},
            {"0.96875", "0.96875"},
            {"0.975", "0.975"},
            {"0.98125", "0.98125"},
            {"0.9875", "0.9875"},
            {"0.99375", "0.99375"},
            {"1.0", "1.0"},
            {"1.00625", "1.00625"},
            {"1.0125", "1.0125"},
            {"1.01875", "1.01875"},
            {"1.025", "1.025"},
            {"1.03125", "1.03125"},
            {"1.0375", "1.0375"},
            {"1.04375", "1.04375"},
            {"1.05", "1.05"},
            {"1.05625", "1.05625"},
            {"1.0625", "1.0625"},
            {"1.06875", "1.06875"},
            {"1.075", "1.075"},
            {"1.08125", "1.08125"},
            {"1.0875", "1.0875"},
            {"1.09375", "1.09375"},
            {"1.1", "1.1"},
            {"1.10625", "1.10625"},
            {"1.1125", "1.1125"},
            {"1.11875", "1.11875"},
            {"1.125", "1.125"},
            {"1.13125", "1.13125"},
            {"1.1375", "1.1375"},
            {"1.14375", "1.14375"},
            {"1.15", "1.15"},
            {"1.15625", "1.15625"},
            {"1.1625", "1.1625"},
            {"1.16875", "1.16875"},
            {"1.175", "1.175"},
            {"1.18125", "1.18125"},
            {"1.1875", "1.1875"},
            {"1.19375", "1.19375"},
            {"1.2", "1.2"},
            {"1.20625", "1.20625"},
            {"1.2125", "1.2125"},
            {"1.21875", "1.21875"},
            {"1.225", "1.225"},
            {"1.23125", "1.23125"},
            {"1.2375", "1.2375"},
            {"1.24375", "1.24375"},
            {"1.25", "1.25"},
            {nullptr, nullptr},
        },
        "1.0",
    },
    {
        "mkxp-z_fontKerning",
        "Kerning",
        nullptr,
        (
            "Kerning adjusts the spacing between individual letters or characters."
            " Enabling it generally looks nicer, but RGSS doesn't use it,"
            " so disabling it should make text appearance more accurate."
            " (default: enabled)"
        ),
        nullptr,
        "text",
        {
            {"inherit", "Inherit from mkxp.json"},
            {"default", "Default"},
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        "inherit",
    },
    {
        "mkxp-z_fontHinting",
        "Font Hinting",
        nullptr,
        (
            "Hinting adjusts the rendering of individual letters or characters."
            " Enabling it may look nicer (especially on low-resolution displays), but"
            " RGSS doesn't use it, so disabling it should make text appearance more"
            " accurate. Documentation can be found at:"
            " https://pysdl2.readthedocs.io/en/latest/modules/sdl2_sdlttf.html#sdl2.sdlttf.TTF_HINTING_NORMAL"
            " (default: 3)"
        ),
        nullptr,
        "text",
        {
            {"inherit", "Inherit from mkxp.json"},
            {"default", "Default"},
            {"0", "0 (Normal)"},
            {"1", "1 (Light)"},
            {"2", "2 (Mono)"},
            {"3", "3 (None)"},
            {nullptr, nullptr},
        },
        "inherit",
    },
    {
        "mkxp-z_fontHeightReporting",
        "Font Height Reporting",
        nullptr,
        (
            "Controls the algorithm for reporting the height of rendered text."
            " 0: Nominal (TTF_FontHeight); matches RGSS behavior; may cut off bottoms of some characters."
            " 1: Rendered (TTF_SizeUTF8); deviates from RGSS; may look better."
            " (default: 0)"
        ),
        nullptr,
        "text",
        {
            {"inherit", "Inherit from mkxp.json"},
            {"default", "Default"},
            {"0", "0 (Nominal)"},
            {"1", "1 (Rendered)"},
            {nullptr, nullptr},
        },
        "inherit",
    },
    {
        "mkxp-z_fontOutlineCrop",
        "Outline Crop",
        nullptr,
        (
            "Crops top row and left column of text that has an outline."
            " Disabling it generally looks nicer, but RGSS enables it, so enabling it"
            " should make text appearance more accurate."
            " (default: enabled)"
        ),
        nullptr,
        "text",
        {
            {"inherit", "Inherit from mkxp.json"},
            {"default", "Default"},
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        "inherit",
    },
    {
        "mkxp-z_loadFontsIntoMemory",
        "Load Fonts Into Memory",
        nullptr,
        (
            "When loading a font, load the entire font file into memory instead of using a file handle."
            " This improves text rendering performance on systems with extremely slow file system access speed"
            " at the cost of higher memory usage."
            " (default: enabled on Emscripten, disabled on other platforms)"
        ),
        nullptr,
        "text",
        {
            {"default", "Default"},
            {"enabled", "Enabled"},
            {"disabled", "Disabled"},
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-13",
        "Input::C Button Binding",
        "Input::C",
        "This button is typically used to perform an action. The stock RPG Maker runtimes bind this to the space key and return key on the keyboard. It is also bound to the C key on the keyboard in RPG Maker XP and the Z key on the keyboard in later versions.",
        nullptr,
        "button",
        {
            {"default", "Default (A on Port 1)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-12",
        "Input::B Button Binding",
        "Input::B",
        "This button is typically used to cancel an action or open the menu. The stock RPG Maker runtimes bind this to the X key, escape key and number pad 0 key on the keyboard.",
        nullptr,
        "button",
        {
            {"default", "Default (B on Port 1)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-11",
        "Input::A Button Binding",
        "Input::A",
        "The stock RPG Maker runtimes bind this to the left shift key on the keyboard. In RPG Maker XP, it is also bound to the Z key on the keyboard, but not in later versions.",
        nullptr,
        "button",
        {
            {"default", "Default (X on Port 1)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-14",
        "Input::X Button Binding",
        "Input::X",
        "The stock RPG Maker runtimes bind this to the A key on the keyboard.",
        nullptr,
        "button",
        {
            {"default", "Default (Y on Port 1)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-15",
        "Input::Y Button Binding",
        "Input::Y",
        "The stock RPG Maker runtimes bind this to the S key on the keyboard.",
        nullptr,
        "button",
        {
            {"default", "Default (L3 on Port 1)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-16",
        "Input::Z Button Binding",
        "Input::Z",
        "The stock RPG Maker runtimes bind this to the D key on the keyboard.",
        nullptr,
        "button",
        {
            {"default", "Default (R3 on Port 1)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-17",
        "Input::L Button Binding",
        "Input::L",
        "This button is typically used to navigate left in a menu. The stock RPG Maker runtimes bind this to the Q key on the keyboard.",
        nullptr,
        "button",
        {
            {"default", "Default (L1 on Port 1)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-18",
        "Input::R Button Binding",
        "Input::R",
        "This button is typically used to navigate right in a menu. The stock RPG Maker runtimes bind this to the W key on the keyboard.",
        nullptr,
        "button",
        {
            {"default", "Default (R1 on Port 1)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-21",
        "Input::SHIFT Button Binding",
        "Input::SHIFT",
        "The stock RPG Maker runtimes bind this to the shift keys on the keyboard.",
        nullptr,
        "button",
        {
            {"default", "Default (R2 on Port 1)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-22",
        "Input::CTRL Button Binding",
        "Input::CTRL",
        "The stock RPG Maker runtimes bind this to the control keys on the keyboard.",
        nullptr,
        "button",
        {
            {"default", "Default (L2 on Port 1)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-23",
        "Input::ALT Button Binding",
        "Input::ALT",
        "The stock RPG Maker runtimes bind this to the alt keys on the keyboard.",
        nullptr,
        "button",
        {
            {"default", "Default (Start on Port 1)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-25",
        "Input::F5 Button Binding",
        "Input::F5",
        "The stock RPG Maker runtimes bind this to the F5 key on the keyboard.",
        nullptr,
        "button",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-26",
        "Input::F6 Button Binding",
        "Input::F6",
        "The stock RPG Maker runtimes bind this to the F6 key on the keyboard.",
        nullptr,
        "button",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-27",
        "Input::F7 Button Binding",
        "Input::F7",
        "The stock RPG Maker runtimes bind this to the F7 key on the keyboard.",
        nullptr,
        "button",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-28",
        "Input::F8 Button Binding",
        "Input::F8",
        "The stock RPG Maker runtimes bind this to the F8 key on the keyboard.",
        nullptr,
        "button",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-29",
        "Input::F9 Button Binding",
        "Input::F9",
        "The stock RPG Maker runtimes bind this to the F9 key on the keyboard.",
        nullptr,
        "button",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-38",
        "Input::MOUSELEFT Button Binding",
        "Input::MOUSELEFT",
        "This is typically bound to the primary mouse button.",
        nullptr,
        "button",
        {
            {"default", "Default (Primary Mouse Button on Port 1)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-39",
        "Input::MOUSEMIDDLE Button Binding",
        "Input::MOUSEMIDDLE",
        "This is typically bound to the middle mouse button.",
        nullptr,
        "button",
        {
            {"default", "Default (Middle Mouse Button on Port 1)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-40",
        "Input::MOUSERIGHT Button Binding",
        "Input::MOUSERIGHT",
        "This is typically bound to the secondary mouse button.",
        nullptr,
        "button",
        {
            {"default", "Default (Secondary Mouse Button on Port 1)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-41",
        "Input::MOUSEX1 Button Binding",
        "Input::MOUSEX1",
        "This is typically bound to mouse button 4.",
        nullptr,
        "button",
        {
            {"default", "Default (Mouse Button 4 on Port 1)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-42",
        "Input::MOUSEX2 Button Binding",
        "Input::MOUSEX2",
        "This is typically bound to mouse button 5.",
        nullptr,
        "button",
        {
            {"default", "Default (Mouse Button 5 on Port 1)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-2",
        "Input::DOWN Button Binding",
        "Input::DOWN",
        "The stock RPG Maker runtimes bind this to the down arrow key on the keyboard.",
        nullptr,
        "button",
        {
            {"default", "Default (Down on Port 1)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-4",
        "Input::LEFT Button Binding",
        "Input::LEFT",
        "The stock RPG Maker runtimes bind this to the left arrow key on the keyboard.",
        nullptr,
        "button",
        {
            {"default", "Default (Left on Port 1)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-6",
        "Input::RIGHT Button Binding",
        "Input::RIGHT",
        "The stock RPG Maker runtimes bind this to the right arrow key on the keyboard.",
        nullptr,
        "button",
        {
            {"default", "Default (Right on Port 1)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_button-8",
        "Input::UP Button Binding",
        "Input::UP",
        "The stock RPG Maker runtimes bind this to the up arrow key on the keyboard.",
        nullptr,
        "button",
        {
            {"default", "Default (Up on Port 1)"},
            {"none", "None"},
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-4",
        "A Scancode Binding",
        "A",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::X)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-5",
        "B Scancode Binding",
        "B",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-6",
        "C Scancode Binding",
        "C",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-7",
        "D Scancode Binding",
        "D",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::Z)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-8",
        "E Scancode Binding",
        "E",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-9",
        "F Scancode Binding",
        "F",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-10",
        "G Scancode Binding",
        "G",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-11",
        "H Scancode Binding",
        "H",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-12",
        "I Scancode Binding",
        "I",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-13",
        "J Scancode Binding",
        "J",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-14",
        "K Scancode Binding",
        "K",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-15",
        "L Scancode Binding",
        "L",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-16",
        "M Scancode Binding",
        "M",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-17",
        "N Scancode Binding",
        "N",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-18",
        "O Scancode Binding",
        "O",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-19",
        "P Scancode Binding",
        "P",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-20",
        "Q Scancode Binding",
        "Q",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::L)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-21",
        "R Scancode Binding",
        "R",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-22",
        "S Scancode Binding",
        "S",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::Y)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-23",
        "T Scancode Binding",
        "T",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-24",
        "U Scancode Binding",
        "U",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-25",
        "V Scancode Binding",
        "V",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-26",
        "W Scancode Binding",
        "W",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::R)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-27",
        "X Scancode Binding",
        "X",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::B)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-28",
        "Y Scancode Binding",
        "Y",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-29",
        "Z Scancode Binding",
        "Z",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::C)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-30",
        "NUMBER_1 Scancode Binding",
        "NUMBER_1",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-31",
        "NUMBER_2 Scancode Binding",
        "NUMBER_2",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-32",
        "NUMBER_3 Scancode Binding",
        "NUMBER_3",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-33",
        "NUMBER_4 Scancode Binding",
        "NUMBER_4",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-34",
        "NUMBER_5 Scancode Binding",
        "NUMBER_5",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-35",
        "NUMBER_6 Scancode Binding",
        "NUMBER_6",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-36",
        "NUMBER_7 Scancode Binding",
        "NUMBER_7",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-37",
        "NUMBER_8 Scancode Binding",
        "NUMBER_8",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-38",
        "NUMBER_9 Scancode Binding",
        "NUMBER_9",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-39",
        "NUMBER_0 Scancode Binding",
        "NUMBER_0",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-40",
        "RETURN Scancode Binding",
        "RETURN",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::C)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-41",
        "ESCAPE Scancode Binding",
        "ESCAPE",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::B)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-42",
        "BACKSPACE Scancode Binding",
        "BACKSPACE",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-43",
        "TAB Scancode Binding",
        "TAB",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-44",
        "SPACE Scancode Binding",
        "SPACE",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::C)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-45",
        "MINUS Scancode Binding",
        "MINUS",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-46",
        "EQUALS Scancode Binding",
        "EQUALS",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-47",
        "LEFTBRACKET Scancode Binding",
        "LEFTBRACKET",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-48",
        "RIGHTBRACKET Scancode Binding",
        "RIGHTBRACKET",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-49",
        "BACKSLASH Scancode Binding",
        "BACKSLASH",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-50",
        "NONUSHASH Scancode Binding",
        "NONUSHASH",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-51",
        "SEMICOLON Scancode Binding",
        "SEMICOLON",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-52",
        "APOSTROPHE Scancode Binding",
        "APOSTROPHE",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-53",
        "GRAVE Scancode Binding",
        "GRAVE",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-54",
        "COMMA Scancode Binding",
        "COMMA",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-55",
        "PERIOD Scancode Binding",
        "PERIOD",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-56",
        "SLASH Scancode Binding",
        "SLASH",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-57",
        "CAPSLOCK Scancode Binding",
        "CAPSLOCK",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-58",
        "F1 Scancode Binding",
        "F1",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-59",
        "F2 Scancode Binding",
        "F2",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-60",
        "F3 Scancode Binding",
        "F3",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-61",
        "F4 Scancode Binding",
        "F4",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-62",
        "F5 Scancode Binding",
        "F5",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::F5)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-63",
        "F6 Scancode Binding",
        "F6",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::F6)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-64",
        "F7 Scancode Binding",
        "F7",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::F7)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-65",
        "F8 Scancode Binding",
        "F8",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::F8)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-66",
        "F9 Scancode Binding",
        "F9",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::F9)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-67",
        "F10 Scancode Binding",
        "F10",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-68",
        "F11 Scancode Binding",
        "F11",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-69",
        "F12 Scancode Binding",
        "F12",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-70",
        "PRINTSCREEN Scancode Binding",
        "PRINTSCREEN",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-71",
        "SCROLLLOCK Scancode Binding",
        "SCROLLLOCK",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-72",
        "PAUSE Scancode Binding",
        "PAUSE",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-73",
        "INSERT Scancode Binding",
        "INSERT",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-74",
        "HOME Scancode Binding",
        "HOME",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-75",
        "PAGEUP Scancode Binding",
        "PAGEUP",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-76",
        "DELETE Scancode Binding",
        "DELETE",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-77",
        "END Scancode Binding",
        "END",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-78",
        "PAGEDOWN Scancode Binding",
        "PAGEDOWN",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-79",
        "RIGHT Scancode Binding",
        "RIGHT",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::RIGHT)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-80",
        "LEFT Scancode Binding",
        "LEFT",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::LEFT)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-81",
        "DOWN Scancode Binding",
        "DOWN",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::DOWN)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-82",
        "UP Scancode Binding",
        "UP",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::UP)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-83",
        "NUMLOCKCLEAR Scancode Binding",
        "NUMLOCKCLEAR",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-84",
        "KP_DIVIDE Scancode Binding",
        "KP_DIVIDE",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-85",
        "KP_MULTIPLY Scancode Binding",
        "KP_MULTIPLY",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-86",
        "KP_MINUS Scancode Binding",
        "KP_MINUS",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-87",
        "KP_PLUS Scancode Binding",
        "KP_PLUS",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-88",
        "KP_ENTER Scancode Binding",
        "KP_ENTER",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-89",
        "KP_1 Scancode Binding",
        "KP_1",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-90",
        "KP_2 Scancode Binding",
        "KP_2",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-91",
        "KP_3 Scancode Binding",
        "KP_3",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-92",
        "KP_4 Scancode Binding",
        "KP_4",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-93",
        "KP_5 Scancode Binding",
        "KP_5",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-94",
        "KP_6 Scancode Binding",
        "KP_6",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-95",
        "KP_7 Scancode Binding",
        "KP_7",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-96",
        "KP_8 Scancode Binding",
        "KP_8",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-97",
        "KP_9 Scancode Binding",
        "KP_9",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-98",
        "KP_0 Scancode Binding",
        "KP_0",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::B)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-99",
        "KP_PERIOD Scancode Binding",
        "KP_PERIOD",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-100",
        "NONUSBACKSLASH Scancode Binding",
        "NONUSBACKSLASH",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-101",
        "APPLICATION Scancode Binding",
        "APPLICATION",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-102",
        "POWER Scancode Binding",
        "POWER",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-103",
        "KP_EQUALS Scancode Binding",
        "KP_EQUALS",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-104",
        "F13 Scancode Binding",
        "F13",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-105",
        "F14 Scancode Binding",
        "F14",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-106",
        "F15 Scancode Binding",
        "F15",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-107",
        "F16 Scancode Binding",
        "F16",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-108",
        "F17 Scancode Binding",
        "F17",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-109",
        "F18 Scancode Binding",
        "F18",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-110",
        "F19 Scancode Binding",
        "F19",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-111",
        "F20 Scancode Binding",
        "F20",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-112",
        "F21 Scancode Binding",
        "F21",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-113",
        "F22 Scancode Binding",
        "F22",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-114",
        "F23 Scancode Binding",
        "F23",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-115",
        "F24 Scancode Binding",
        "F24",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-116",
        "EXECUTE Scancode Binding",
        "EXECUTE",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-117",
        "HELP Scancode Binding",
        "HELP",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-118",
        "MENU Scancode Binding",
        "MENU",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-119",
        "SELECT Scancode Binding",
        "SELECT",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-120",
        "STOP Scancode Binding",
        "STOP",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-121",
        "AGAIN Scancode Binding",
        "AGAIN",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-122",
        "UNDO Scancode Binding",
        "UNDO",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-123",
        "CUT Scancode Binding",
        "CUT",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-124",
        "COPY Scancode Binding",
        "COPY",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-125",
        "PASTE Scancode Binding",
        "PASTE",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-126",
        "FIND Scancode Binding",
        "FIND",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-127",
        "MUTE Scancode Binding",
        "MUTE",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-128",
        "VOLUMEUP Scancode Binding",
        "VOLUMEUP",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-129",
        "VOLUMEDOWN Scancode Binding",
        "VOLUMEDOWN",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-133",
        "KP_COMMA Scancode Binding",
        "KP_COMMA",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-134",
        "KP_EQUALSAS400 Scancode Binding",
        "KP_EQUALSAS400",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-135",
        "INTERNATIONAL1 Scancode Binding",
        "INTERNATIONAL1",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-136",
        "INTERNATIONAL2 Scancode Binding",
        "INTERNATIONAL2",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-137",
        "INTERNATIONAL3 Scancode Binding",
        "INTERNATIONAL3",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-138",
        "INTERNATIONAL4 Scancode Binding",
        "INTERNATIONAL4",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-139",
        "INTERNATIONAL5 Scancode Binding",
        "INTERNATIONAL5",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-140",
        "INTERNATIONAL6 Scancode Binding",
        "INTERNATIONAL6",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-141",
        "INTERNATIONAL7 Scancode Binding",
        "INTERNATIONAL7",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-142",
        "INTERNATIONAL8 Scancode Binding",
        "INTERNATIONAL8",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-143",
        "INTERNATIONAL9 Scancode Binding",
        "INTERNATIONAL9",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-144",
        "LANG1 Scancode Binding",
        "LANG1",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-145",
        "LANG2 Scancode Binding",
        "LANG2",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-146",
        "LANG3 Scancode Binding",
        "LANG3",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-147",
        "LANG4 Scancode Binding",
        "LANG4",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-148",
        "LANG5 Scancode Binding",
        "LANG5",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-149",
        "LANG6 Scancode Binding",
        "LANG6",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-150",
        "LANG7 Scancode Binding",
        "LANG7",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-151",
        "LANG8 Scancode Binding",
        "LANG8",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-152",
        "LANG9 Scancode Binding",
        "LANG9",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-153",
        "ALTERASE Scancode Binding",
        "ALTERASE",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-154",
        "SYSREQ Scancode Binding",
        "SYSREQ",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-155",
        "CANCEL Scancode Binding",
        "CANCEL",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-156",
        "CLEAR Scancode Binding",
        "CLEAR",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-157",
        "PRIOR Scancode Binding",
        "PRIOR",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-158",
        "RETURN2 Scancode Binding",
        "RETURN2",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-159",
        "SEPARATOR Scancode Binding",
        "SEPARATOR",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-160",
        "OUT Scancode Binding",
        "OUT",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-161",
        "OPER Scancode Binding",
        "OPER",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-162",
        "CLEARAGAIN Scancode Binding",
        "CLEARAGAIN",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-163",
        "CRSEL Scancode Binding",
        "CRSEL",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-164",
        "EXSEL Scancode Binding",
        "EXSEL",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-176",
        "KP_00 Scancode Binding",
        "KP_00",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-177",
        "KP_000 Scancode Binding",
        "KP_000",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-178",
        "THOUSANDSSEPARATOR Scancode Binding",
        "THOUSANDSSEPARATOR",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-179",
        "DECIMALSEPARATOR Scancode Binding",
        "DECIMALSEPARATOR",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-180",
        "CURRENCYUNIT Scancode Binding",
        "CURRENCYUNIT",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-181",
        "CURRENCYSUBUNIT Scancode Binding",
        "CURRENCYSUBUNIT",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-182",
        "KP_LEFTPAREN Scancode Binding",
        "KP_LEFTPAREN",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-183",
        "KP_RIGHTPAREN Scancode Binding",
        "KP_RIGHTPAREN",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-184",
        "KP_LEFTBRACE Scancode Binding",
        "KP_LEFTBRACE",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-185",
        "KP_RIGHTBRACE Scancode Binding",
        "KP_RIGHTBRACE",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-186",
        "KP_TAB Scancode Binding",
        "KP_TAB",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-187",
        "KP_BACKSPACE Scancode Binding",
        "KP_BACKSPACE",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-188",
        "KP_A Scancode Binding",
        "KP_A",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-189",
        "KP_B Scancode Binding",
        "KP_B",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-190",
        "KP_C Scancode Binding",
        "KP_C",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-191",
        "KP_D Scancode Binding",
        "KP_D",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-192",
        "KP_E Scancode Binding",
        "KP_E",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-193",
        "KP_F Scancode Binding",
        "KP_F",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-194",
        "KP_XOR Scancode Binding",
        "KP_XOR",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-195",
        "KP_POWER Scancode Binding",
        "KP_POWER",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-196",
        "KP_PERCENT Scancode Binding",
        "KP_PERCENT",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-197",
        "KP_LESS Scancode Binding",
        "KP_LESS",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-198",
        "KP_GREATER Scancode Binding",
        "KP_GREATER",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-199",
        "KP_AMPERSAND Scancode Binding",
        "KP_AMPERSAND",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-200",
        "KP_DBLAMPERSAND Scancode Binding",
        "KP_DBLAMPERSAND",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-201",
        "KP_VERTICALBAR Scancode Binding",
        "KP_VERTICALBAR",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-202",
        "KP_DBLVERTICALBAR Scancode Binding",
        "KP_DBLVERTICALBAR",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-203",
        "KP_COLON Scancode Binding",
        "KP_COLON",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-204",
        "KP_HASH Scancode Binding",
        "KP_HASH",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-205",
        "KP_SPACE Scancode Binding",
        "KP_SPACE",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-206",
        "KP_AT Scancode Binding",
        "KP_AT",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-207",
        "KP_EXCLAM Scancode Binding",
        "KP_EXCLAM",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-208",
        "KP_MEMSTORE Scancode Binding",
        "KP_MEMSTORE",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-209",
        "KP_MEMRECALL Scancode Binding",
        "KP_MEMRECALL",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-210",
        "KP_MEMCLEAR Scancode Binding",
        "KP_MEMCLEAR",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-211",
        "KP_MEMADD Scancode Binding",
        "KP_MEMADD",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-212",
        "KP_MEMSUBTRACT Scancode Binding",
        "KP_MEMSUBTRACT",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-213",
        "KP_MEMMULTIPLY Scancode Binding",
        "KP_MEMMULTIPLY",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-214",
        "KP_MEMDIVIDE Scancode Binding",
        "KP_MEMDIVIDE",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-215",
        "KP_PLUSMINUS Scancode Binding",
        "KP_PLUSMINUS",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-216",
        "KP_CLEAR Scancode Binding",
        "KP_CLEAR",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-217",
        "KP_CLEARENTRY Scancode Binding",
        "KP_CLEARENTRY",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-218",
        "KP_BINARY Scancode Binding",
        "KP_BINARY",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-219",
        "KP_OCTAL Scancode Binding",
        "KP_OCTAL",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-220",
        "KP_DECIMAL Scancode Binding",
        "KP_DECIMAL",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-221",
        "KP_HEXADECIMAL Scancode Binding",
        "KP_HEXADECIMAL",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-224",
        "LCTRL Scancode Binding",
        "LCTRL",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::CTRL)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-225",
        "LSHIFT Scancode Binding",
        "LSHIFT",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::A)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-226",
        "LALT Scancode Binding",
        "LALT",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::ALT)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-227",
        "LGUI Scancode Binding",
        "LGUI",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-228",
        "RCTRL Scancode Binding",
        "RCTRL",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::CTRL)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-229",
        "RSHIFT Scancode Binding",
        "RSHIFT",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::SHIFT)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-230",
        "RALT Scancode Binding",
        "RALT",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (Same as Input::ALT)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-231",
        "RGUI Scancode Binding",
        "RGUI",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-257",
        "MODE Scancode Binding",
        "MODE",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-258",
        "AUDIONEXT Scancode Binding",
        "AUDIONEXT",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-259",
        "AUDIOPREV Scancode Binding",
        "AUDIOPREV",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-260",
        "AUDIOSTOP Scancode Binding",
        "AUDIOSTOP",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-261",
        "AUDIOPLAY Scancode Binding",
        "AUDIOPLAY",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-262",
        "AUDIOMUTE Scancode Binding",
        "AUDIOMUTE",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-263",
        "MEDIASELECT Scancode Binding",
        "MEDIASELECT",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-264",
        "WWW Scancode Binding",
        "WWW",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-265",
        "MAIL Scancode Binding",
        "MAIL",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-266",
        "CALCULATOR Scancode Binding",
        "CALCULATOR",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-267",
        "COMPUTER Scancode Binding",
        "COMPUTER",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-268",
        "AC_SEARCH Scancode Binding",
        "AC_SEARCH",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-269",
        "AC_HOME Scancode Binding",
        "AC_HOME",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-270",
        "AC_BACK Scancode Binding",
        "AC_BACK",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-271",
        "AC_FORWARD Scancode Binding",
        "AC_FORWARD",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-272",
        "AC_STOP Scancode Binding",
        "AC_STOP",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-273",
        "AC_REFRESH Scancode Binding",
        "AC_REFRESH",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-274",
        "AC_BOOKMARKS Scancode Binding",
        "AC_BOOKMARKS",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-275",
        "BRIGHTNESSDOWN Scancode Binding",
        "BRIGHTNESSDOWN",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-276",
        "BRIGHTNESSUP Scancode Binding",
        "BRIGHTNESSUP",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-277",
        "DISPLAYSWITCH Scancode Binding",
        "DISPLAYSWITCH",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-278",
        "KBDILLUMTOGGLE Scancode Binding",
        "KBDILLUMTOGGLE",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-279",
        "KBDILLUMDOWN Scancode Binding",
        "KBDILLUMDOWN",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-280",
        "KBDILLUMUP Scancode Binding",
        "KBDILLUMUP",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-281",
        "EJECT Scancode Binding",
        "EJECT",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-282",
        "SLEEP Scancode Binding",
        "SLEEP",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-283",
        "APP1 Scancode Binding",
        "APP1",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-284",
        "APP2 Scancode Binding",
        "APP2",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-285",
        "AUDIOREWIND Scancode Binding",
        "AUDIOREWIND",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-286",
        "AUDIOFASTFORWARD Scancode Binding",
        "AUDIOFASTFORWARD",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-287",
        "SOFTLEFT Scancode Binding",
        "SOFTLEFT",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-288",
        "SOFTRIGHT Scancode Binding",
        "SOFTRIGHT",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-289",
        "CALL Scancode Binding",
        "CALL",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_scancode-290",
        "ENDCALL Scancode Binding",
        "ENDCALL",
        nullptr,
        nullptr,
        "scancode",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_controller-0",
        "A Controller Binding",
        "A",
        "The A button on the controller, used to confirm an action. This is the cross button on a PlayStation controller.",
        nullptr,
        "controller",
        {
            {"default", "Default (A on Port 1)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_controller-1",
        "B Controller Binding",
        "B",
        "The B button on the controller, used to cancel an action. This is the circle button on a PlayStation controller.",
        nullptr,
        "controller",
        {
            {"default", "Default (B on Port 1)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_controller-2",
        "X Controller Binding",
        "X",
        "The X button on the controller. This is the square button on a PlayStation controller.",
        nullptr,
        "controller",
        {
            {"default", "Default (X on Port 1)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_controller-3",
        "Y Controller Binding",
        "Y",
        "The Y button on the controller. This is the triangle button on a PlayStation controller.",
        nullptr,
        "controller",
        {
            {"default", "Default (Y on Port 1)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_controller-4",
        "BACK Controller Binding",
        "BACK",
        "The select or back button on the controller.",
        nullptr,
        "controller",
        {
            {"default", "Default (Select on Port 1)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_controller-5",
        "GUIDE Controller Binding",
        "GUIDE",
        "The button on the controller that causes a game console to open its system menu.",
        nullptr,
        "controller",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_controller-6",
        "START Controller Binding",
        "START",
        "The start or forward button on the controller.",
        nullptr,
        "controller",
        {
            {"default", "Default (Start on Port 1)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_controller-7",
        "LEFTSTICK Controller Binding",
        "LEFTSTICK",
        "The L3 button on the controller, typically triggered by pressing on the left analog stick.",
        nullptr,
        "controller",
        {
            {"default", "Default (L3 on Port 1)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_controller-8",
        "RIGHTSTICK Controller Binding",
        "RIGHTSTICK",
        "The R3 button on the controller, typically triggered by pressing on the right analog stick.",
        nullptr,
        "controller",
        {
            {"default", "Default (R3 on Port 1)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_controller-9",
        "LEFTSHOULDER Controller Binding",
        "LEFTSHOULDER",
        "The left shoulder button or L1 button on the controller.",
        nullptr,
        "controller",
        {
            {"default", "Default (L1 on Port 1)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_controller-10",
        "RIGHTSHOULDER Controller Binding",
        "RIGHTSHOULDER",
        "The right shoulder button or R1 button on the controller.",
        nullptr,
        "controller",
        {
            {"default", "Default (R1 on Port 1)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_controller-11",
        "DPAD_UP Controller Binding",
        "DPAD_UP",
        "The up button on the controller's directional pad.",
        nullptr,
        "controller",
        {
            {"default", "Default (Up on Port 1)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_controller-12",
        "DPAD_DOWN Controller Binding",
        "DPAD_DOWN",
        "The down button on the controller's directional pad.",
        nullptr,
        "controller",
        {
            {"default", "Default (Down on Port 1)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_controller-13",
        "DPAD_LEFT Controller Binding",
        "DPAD_LEFT",
        "The left button on the controller's directional pad.",
        nullptr,
        "controller",
        {
            {"default", "Default (Left on Port 1)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_controller-14",
        "DPAD_RIGHT Controller Binding",
        "DPAD_RIGHT",
        "The right button on the controller's directional pad.",
        nullptr,
        "controller",
        {
            {"default", "Default (Right on Port 1)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_controller-15",
        "MISC1 Controller Binding",
        "MISC1",
        "Typically the button on the controller that captures a screenshot or video recording of gameplay.",
        nullptr,
        "controller",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_controller-16",
        "PADDLE1 Controller Binding",
        "PADDLE1",
        "The top left paddle button on an Xbox controller or the left trigger button on other controllers.",
        nullptr,
        "controller",
        {
            {"default", "Default (L2 on Port 1)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_controller-17",
        "PADDLE2 Controller Binding",
        "PADDLE2",
        "The top right paddle button on an Xbox controller or the right trigger button on other controllers.",
        nullptr,
        "controller",
        {
            {"default", "Default (R2 on Port 1)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_controller-18",
        "PADDLE3 Controller Binding",
        "PADDLE3",
        "The bottom left paddle button on an Xbox controller.",
        nullptr,
        "controller",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_controller-19",
        "PADDLE4 Controller Binding",
        "PADDLE4",
        "The bottom right paddle button on an Xbox controller.",
        nullptr,
        "controller",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        "mkxp-z_controller-20",
        "TOUCHPAD Controller Binding",
        "TOUCHPAD",
        "This corresponds to touching on the touchpad of a PlayStation DualShock controller or other controller with a similar touchpad.",
        nullptr,
        "controller",
        {
            {"default", "Default (None)"},
            {"none", "None"},
            BUTTON_OPTIONS
            RETROPAD_OPTIONS
            {nullptr, nullptr},
        },
        "default",
    },
    {
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        {{nullptr, nullptr}},
        nullptr,
    },
};

#undef BUTTON_OPTIONS
#undef RETROPAD_OPIONS

#endif // MKXPZ_CORE_OPTIONS_H
