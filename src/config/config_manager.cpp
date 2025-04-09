#include "config_manager.h"
#include <fstream>
#include <sstream>
#include <random>
#include <iomanip>
#include <filesystem>
#include <algorithm>

namespace tdoa {
namespace config {

namespace {
// Helper function to split path into components
std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> components;
    std::istringstream ss(path);
    std::string component;
    
    while (std::getline(ss, component, '.')) {
        if (!component.empty()) {
            components.push_back(component);
        }
    }
    
    return components;
}

// Helper function to generate a random string
std::string generateRandomString(size_t length) {
    static const std::string chars =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, chars.size() - 1);
    
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += chars[dis(gen)];
    }
    return result;
}

// Helper function to get current timestamp
std::chrono::system_clock::time_point getCurrentTimestamp() {
    return std::chrono::system_clock::now();
}

} // anonymous namespace

ConfigManager::ConfigManager(const std::string& configPath)
    : configPath_(configPath) {
}

bool ConfigManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Create config directory if it doesn't exist
        std::filesystem::path configDir = std::filesystem::path(configPath_).parent_path();
        std::filesystem::create_directories(configDir);
        
        // Load existing configuration or create default
        if (!loadConfig()) {
            // Initialize with empty configuration
            config_ = nlohmann::json::object();
            
            // Create initial version
            createVersion("Initial configuration", "system");
            
            // Save configuration
            return saveConfig();
        }
        
        return true;
    }
    catch (const std::exception& e) {
        // Log error
        return false;
    }
}

std::optional<ConfigValue> ConfigManager::getValue(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto components = splitPath(path);
        nlohmann::json current = config_;
        
        // Navigate to the specified path
        for (const auto& component : components) {
            if (!current.contains(component)) {
                return std::nullopt;
            }
            current = current[component];
        }
        
        return getValueFromJson(current);
    }
    catch (const std::exception& e) {
        return std::nullopt;
    }
}

bool ConfigManager::setValue(const std::string& path, const ConfigValue& value, const std::string& userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Validate the new value
        if (!validateValue(path, value)) {
            return false;
        }
        
        auto components = splitPath(path);
        nlohmann::json* current = &config_;
        
        // Get the old value if it exists
        auto oldValue = getValue(path);
        
        // Navigate to the parent of the target path
        for (size_t i = 0; i < components.size() - 1; ++i) {
            const auto& component = components[i];
            if (!current->contains(component)) {
                (*current)[component] = nlohmann::json::object();
            }
            current = &(*current)[component];
        }
        
        // Set the new value
        (*current)[components.back()] = convertValueToJson(value);
        
        // Save configuration
        if (!saveConfig()) {
            return false;
        }
        
        // Notify change listeners
        ConfigChangeEvent event;
        event.path = path;
        event.newValue = value;
        if (oldValue) {
            event.oldValue = *oldValue;
        }
        event.userId = userId;
        event.timestamp = getCurrentTimestamp();
        notifyChange(event);
        
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

bool ConfigManager::addValidationRule(const ValidationRule& rule) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Check if rule already exists for this path
        auto it = std::find_if(validationRules_.begin(), validationRules_.end(),
            [&rule](const auto& existing) {
                return existing.path == rule.path;
            });
        
        if (it != validationRules_.end()) {
            return false;
        }
        
        validationRules_.push_back(rule);
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

bool ConfigManager::removeValidationRule(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto it = std::find_if(validationRules_.begin(), validationRules_.end(),
            [&path](const auto& rule) {
                return rule.path == path;
            });
        
        if (it == validationRules_.end()) {
            return false;
        }
        
        validationRules_.erase(it);
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

std::string ConfigManager::createVersion(const std::string& description, const std::string& userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        ConfigVersion version;
        version.id = generateId();
        version.description = description;
        version.userId = userId;
        version.timestamp = getCurrentTimestamp();
        version.config = config_;
        
        versions_.push_back(version);
        return version.id;
    }
    catch (const std::exception& e) {
        return "";
    }
}

std::optional<ConfigVersion> ConfigManager::getVersion(const std::string& versionId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto it = std::find_if(versions_.begin(), versions_.end(),
            [&versionId](const auto& version) {
                return version.id == versionId;
            });
        
        if (it == versions_.end()) {
            return std::nullopt;
        }
        
        return *it;
    }
    catch (const std::exception& e) {
        return std::nullopt;
    }
}

std::vector<ConfigVersion> ConfigManager::listVersions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return versions_;
}

bool ConfigManager::restoreVersion(const std::string& versionId, const std::string& userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto version = getVersion(versionId);
        if (!version) {
            return false;
        }
        
        // Create backup before restoring
        createBackup("Automatic backup before version restore", userId);
        
        // Restore configuration
        config_ = version->config;
        
        // Save configuration
        if (!saveConfig()) {
            return false;
        }
        
        // Create new version to mark the restore
        createVersion("Restored from version " + versionId, userId);
        
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

std::string ConfigManager::createBackup(const std::string& description, const std::string& userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        ConfigBackup backup;
        backup.id = generateId();
        backup.description = description;
        backup.userId = userId;
        backup.timestamp = getCurrentTimestamp();
        backup.config = config_;
        
        backups_.push_back(backup);
        return backup.id;
    }
    catch (const std::exception& e) {
        return "";
    }
}

