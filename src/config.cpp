//
//  config.cpp
//  Player
//
//  Created by ゾロアーク on 11/21/20.
//

#include "config.h"
#ifdef MKXPZ_RETRO
#  include "core.h"
#else
#  include <SDL_filesystem.h>
#endif // MKXPZ_RETRO
#include <assert.h>

#include <stdint.h>
#include <vector>

#include "filesystem/filesystem.h"
#include "util/exception.h"
#include "util/debugwriter.h"
#include "util/sdl-util.h"
#include "util/util.h"

#include "util/json5pp.hpp"

#include "util/iniconfig.h"
#include "util/encoding.h"

#include "system/system.h"


namespace json = json5pp;

#ifndef MKXPZ_RETRO
static std::string prefPath(const char *org, const char *app) {
    char *path = SDL_GetPrefPath(org, app);
    if (!path)
        return std::string("");
    std::string ret(path);
    SDL_free(path);
    return ret;
}
#endif // MKXPZ_RETRO

static void fillStringVec(json::value &item, std::vector<std::string> &vector) {
    bool failure = false;
    if (!item.is_array()) {
        if (item.is_string()) {
            vector.push_back(item.as_string(failure));
            assert(!failure);
        }
        return;
    }
    auto &array = item.as_array(failure);
    assert(!failure);
    for (size_t i = 0; i < array.size(); i++) {
        if (!array[i].is_string())
            continue;
        vector.push_back(array[i].as_string(failure));
        assert(!failure);
    }
}

static bool copyObject(json::value &dest, json::value &src, const char *objectName = "") {
    assert(dest.is_object());
    if (src.is_null())
        return false;
    
    if (!src.is_object())
        return false;
    
    bool failure = false;
    auto &srcVec = src.as_object(failure);
    assert(!failure);
    auto &destVec = dest.as_object(failure);
    assert(!failure);
    
    for (auto it : srcVec) {
        // Specifically processs this object later.
        if (it.second.is_object() && destVec[it.first].is_object())
            continue;
        
        if ((it.second.is_array() && destVec[it.first].is_array())    ||
            (it.second.is_number() && destVec[it.first].is_number())  ||
            (it.second.is_string() && destVec[it.first].is_string())  ||
            (it.second.is_boolean() && destVec[it.first].is_boolean()) ||
            (destVec[it.first].is_null()))
        {
            destVec[it.first] = it.second;
        }
        else {
            Debug() << "Invalid variable in configuration:" << objectName << it.first;
        }
    }
    return true;
}

#ifndef MKXPZ_RETRO
static bool getEnvironmentBool(const char *env, bool defaultValue) {
    const char *e = SDL_getenv(env);
    if (!e)
        return defaultValue;
    
    if (!strcmp(e, "0"))
        return false;
    else if (!strcmp(e, "1"))
        return true;
    
    return defaultValue;
}
#endif // MKXPZ_RETRO

static json::value readConfFile(const char *path) {
    
    json::value ret(0);

#ifdef MKXPZ_RETRO
    FileSystem::File file(*mkxp_retro::fs, path);
    if (!file.is_read_open())
#else
    if (!mkxp_fs::fileExists(path))
#endif // MKXPZ_RETRO
    {
        return json::object({});
    }
    
#ifdef MKXPZ_RETRO
    std::vector<uint8_t> buf(16);
    size_t size = 0;
    for (;;) {
        PHYSFS_sint64 n = PHYSFS_readBytes(file.get_read(), buf.data() + size, buf.size() - size);
        if (n <= 0) {
            break;
        }
        size += n;
        if (size >= buf.size()) {
            buf.resize(buf.size() * 2);
        }
    }
    std::string cfg(buf.begin(), buf.begin() + size);
#else
    std::string cfg;
    try {
        cfg = mkxp_fs::contentsOfFileAsString(path);
    }
    catch (const Exception &e) {
        Debug() << "Failed to parse" << path << ":" << "Unknown encoding";
    }
#endif // MKXPZ_RETRO
    if (!cfg.empty()) {
        cfg = Encoding::convertString(cfg);
        if (cfg.empty()) {
            Debug() << "Failed to parse" << path << ":" << "Unknown encoding";
        } else {
            json::failure = false;
            ret = json::parse5(Encoding::convertString(cfg));
            if (json::failure) {
                Debug() << "Failed to parse" << path << ":" << (json::failure.error() == nullptr ? "bad cast" : json::failure.error()->what());
            }
        }
    }
    
    if (!ret.is_object())
        ret = json::object({});
    
    return ret;
}

