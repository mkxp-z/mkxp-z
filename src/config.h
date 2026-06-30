/*
 ** config.h
 **
 ** This file is part of mkxp.
 **
 ** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
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

#ifndef CONFIG_H
#define CONFIG_H

#include "util/json5pp.hpp"

#include <set>
#include <string>
#include <type_traits>
#include <vector>

template <typename T> struct OverridableConfigOptionOperatorSet {
    static inline T apply(T value, T override) noexcept {
        return override;
    }
};

template <typename T> struct OverridableConfigOptionOperatorMaximum {
    static inline T apply(T value, T override) noexcept {
        return std::max(value, override);
    }
};

template <typename T> struct OverridableConfigOptionOperatorMultiply {
    static inline T apply(T value, T override) noexcept {
        return value * override;
    }
};

template <typename T, template <typename U> class Operator = OverridableConfigOptionOperatorSet> struct OverridableConfigOption {
    static_assert(std::is_arithmetic<T>::value, "overridable config options must be numeric");

    OverridableConfigOption() noexcept : overrideActive(false) {};

    OverridableConfigOption(T value) noexcept : value(value), overrideActive(false) {};

    T get() const noexcept {
        return overrideActive ? Operator<T>::apply(value, override) : value;
    }

    void set(T value) noexcept {
        this->value = value;
    }

    void setOverride(T override) noexcept {
        this->override = override;
        this->overrideActive = true;
    }

    void clearOverride() noexcept {
        this->overrideActive = false;
    }

    inline operator T() const noexcept {
        return get();
    }

    inline void operator=(T value) noexcept {
        set(value);
    }

private:
    T value;
    T override;
    bool overrideActive;
};

struct Config {
    // Used for sending the JSON data to Ruby as System::CONFIG
    json5pp::value raw;
    
    int rgssVersion;
    
    bool debugMode;
#ifndef MKXPZ_RETRO
    bool winConsole;
#endif // MKXPZ_RETRO
    bool preferMetalRenderer;
    bool displayFPS;
    bool printFPS;
    
    bool winResizable;
    bool fullscreen;
    bool fixedAspectRatio;
    int smoothScaling;
    int smoothScalingDown;
    int bitmapSmoothScaling;
    int bitmapSmoothScalingDown;
    bool smoothScalingMipmaps;
    int bicubicSharpness;
#ifdef MKXPZ_SSL
    double xbrzScalingFactor;
#endif
    bool enableHires;
    double textureScalingFactor;
    double framebufferScalingFactor;
    double atlasScalingFactor;
    bool vsync;
    
    int defScreenW;
    int defScreenH;
    std::string windowTitle;
    
    int fixedFramerate;
    OverridableConfigOption<bool> frameSkip;
    bool syncToRefreshrate;
    
    std::vector<std::string> solidFonts;
    
    OverridableConfigOption<bool> subImageFix;
    OverridableConfigOption<bool> enableBlitting;
    int maxTextureSize;
    
    struct {
        bool active;
        bool lastMileScaling;
    } integerScaling;
    
    std::string gameFolder;
#ifndef MKXPZ_RETRO
    bool manualFolderSelect;
#endif // MKXPZ_RETRO
    
    bool anyAltToggleFS;
    bool enableReset;
    bool enableSettings;
    bool allowSymlinks;
    bool pathCache;
    
    std::string dataPathOrg;
    std::string dataPathApp;
    
    std::string iconPath;
    std::string execName;
    std::string titleLanguage;
    
    struct {
        std::string soundFont;
        OverridableConfigOption<bool> chorus;
        OverridableConfigOption<bool> reverb;
    } midi;
    
    struct {
        OverridableConfigOption<int, OverridableConfigOptionOperatorMaximum> sourceCount;
    } SE;
    
    struct {
        int trackCount;
    } BGM;
    
    bool useScriptNames;
    
    std::string customScript;
    
    std::vector<std::string> launchArgs;
    std::vector<std::string> preloadScripts;
    std::vector<std::string> postloadScripts;
    std::vector<std::string> rtps;
    std::vector<std::string> patches;
    
    std::vector<std::string> fontSubs;
    OverridableConfigOption<float, OverridableConfigOptionOperatorMultiply> fontScale;
    OverridableConfigOption<bool> fontKerning;
    OverridableConfigOption<int> fontHinting;
    OverridableConfigOption<int> fontHeightReporting;
    OverridableConfigOption<bool> fontOutlineCrop;
    
    std::vector<std::string> rubyLoadpaths;

    /* Editor flags */
    struct {
        bool debug;
        bool battleTest;
    } editor;
    
    /* Game INI contents */
    struct {
        std::string scripts;
        std::string title;
        std::vector<std::string> rtps;
    } game;
    
    // MJIT Options
    struct {
        bool enabled;
        int verboseLevel;
        int maxCache;
        int minCalls;
    } jit;
    
    // YJIT Options
    struct {
        bool enabled;
    } yjit;

    bool dumpAtlas;

    // Keybinding action name mappings
    struct {
        std::string a;
        std::string b;
        std::string c;
        
        std::string x;
        std::string y;
        std::string z;
        
        std::string l;
        std::string r;
    } kbActionNames;
    
    std::string userConfPath;
    
    /* Internal */
#ifndef MKXPZ_RETRO
    std::string customDataPath;
#endif // MKXPZ_RETRO
    bool loadFontsIntoMemory;
    
    Config();
    
    bool fontIsSolid(const char *fontName) const;
    
    void read(int argc, char *argv[], int forceRgssVersion = -1);
    void readGameINI();
};

#endif // CONFIG_H