std::optional<ConfigBackup> ConfigManager::getBackup(const std::string& backupId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto it = std::find_if(backups_.begin(), backups_.end(),
            [&backupId](const auto& backup) {
                return backup.id == backupId;
            });
        
        if (it == backups_.end()) {
            return std::nullopt;
        }
        
        return *it;
    }
    catch (const std::exception& e) {
        return std::nullopt;
    }
}

std::vector<ConfigBackup> ConfigManager::listBackups() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return backups_;
}

bool ConfigManager::restoreBackup(const std::string& backupId, const std::string& userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto backup = getBackup(backupId);
        if (!backup) {
            return false;
        }
        
        // Create version before restoring
        createVersion("Automatic version before backup restore", userId);
        
        // Restore configuration
        config_ = backup->config;
        
        // Save configuration
        if (!saveConfig()) {
            return false;
        }
        
        // Create new version to mark the restore
        createVersion("Restored from backup " + backupId, userId);
        
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

std::string ConfigManager::exportConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.dump(4);
}

bool ConfigManager::importConfig(const std::string& json, const std::string& userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Parse and validate JSON
        auto newConfig = nlohmann::json::parse(json);
        
        // Create backup before import
        createBackup("Automatic backup before config import", userId);
        
        // Update configuration
        config_ = newConfig;
        
        // Save configuration
        if (!saveConfig()) {
            return false;
        }
        
        // Create new version to mark the import
        createVersion("Imported configuration", userId);
        
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

bool ConfigManager::registerChangeCallback(ConfigChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        changeCallbacks_.push_back(callback);
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

std::vector<ValidationResult> ConfigManager::validateConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<ValidationResult> results;
    
    try {
        // Validate each path that has a validation rule
        for (const auto& rule : validationRules_) {
            auto value = getValue(rule.path);
            if (value) {
                auto result = rule.validator(*value);
                if (!result.valid) {
                    results.push_back(result);
                }
            }
        }
    }
    catch (const std::exception& e) {
        ValidationResult result;
        result.valid = false;
        result.message = "Error during validation: " + std::string(e.what());
        results.push_back(result);
    }
    
    return results;
}

bool ConfigManager::loadConfig() {
    try {
        std::ifstream file(configPath_);
        if (!file.is_open()) {
            return false;
        }
        
        config_ = nlohmann::json::parse(file);
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

bool ConfigManager::saveConfig() const {
    try {
        std::ofstream file(configPath_);
        if (!file.is_open()) {
            return false;
        }
        
        file << config_.dump(4);
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

bool ConfigManager::validateValue(const std::string& path, const ConfigValue& value) const {
    try {
        // Find validation rules for this path
        for (const auto& rule : validationRules_) {
            if (rule.path == path) {
                auto result = rule.validator(value);
                if (!result.valid) {
                    return false;
                }
            }
        }
        
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

void ConfigManager::notifyChange(const ConfigChangeEvent& event) {
    for (const auto& callback : changeCallbacks_) {
        try {
            callback(event);
        }
        catch (...) {
            // Ignore callback errors
        }
    }
}

std::string ConfigManager::generateId() const {
    return generateRandomString(16);
}

ConfigValue ConfigManager::getValueFromJson(const nlohmann::json& json) const {
    if (json.is_boolean()) {
        return json.get<bool>();
    }
    else if (json.is_number_integer()) {
        if (json.get<int64_t>() <= std::numeric_limits<int32_t>::max()) {
            return json.get<int32_t>();
        }
        return json.get<int64_t>();
    }
    else if (json.is_number_float()) {
        if (json.get<double>() <= std::numeric_limits<float>::max()) {
            return json.get<float>();
        }
        return json.get<double>();
    }
    else if (json.is_string()) {
        return json.get<std::string>();
    }
    else if (json.is_array()) {
        std::vector<std::string> result;
        for (const auto& item : json) {
            if (item.is_string()) {
                result.push_back(item.get<std::string>());
            }
        }
        return result;
    }
    else if (json.is_object()) {
        std::map<std::string, std::string> result;
        for (auto it = json.begin(); it != json.end(); ++it) {
            if (it.value().is_string()) {
                result[it.key()] = it.value().get<std::string>();
            }
        }
        return result;
    }
    
    throw std::runtime_error("Unsupported JSON type");
}

nlohmann::json ConfigManager::convertValueToJson(const ConfigValue& value) const {
    return std::visit([](const auto& v) -> nlohmann::json {
        return v;
    }, value);
}

} // namespace config
} // namespace tdoa 