#define CONF_FILE "mkxp.json"

Config::Config() :
    loadFontsIntoMemory(false)
{}

void Config::read(int argc, char *argv[], int forceRgssVersion) {
    auto optsJ = json::object({
        {"rgssVersion", 0},
        {"debugMode", false},
        {"displayFPS", false},
        {"printFPS", false},
        {"winResizable", true},
        {"fullscreen", false},
        {"fixedAspectRatio", true},
        {"smoothScaling", 0},
        {"smoothScalingDown", 0},
        {"bitmapSmoothScaling", 0},
        {"bitmapSmoothScalingDown", 0},
        {"smoothScalingMipmaps", false},
        {"bicubicSharpness", 100},
#ifdef MKXPZ_SSL
        {"xbrzScalingFactor", 1.},
#endif
        {"enableHires", false},
        {"textureScalingFactor", 1.},
        {"framebufferScalingFactor", 1.},
        {"atlasScalingFactor", 1.},
        {"vsync", false},
        {"defScreenW", 0},
        {"defScreenH", 0},
        {"windowTitle", ""},
        {"fixedFramerate", 0},
        {"frameSkip", false},
        {"syncToRefreshrate", false},
        {"solidFonts", json::array({})},
#if defined(__APPLE__) && defined(__aarch64__)
        {"preferMetalRenderer", true},
#else
        {"preferMetalRenderer", false},
#endif
#ifdef MKXPZ_RETRO
#ifdef __DEVKITA64__
        {"subImageFix", true},
#else
        {"subImageFix", mkxp_retro::hw_render.context_type == RETRO_HW_CONTEXT_OPENGLES2 || mkxp_retro::hw_render.context_type == RETRO_HW_CONTEXT_OPENGLES3 || mkxp_retro::hw_render.context_type == RETRO_HW_CONTEXT_OPENGLES_VERSION},
#endif // __DEVKITA64__
#else
        {"subImageFix", false},
#endif // MKXPZ_RETRO
#ifdef __WIN32
        {"enableBlitting", false},
#else
        {"enableBlitting", true},
#endif
        {"integerScalingActive", false},
        {"integerScalingLastMile", true},
        {"maxTextureSize", 0},
        {"gameFolder", ""},
        {"anyAltToggleFS", false},
        {"enableReset", true},
        {"enableSettings", true},
        {"allowSymlinks", true},
        {"dataPathOrg", ""},
        {"dataPathApp", ""},
        {"iconPath", ""},
        {"execName", "Game"},
        {"midiSoundFont", ""},
        {"midiChorus", false},
        {"midiReverb", false},
        {"SESourceCount", 6},
        {"BGMTrackCount", 1},
        {"customScript", ""},
        {"pathCache", true},
        {"useScriptNames", true},
        {"preloadScript", json::array({})},
        {"postloadScript", json::array({})},
        {"RTP", json::array({})},
        {"patches", json::array({})},
        {"fontSub", json::array({})},
        {"fontScale", 1.0f},
        {"fontKerning", true},
        {"fontHinting", 3}, // TTF_HINTING_NONE
        {"fontHeightReporting", 0},
        {"fontOutlineCrop", true},
        {"rubyLoadpath", json::array({})},
        {"JITEnable", false},
        {"JITVerboseLevel", 0},
        {"JITMaxCache", 100},
        {"JITMinCalls", 10000},
        {"YJITEnable", false},
        {"dumpAtlas", false},
        {"bindingNames", json::object({
            {"a", "A"},
            {"b", "B"},
            {"c", "C"},
            {"x", "X"},
            {"y", "Y"},
            {"z", "Z"},
            {"l", "L"},
            {"r", "R"}
        })}
    });
    
    bool failure = false;
    auto &opts = optsJ.as_object(failure);
    
#ifndef MKXPZ_RETRO
    editor.debug = false;
    editor.battleTest = false;
    
    if (argc > 1) {
        if (!strcmp(argv[1], "debug") || !strcmp(argv[1], "test"))
            editor.debug = true;
        else if (!strcmp(argv[1], "btest"))
            editor.battleTest = true;
        
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "debug"))
                launchArgs.push_back(argv[i]);
        }
    }
