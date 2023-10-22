//
// Created by fcors on 10/20/2023.
//

#pragma once

#include <memory>
#include <vector>
#include <string>

#define CONFIG_MANAGER ConfigManager::getInstance()
#define CONFIG ConfigManager::getInstance().getConfig()
#define FONT_STATE ConfigManager::getInstance().getFontState()
#define FILESYSTEM ConfigManager::getInstance().getFileSystem()
#define MIDI_STATE ConfigManager::getInstance().getMidiState()

class Config;

class SharedFontState;

class Font;

class FileSystem;

class SharedMidiState;

class ConfigManager {
    ConfigManager();
    ~ConfigManager();

public:
    static ConfigManager &getInstance();

    void initConfig(const std::string &appName, const std::vector<std::string> &args);

    Config & getConfig();

    SharedFontState &getFontState();

    Font &defaultFont() const;

    FileSystem &getFileSystem();

    SharedMidiState &getMidiState();

private:
    std::unique_ptr<Config> m_config;
    std::unique_ptr<SharedFontState> m_fontState;
    std::unique_ptr<Font> m_defaultFont;
    std::unique_ptr<FileSystem> m_fileSystem;
    std::unique_ptr<SharedMidiState> m_midiState;
};
