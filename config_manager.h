#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <map>

class ConfigManager {
public:
    ConfigManager(const std::string& filename = "amalyzer.conf");
    
    void load();
    void save();
    void print();
    
    int getInt(const std::string& key, int defaultValue);
    void setInt(const std::string& key, int value);
    void setValue(const std::string& key, const std::string& value);

private:
    std::string filename;
    std::map<std::string, std::string> configData;
    
    void createDefaultConfig();
    std::string trim(const std::string& str);
};

#endif // CONFIG_MANAGER_H