#endif // MKXPZ_RETRO
    
    json::value baseConf = readConfFile(CONF_FILE);
    copyObject(optsJ, baseConf);
    copyObject(opts["bindingNames"], baseConf.as_object(failure)["bindingNames"], "bindingNames .");
    
#define SET_OPT_CUSTOMKEY(var, key, type) var = opts[#key].as_##type(failure)
#define SET_OPT(var, type) SET_OPT_CUSTOMKEY(var, var, type)
#define SET_STRINGOPT(var, key) var = std::string(opts[#key].as_string(failure))
    
    SET_STRINGOPT(gameFolder, gameFolder);
    SET_STRINGOPT(dataPathOrg, dataPathOrg);
    SET_STRINGOPT(dataPathApp, dataPathApp);
    SET_STRINGOPT(iconPath, iconPath);
    SET_STRINGOPT(execName, execName);
    SET_OPT(allowSymlinks, boolean);
    SET_OPT(pathCache, boolean);
    SET_OPT_CUSTOMKEY(jit.enabled, JITEnable, boolean);
    SET_OPT_CUSTOMKEY(jit.verboseLevel, JITVerboseLevel, integer);
    SET_OPT_CUSTOMKEY(jit.maxCache, JITMaxCache, integer);
    SET_OPT_CUSTOMKEY(jit.minCalls, JITMinCalls, integer);
    SET_OPT_CUSTOMKEY(yjit.enabled, YJITEnable, boolean);
    SET_OPT(rgssVersion, integer);
    SET_OPT(defScreenW, integer);
    SET_OPT(defScreenH, integer);
    
#ifndef MKXPZ_RETRO
    // Take a break real quick and witch to set game folder and read the game's ini
    if (!gameFolder.empty() && !mkxp_fs::setCurrentDirectory(gameFolder.c_str())) {
        throw Exception(Exception::MKXPError, "Unable to switch into gameFolder %s", gameFolder.c_str());
    }
#endif // MKXPZ_RETRO
    
    if (forceRgssVersion >= 0) {
        rgssVersion = forceRgssVersion;
    }
    readGameINI();
    
#ifndef MKXPZ_RETRO
    // Now check for an extra mkxp.conf in the user's save directory and merge anything else from that
    userConfPath = mkxp_fs::normalizePath(std::string(customDataPath + "/" CONF_FILE).c_str(), 0, 1);
    json::value userConf = readConfFile(userConfPath.c_str());
    copyObject(optsJ, userConf);
#endif // MKXPZ_RETRO
    
    // now RESUME
    
    SET_OPT(debugMode, boolean);
    SET_OPT(displayFPS, boolean);
    SET_OPT(printFPS, boolean);
    SET_OPT(fullscreen, boolean);
    SET_OPT(fixedAspectRatio, boolean);
    SET_OPT(smoothScaling, integer);
    SET_OPT(smoothScalingDown, integer);
    SET_OPT(bitmapSmoothScaling, integer);
    SET_OPT(bitmapSmoothScalingDown, integer);
    SET_OPT(smoothScalingMipmaps, boolean);
    SET_OPT(bicubicSharpness, integer);
#ifdef MKXPZ_SSL
    SET_OPT(xbrzScalingFactor, integer);
