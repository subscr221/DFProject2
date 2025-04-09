#include "../config_manager.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace tdoa::config;
using ::testing::_;
using ::testing::Return;

class ConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test files
        testDir_ = std::filesystem::temp_directory_path() / "config_test";
        std::filesystem::create_directories(testDir_);
        
        configPath_ = testDir_ / "config.json";
        configManager_ = std::make_unique<ConfigManager>(configPath_.string());
        
        // Initialize with test configuration
        ASSERT_TRUE(configManager_->initialize());
        
        // Set up test validation rules
        ValidationRule intRule;
        intRule.path = "test.intValue";
        intRule.description = "Integer between 0 and 100";
        intRule.validator = [](const ConfigValue& value) -> ValidationResult {
            ValidationResult result;
            if (auto intVal = std::get_if<int32_t>(&value)) {
                result.valid = *intVal >= 0 && *intVal <= 100;
                if (!result.valid) {
                    result.message = "Value must be between 0 and 100";
                }
            } else {
                result.valid = false;
                result.message = "Value must be an integer";
            }
            return result;
        };
        
        ValidationRule stringRule;
        stringRule.path = "test.stringValue";
        stringRule.description = "Non-empty string";
        stringRule.validator = [](const ConfigValue& value) -> ValidationResult {
            ValidationResult result;
            if (auto strVal = std::get_if<std::string>(&value)) {
                result.valid = !strVal->empty();
                if (!result.valid) {
                    result.message = "String cannot be empty";
                }
            } else {
                result.valid = false;
                result.message = "Value must be a string";
            }
            return result;
        };
        
        configManager_->addValidationRule(intRule);
        configManager_->addValidationRule(stringRule);
        
        // Register change callback
        configManager_->registerChangeCallback([this](const ConfigChangeEvent& event) {
            lastChangeEvent_ = event;
        });
    }
    
    void TearDown() override {
        configManager_.reset();
        std::filesystem::remove_all(testDir_);
    }
    
    std::filesystem::path testDir_;
    std::filesystem::path configPath_;
    std::unique_ptr<ConfigManager> configManager_;
    std::optional<ConfigChangeEvent> lastChangeEvent_;
};

TEST_F(ConfigManagerTest, BasicOperations) {
    // Set values
    EXPECT_TRUE(configManager_->setValue("test.intValue", 50, "test_user"));
    EXPECT_TRUE(configManager_->setValue("test.stringValue", std::string("test"), "test_user"));
    
    // Get values
    auto intValue = configManager_->getValue("test.intValue");
    ASSERT_TRUE(intValue);
    EXPECT_EQ(std::get<int32_t>(*intValue), 50);
    
    auto stringValue = configManager_->getValue("test.stringValue");
    ASSERT_TRUE(stringValue);
    EXPECT_EQ(std::get<std::string>(*stringValue), "test");
    
    // Check change event
    ASSERT_TRUE(lastChangeEvent_);
    EXPECT_EQ(lastChangeEvent_->path, "test.stringValue");
    EXPECT_EQ(std::get<std::string>(lastChangeEvent_->newValue), "test");
    EXPECT_EQ(lastChangeEvent_->userId, "test_user");
}

TEST_F(ConfigManagerTest, Validation) {
    // Test valid values
    EXPECT_TRUE(configManager_->setValue("test.intValue", 50, "test_user"));
    EXPECT_TRUE(configManager_->setValue("test.stringValue", std::string("test"), "test_user"));
    
    // Test invalid values
    EXPECT_FALSE(configManager_->setValue("test.intValue", 150, "test_user"));
    EXPECT_FALSE(configManager_->setValue("test.stringValue", std::string(""), "test_user"));
    
    // Test validation results
    auto results = configManager_->validateConfig();
    EXPECT_TRUE(results.empty());
    
    // Set invalid value directly in JSON and validate
    configManager_->setValue("test.intValue", 150, "test_user");
    results = configManager_->validateConfig();
    EXPECT_FALSE(results.empty());
}

