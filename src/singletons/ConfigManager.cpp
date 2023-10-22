//
// Created by fcors on 10/20/2023.
//

#include "ConfigManager.h"

#include "config.h"
#include "font.h"
#include "filesystem.h"
#include "sharedmidistate.h"

ConfigManager::ConfigManager() = default;
ConfigManager::~ConfigManager() = default;

ConfigManager &ConfigManager::getInstance() {
    static ConfigManager configManager;
    return configManager;
}

void ConfigManager::initConfig(const std::string &appName, const std::vector<std::string> &args) {
    m_config = std::make_unique<Config>();
    m_fontState = std::make_unique<SharedFontState>(*m_config);
    m_defaultFont = std::make_unique<Font>();
    m_fileSystem = std::make_unique<FileSystem>(appName.data(), m_config->allowSymlinks);
    m_midiState = std::make_unique<SharedMidiState>(*m_config);
    m_config->read(args);
}

Config &ConfigManager::getConfig() {
    return *m_config;
}

Font &ConfigManager::defaultFont() const {
    return *m_defaultFont;
}

SharedFontState &ConfigManager::getFontState() {
    return *m_fontState;
}

FileSystem &ConfigManager::getFileSystem() {
    return *m_fileSystem;
}

SharedMidiState &ConfigManager::getMidiState() {
    return *m_midiState;
}