#endif
    SET_OPT(enableHires, boolean);
    SET_OPT(textureScalingFactor, number);
    SET_OPT(framebufferScalingFactor, number);
    SET_OPT(atlasScalingFactor, number);
    SET_OPT(winResizable, boolean);
    SET_OPT(vsync, boolean);
    SET_STRINGOPT(windowTitle, windowTitle);
    SET_OPT(fixedFramerate, integer);
    SET_OPT(frameSkip, boolean);
    SET_OPT(syncToRefreshrate, boolean);
    fillStringVec(opts["solidFonts"], solidFonts);
    for (std::string & solidFont : solidFonts)
        std::transform(solidFont.begin(), solidFont.end(), solidFont.begin(),
            [](unsigned char c) { return std::tolower(c); });
#ifdef __APPLE__
    SET_OPT(preferMetalRenderer, boolean);
#endif
    SET_OPT(subImageFix, boolean);
    SET_OPT(enableBlitting, boolean);
    SET_OPT_CUSTOMKEY(integerScaling.active, integerScalingActive, boolean);
    SET_OPT_CUSTOMKEY(integerScaling.lastMileScaling, integerScalingLastMile, boolean);
    SET_OPT(maxTextureSize, integer);
    SET_OPT(anyAltToggleFS, boolean);
    SET_OPT(enableReset, boolean);
    SET_OPT(enableSettings, boolean);
    SET_STRINGOPT(midi.soundFont, midiSoundFont);
    SET_OPT_CUSTOMKEY(midi.chorus, midiChorus, boolean);
    SET_OPT_CUSTOMKEY(midi.reverb, midiReverb, boolean);
    SET_OPT_CUSTOMKEY(SE.sourceCount, SESourceCount, integer);
    SET_OPT_CUSTOMKEY(BGM.trackCount, BGMTrackCount, integer);
    SET_STRINGOPT(customScript, customScript);
    SET_OPT(useScriptNames, boolean);
    SET_OPT(dumpAtlas, boolean);
    
    fillStringVec(opts["preloadScript"], preloadScripts);
    fillStringVec(opts["postloadScript"], postloadScripts);
    fillStringVec(opts["RTP"], rtps);
    fillStringVec(opts["patches"], patches);
    fillStringVec(opts["fontSub"], fontSubs);
    for (std::string & fontSub : fontSubs)
        std::transform(fontSub.begin(), fontSub.end(), fontSub.begin(),
            [](unsigned char c) { return std::tolower(c); });
    SET_OPT(fontScale, number);
    SET_OPT(fontKerning, boolean);
    SET_OPT(fontHinting, integer);
    SET_OPT(fontHeightReporting, integer);
    SET_OPT(fontOutlineCrop, boolean);
    fillStringVec(opts["rubyLoadpath"], rubyLoadpaths);
    
    auto &bnames = opts["bindingNames"].as_object(failure);
    
#define BINDING_NAME(btn) kbActionNames.btn = bnames[#btn].as_string(failure)
    BINDING_NAME(a);
    BINDING_NAME(b);
    BINDING_NAME(c);
    BINDING_NAME(x);
    BINDING_NAME(y);
    BINDING_NAME(z);
    BINDING_NAME(l);
    BINDING_NAME(r);
    
    rgssVersion = clamp(rgssVersion, 0, 3);
    SE.sourceCount = clamp<int>(SE.sourceCount, 1, 64);
    BGM.trackCount = clamp(BGM.trackCount, 1, 16);
    
#ifndef MKXPZ_RETRO
    // Determine whether to open a console window on... Windows
    winConsole = getEnvironmentBool("MKXPZ_WINDOWS_CONSOLE", editor.debug);
    
#ifdef __APPLE__
    // Determine whether to use the Metal renderer on macOS
    // Environment variable takes priority over the json setting
    preferMetalRenderer = isMetalSupported() && getEnvironmentBool("MKXPZ_MACOS_METAL", preferMetalRenderer);
#endif
    
    // Determine whether to allow manual selection of a game folder on startup
    // Only works on macOS atm, mainly used to test games located outside of the bundle.
    // The config is re-read after the window is already created, so some entries
    // may not take effect
    manualFolderSelect = getEnvironmentBool("MKXPZ_FOLDER_SELECT", false);
