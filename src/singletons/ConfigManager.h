//
// Created by fcors on 10/20/2023.
//

#pragma once

#include <memory>
#include <vector>
#include <string>

/*
#define CONFIG ConfigManager::getInstance().getConfig()
#define filesystem ConfigManager::getInstance().getfilesystem()
 */

class Config;
class FileSystem;


class ConfigManager {
    ConfigManager();

    ~ConfigManager();

public:
    static ConfigManager &getInstance();

    void initConfig(const std::string &appName, const std::vector<std::string> &args);

    std::shared_ptr<Config> getConfig();

    std::shared_ptr<FileSystem> getfilesystem();

private:
    std::shared_ptr<Config> m_config;
    std::shared_ptr<FileSystem> m_filesystem;
};
