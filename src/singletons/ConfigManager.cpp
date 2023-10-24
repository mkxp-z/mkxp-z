//
// Created by fcors on 10/20/2023.
//

#include "ConfigManager.h"

#include "config.h"
#include "filesystem.h"

ConfigManager::ConfigManager() = default;

ConfigManager::~ConfigManager() = default;

std::unique_ptr<ConfigManager> ConfigManager::s_instance(new ConfigManager());

ConfigManager &ConfigManager::getInstance() {
    return *s_instance;
}

void ConfigManager::killInstance() {
    s_instance = nullptr;
}

void ConfigManager::initConfig(const std::string &appName, const std::vector<std::string> &args) {
    m_config = std::make_shared<Config>();
    m_filesystem = std::make_shared<FileSystem>(appName.data(), m_config->allowSymlinks);
    m_config->read(args);
}

std::shared_ptr<Config> ConfigManager::getConfig() {
    return m_config;
}

std::shared_ptr<FileSystem> ConfigManager::getfilesystem() {
    return m_filesystem;
}