TEST_F(ConfigManagerTest, Versioning) {
    // Set initial values
    configManager_->setValue("test.intValue", 50, "test_user");
    configManager_->setValue("test.stringValue", std::string("test"), "test_user");
    
    // Create version
    std::string versionId = configManager_->createVersion("Test version", "test_user");
    EXPECT_FALSE(versionId.empty());
    
    // Modify values
    configManager_->setValue("test.intValue", 75, "test_user");
    configManager_->setValue("test.stringValue", std::string("modified"), "test_user");
    
    // Get version
    auto version = configManager_->getVersion(versionId);
    ASSERT_TRUE(version);
    EXPECT_EQ(version->description, "Test version");
    EXPECT_EQ(version->userId, "test_user");
    
    // List versions
    auto versions = configManager_->listVersions();
    EXPECT_FALSE(versions.empty());
    
    // Restore version
    EXPECT_TRUE(configManager_->restoreVersion(versionId, "test_user"));
    
    // Verify restored values
    auto intValue = configManager_->getValue("test.intValue");
    ASSERT_TRUE(intValue);
    EXPECT_EQ(std::get<int32_t>(*intValue), 50);
    
    auto stringValue = configManager_->getValue("test.stringValue");
    ASSERT_TRUE(stringValue);
    EXPECT_EQ(std::get<std::string>(*stringValue), "test");
}

TEST_F(ConfigManagerTest, BackupRestore) {
    // Set initial values
    configManager_->setValue("test.intValue", 50, "test_user");
    configManager_->setValue("test.stringValue", std::string("test"), "test_user");
    
    // Create backup
    std::string backupId = configManager_->createBackup("Test backup", "test_user");
    EXPECT_FALSE(backupId.empty());
    
    // Modify values
    configManager_->setValue("test.intValue", 75, "test_user");
    configManager_->setValue("test.stringValue", std::string("modified"), "test_user");
    
    // Get backup
    auto backup = configManager_->getBackup(backupId);
    ASSERT_TRUE(backup);
    EXPECT_EQ(backup->description, "Test backup");
    EXPECT_EQ(backup->userId, "test_user");
    
    // List backups
    auto backups = configManager_->listBackups();
    EXPECT_FALSE(backups.empty());
    
    // Restore backup
    EXPECT_TRUE(configManager_->restoreBackup(backupId, "test_user"));
    
    // Verify restored values
    auto intValue = configManager_->getValue("test.intValue");
    ASSERT_TRUE(intValue);
    EXPECT_EQ(std::get<int32_t>(*intValue), 50);
    
    auto stringValue = configManager_->getValue("test.stringValue");
    ASSERT_TRUE(stringValue);
    EXPECT_EQ(std::get<std::string>(*stringValue), "test");
}

TEST_F(ConfigManagerTest, ImportExport) {
    // Set initial values
    configManager_->setValue("test.intValue", 50, "test_user");
    configManager_->setValue("test.stringValue", std::string("test"), "test_user");
    
    // Export configuration
    std::string exported = configManager_->exportConfig();
    EXPECT_FALSE(exported.empty());
    
    // Modify values
    configManager_->setValue("test.intValue", 75, "test_user");
    configManager_->setValue("test.stringValue", std::string("modified"), "test_user");
    
    // Import configuration
    EXPECT_TRUE(configManager_->importConfig(exported, "test_user"));
    
    // Verify imported values
    auto intValue = configManager_->getValue("test.intValue");
    ASSERT_TRUE(intValue);
    EXPECT_EQ(std::get<int32_t>(*intValue), 50);
    
    auto stringValue = configManager_->getValue("test.stringValue");
    ASSERT_TRUE(stringValue);
    EXPECT_EQ(std::get<std::string>(*stringValue), "test");
}

TEST_F(ConfigManagerTest, Persistence) {
    // Set values
    configManager_->setValue("test.intValue", 50, "test_user");
    configManager_->setValue("test.stringValue", std::string("test"), "test_user");
    
    // Create new instance with same file
    configManager_.reset();
    configManager_ = std::make_unique<ConfigManager>(configPath_.string());
    ASSERT_TRUE(configManager_->initialize());
    
    // Verify values persisted
    auto intValue = configManager_->getValue("test.intValue");
    ASSERT_TRUE(intValue);
    EXPECT_EQ(std::get<int32_t>(*intValue), 50);
    
    auto stringValue = configManager_->getValue("test.stringValue");
    ASSERT_TRUE(stringValue);
    EXPECT_EQ(std::get<std::string>(*stringValue), "test");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 