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

    static void killInstance();

    void initConfig(const std::string &appName, int argc, char *argv[]);

    std::shared_ptr<Config> getConfig();

    std::shared_ptr<FileSystem> getFilesystem();

private:
    friend std::unique_ptr<ConfigManager>::deleter_type;

    static std::unique_ptr<ConfigManager> s_instance;

    std::shared_ptr<Config> m_config;
    std::shared_ptr<FileSystem> m_filesystem;
};
