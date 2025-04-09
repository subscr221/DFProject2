#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <functional>
#include <chrono>
#include <variant>
#include <optional>
#include <nlohmann/json.hpp>

namespace tdoa {
namespace config {

// Forward declarations
class ConfigManager;

/**
 * @brief Supported configuration value types
 */
using ConfigValue = std::variant<
    bool,
    int32_t,
    int64_t,
    float,
    double,
    std::string,
    std::vector<std::string>,
    std::map<std::string, std::string>
>;

/**
 * @brief Configuration change event data
 */
struct ConfigChangeEvent {
    std::string path;           ///< Path to the changed configuration
    ConfigValue oldValue;       ///< Previous value
    ConfigValue newValue;       ///< New value
    std::string userId;         ///< ID of the user who made the change
    std::chrono::system_clock::time_point timestamp;  ///< When the change occurred
};

/**
 * @brief Configuration validation result
 */
struct ValidationResult {
    bool valid;                 ///< Whether the configuration is valid
    std::string message;        ///< Error message if invalid
};

/**
 * @brief Configuration version information
 */
struct ConfigVersion {
    std::string id;            ///< Version identifier
    std::string description;   ///< Version description
    std::string userId;        ///< User who created this version
    std::chrono::system_clock::time_point timestamp;  ///< When version was created
    nlohmann::json config;     ///< Complete configuration snapshot
};

/**
 * @brief Configuration validation rule
 */
struct ValidationRule {
    std::string path;          ///< Configuration path this rule applies to
    std::function<ValidationResult(const ConfigValue&)> validator;  ///< Validation function
    std::string description;   ///< Description of what this rule validates
};

/**
 * @brief Configuration backup
 */
struct ConfigBackup {
    std::string id;            ///< Backup identifier
    std::string description;   ///< Backup description
    std::string userId;        ///< User who created the backup
    std::chrono::system_clock::time_point timestamp;  ///< When backup was created
    nlohmann::json config;     ///< Complete configuration snapshot
};

/**
 * @brief Callback type for configuration change notifications
 */
using ConfigChangeCallback = std::function<void(const ConfigChangeEvent&)>;

/**
 * @brief Configuration manager class
 * 
 * Provides centralized management of system configuration with validation,
 * versioning, and backup/restore capabilities.
 */
class ConfigManager {
public:
    /**
     * @brief Constructor
     * @param configPath Path to the configuration file
     */
    explicit ConfigManager(const std::string& configPath);
    
    /**
     * @brief Initialize the configuration manager
     * @return true if initialization successful, false otherwise
     */
    bool initialize();
    
    /**
     * @brief Get a configuration value
     * @param path Path to the configuration value
     * @return Optional containing the value if found
     */
    std::optional<ConfigValue> getValue(const std::string& path) const;
    
    /**
     * @brief Set a configuration value
     * @param path Path to the configuration value
     * @param value New value
     * @param userId ID of the user making the change
     * @return true if successful, false otherwise
     */
    bool setValue(const std::string& path, const ConfigValue& value, const std::string& userId);
    
    /**
     * @brief Add a validation rule
     * @param rule Validation rule to add
     * @return true if successful, false otherwise
     */
    bool addValidationRule(const ValidationRule& rule);
    
    /**
     * @brief Remove a validation rule
     * @param path Path the rule applies to
     * @return true if successful, false otherwise
     */
    bool removeValidationRule(const std::string& path);
    
    /**
     * @brief Create a new configuration version
     * @param description Version description
     * @param userId ID of the user creating the version
     * @return Version ID if successful, empty string otherwise
     */
    std::string createVersion(const std::string& description, const std::string& userId);
    
    /**
     * @brief Get configuration version information
     * @param versionId Version identifier
     * @return Optional containing version info if found
     */
    std::optional<ConfigVersion> getVersion(const std::string& versionId) const;
    
    /**
     * @brief List all configuration versions
     * @return Vector of version information
     */
    std::vector<ConfigVersion> listVersions() const;
    
    /**
     * @brief Restore configuration to a specific version
     * @param versionId Version to restore to
     * @param userId ID of the user performing the restore
     * @return true if successful, false otherwise
     */
    bool restoreVersion(const std::string& versionId, const std::string& userId);
    
    /**
     * @brief Create a configuration backup
     * @param description Backup description
     * @param userId ID of the user creating the backup
     * @return Backup ID if successful, empty string otherwise
     */
    std::string createBackup(const std::string& description, const std::string& userId);
    
    /**
     * @brief Get configuration backup information
     * @param backupId Backup identifier
     * @return Optional containing backup info if found
     */
    std::optional<ConfigBackup> getBackup(const std::string& backupId) const;
    
    /**
     * @brief List all configuration backups
     * @return Vector of backup information
     */
    std::vector<ConfigBackup> listBackups() const;
    
    /**
     * @brief Restore configuration from a backup
     * @param backupId Backup to restore from
     * @param userId ID of the user performing the restore
     * @return true if successful, false otherwise
     */
    bool restoreBackup(const std::string& backupId, const std::string& userId);
    
    /**
     * @brief Export configuration to JSON
     * @return JSON string containing current configuration
     */
    std::string exportConfig() const;
    
    /**
     * @brief Import configuration from JSON
     * @param json JSON string containing configuration
     * @param userId ID of the user performing the import
     * @return true if successful, false otherwise
     */
    bool importConfig(const std::string& json, const std::string& userId);
    
    /**
     * @brief Register a callback for configuration changes
     * @param callback Function to call when configuration changes
     * @return true if successful, false otherwise
     */
    bool registerChangeCallback(ConfigChangeCallback callback);
    
    /**
     * @brief Validate current configuration
     * @return Vector of validation results
     */
    std::vector<ValidationResult> validateConfig() const;
    
private:
    std::string configPath_;
    nlohmann::json config_;
    std::vector<ValidationRule> validationRules_;
    std::vector<ConfigVersion> versions_;
    std::vector<ConfigBackup> backups_;
    std::vector<ConfigChangeCallback> changeCallbacks_;
    mutable std::mutex mutex_;
    
    bool loadConfig();
    bool saveConfig() const;
    bool validateValue(const std::string& path, const ConfigValue& value) const;
    void notifyChange(const ConfigChangeEvent& event);
    std::string generateId() const;
    ConfigValue getValueFromJson(const nlohmann::json& json) const;
    nlohmann::json convertValueToJson(const ConfigValue& value) const;
};

} // namespace config
} // namespace tdoa 