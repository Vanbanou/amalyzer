#include "config_manager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

ConfigManager::ConfigManager(const std::string& filename) : filename(filename) {
    load();
}

void ConfigManager::load() {
    if (!fs::exists(filename)) {
        createDefaultConfig();
        return;
    }

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Erro ao abrir arquivo de configuração: " << filename << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        size_t delimiterPos = line.find('=');
        if (delimiterPos != std::string::npos) {
            std::string key = trim(line.substr(0, delimiterPos));
            std::string value = trim(line.substr(delimiterPos + 1));
            configData[key] = value;
        }
    }
}

void ConfigManager::save() {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Erro ao salvar arquivo de configuração: " << filename << std::endl;
        return;
    }

    for (const auto& pair : configData) {
        file << pair.first << "=" << pair.second << "\n";
    }
}

void ConfigManager::print() {
    std::cout << "Configurações atuais:\n";
    for (const auto& pair : configData) {
        std::cout << "  " << pair.first << " = " << pair.second << "\n";
    }
}

void ConfigManager::createDefaultConfig() {
    // Default values based on hardcoded values in main.cpp
    configData["name_w"] = "25";
    configData["artist_w"] = "15";
    configData["album_w"] = "20";
    configData["ana_name_w"] = "20";
    
    save();
}

int ConfigManager::getInt(const std::string& key, int defaultValue) {
    if (configData.find(key) != configData.end()) {
        try {
            return std::stoi(configData[key]);
        } catch (...) {
            return defaultValue;
        }
    }
    return defaultValue;
}

void ConfigManager::setInt(const std::string& key, int value) {
    configData[key] = std::to_string(value);
}

void ConfigManager::setValue(const std::string& key, const std::string& value) {
    configData[key] = value;
}

std::string ConfigManager::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}