#endif // MKXPZ_RETRO
    
    raw = optsJ;
}

static void setupScreenSize(Config &conf) {
    if (conf.defScreenW <= 0)
        conf.defScreenW = (conf.rgssVersion == 1 ? 640 : 544);
    
    if (conf.defScreenH <= 0)
        conf.defScreenH = (conf.rgssVersion == 1 ? 480 : 416);
}

bool Config::fontIsSolid(const char *fontName) const {
    for (std::string solidfont : solidFonts)
        if (!strcmp(solidfont.c_str(), fontName)) return true;
    
    return false;
}

void Config::readGameINI() {
    if (!customScript.empty()) {
        game.title = customScript.c_str();
        
        if (rgssVersion == 0)
            rgssVersion = 1;
        
        setupScreenSize(*this);
        
        return;
    }
    
#ifdef MKXPZ_RETRO
    std::string iniFileName(execName + ".ini");
    std::shared_ptr<FileSystem::File> iniFile(new FileSystem::File(*mkxp_retro::fs, iniFileName.c_str()));
#else
    std::string iniFileName(execName + ".ini");
    SDLRWStream iniFile(iniFileName.c_str(), "r");
#endif // MKXPZ_RETRO
    
    bool convSuccess = true;
#ifdef MKXPZ_RETRO
    if (iniFile->is_read_open())
#else
    if (iniFile)
#endif // MKXPZ_RETRO
    {
        INIConfiguration ic;
#ifdef MKXPZ_RETRO
        PHYSFSRWBuf buf(iniFile);
        std::istream stream(&buf);
        if (ic.load(stream))
#else
        if (ic.load(iniFile.stream()))
#endif // MKXPZ_RETRO
        {
            game.title = ic.getStringProperty("Game", "Title");
            game.scripts = ic.getStringProperty("Game", "Scripts");
            
            strReplace(game.scripts, '\\', '/');
            
            if (game.title.empty()) {
                Debug() << iniFileName + ": Could not find Game.Title";
            }
            
            if (game.scripts.empty())
                Debug() << iniFileName + ": Could not find Game.Scripts";

            std::string rtp;
            rtp = ic.getStringProperty("Game", "RTP");
            if (!rtp.empty()) {
                game.rtps.push_back(rtp);
            }
            rtp = ic.getStringProperty("Game", "RTP1");
            if (!rtp.empty()) {
                game.rtps.push_back(rtp);
            }
            rtp = ic.getStringProperty("Game", "RTP2");
            if (!rtp.empty()) {
                game.rtps.push_back(rtp);
            }
            rtp = ic.getStringProperty("Game", "RTP3");
            if (!rtp.empty()) {
                game.rtps.push_back(rtp);
            }
        }
    }
    else
        Debug() << "Could not read" << iniFileName;
    
    if (!game.title.empty()) {
        game.title = Encoding::convertString(game.title);
        if (game.title.empty()) {
            Debug() << iniFileName + ": Could not determine encoding of Game.Title";
        }
    }
    
    if (game.title.empty() || !convSuccess)
        game.title = "mkxp-z";
    
    if (dataPathOrg.empty())
        dataPathOrg = ".";
    
    if (dataPathApp.empty())
        dataPathApp = game.title;
    
#ifndef MKXPZ_RETRO
    customDataPath = mkxp_fs::normalizePath(prefPath(dataPathOrg.c_str(), dataPathApp.c_str()).c_str(), 0, 1);
#endif // MKXPZ_RETRO
    
    if (rgssVersion == 0) {
        /* Try to guess RGSS version based on Data/Scripts extension */
        rgssVersion = 1;
        
        if (!game.scripts.empty()) {
            const char *p = &game.scripts[game.scripts.size()];
            const char *head = &game.scripts[0];
            
            while (--p != head)
                if (*p == '.')
                    break;
            
            if (!strcmp(p, ".rvdata"))
                rgssVersion = 2;
            else if (!strcmp(p, ".rvdata2"))
                rgssVersion = 3;
        }
    }
    
    setupScreenSize(*this);
}
