//
// Created by fcors on 10/20/2023.
//

#include "ConfigManager.h"

#include "config.h"
#include "font.h"
#include "filesystem.h"

ConfigManager::ConfigManager() = default;
ConfigManager::~ConfigManager() = default;

ConfigManager &ConfigManager::getInstance() {
    static ConfigManager configManager;
    return configManager;
}

void ConfigManager::initConfig(const std::string &appName, const std::vector<std::string> &args) {
    m_config = std::make_unique<Config>();
    m_fontState = std::make_unique<SharedFontState>(*m_config);
    m_fileSystem = std::make_unique<FileSystem>(appName.data(), m_config->allowSymlinks);
    m_config->read(args);
}

Config & ConfigManager::getConfig() {
    return *m_config;
}

SharedFontState & ConfigManager::getFontState() {
    return *m_fontState;
}

FileSystem &ConfigManager::getFileSystem() {
    return *m_fileSystem;
}