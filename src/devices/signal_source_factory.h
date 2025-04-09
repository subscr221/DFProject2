/**
 * @file signal_source_factory.h
 * @brief Factory for creating signal source device instances
 */

#pragma once

#include "signal_source_device.h"
#include <memory>
#include <string>
#include <map>
#include <functional>

namespace tdoa {
namespace devices {

/**
 * @class SignalSourceFactory
 * @brief Factory for creating signal source devices
 * 
 * Provides a unified interface for creating different types of signal source
 * devices, allowing the application to use hardware abstractions without
 * directly depending on specific implementations.
 */
class SignalSourceFactory {
public:
    /**
     * @brief Device type enumeration
     */
    enum class DeviceType {
        BB60C,       ///< SignalHound BB60C
        Unknown      ///< Unknown device type
    };
    
    /**
     * @brief Get the singleton instance
     * @return Reference to the factory singleton
     */
    static SignalSourceFactory& getInstance() {
        static SignalSourceFactory instance;
        return instance;
    }
    
    /**
     * @brief Create a signal source device
     * @param type Type of device to create
     * @return Unique pointer to the created device, or nullptr if type is unknown
     */
    std::unique_ptr<SignalSourceDevice> createDevice(DeviceType type) {
        auto it = creators_.find(type);
        if (it != creators_.end()) {
            return it->second();
        }
        return nullptr;
    }
    
    /**
     * @brief Create a signal source device from a string type name
     * @param typeName Name of the device type (case-insensitive)
     * @return Unique pointer to the created device, or nullptr if type is unknown
     */
    std::unique_ptr<SignalSourceDevice> createDevice(const std::string& typeName) {
        // Convert type name to device type
        DeviceType type = deviceTypeFromString(typeName);
        return createDevice(type);
    }
    
    /**
     * @brief Get a list of supported device types
     * @return Vector of supported device type names
     */
    std::vector<std::string> getSupportedDeviceTypes() const {
        std::vector<std::string> types;
        for (const auto& pair : typeNames_) {
            types.push_back(pair.second);
        }
        return types;
    }
    
    /**
     * @brief Check if a device type is supported
     * @param type Device type to check
     * @return True if the device type is supported
     */
    bool isDeviceTypeSupported(DeviceType type) const {
        return creators_.find(type) != creators_.end();
    }
    
    /**
     * @brief Check if a device type is supported
     * @param typeName Name of the device type (case-insensitive)
     * @return True if the device type is supported
     */
    bool isDeviceTypeSupported(const std::string& typeName) const {
        DeviceType type = deviceTypeFromString(typeName);
        return isDeviceTypeSupported(type);
    }
    
    /**
     * @brief Get device type from string name
     * @param typeName Name of the device type (case-insensitive)
     * @return Device type enum value
     */
    DeviceType deviceTypeFromString(const std::string& typeName) const {
        // Convert to lowercase for case-insensitive comparison
        std::string lowerName = typeName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        
        // Find matching type
        for (const auto& pair : typeNames_) {
            std::string lowerTypeName = pair.second;
            std::transform(lowerTypeName.begin(), lowerTypeName.end(), lowerTypeName.begin(),
                          [](unsigned char c) { return std::tolower(c); });
            
            if (lowerName == lowerTypeName) {
                return pair.first;
            }
        }
        
        return DeviceType::Unknown;
    }
    
    /**
     * @brief Get string name for device type
     * @param type Device type enum value
     * @return String name for the device type, or "Unknown" if not found
     */
    std::string deviceTypeToString(DeviceType type) const {
        auto it = typeNames_.find(type);
        if (it != typeNames_.end()) {
            return it->second;
        }
        return "Unknown";
    }
    
private:
    // Private constructor for singleton
    SignalSourceFactory() {
        registerDeviceTypes();
    }
    
    // No copying allowed
    SignalSourceFactory(const SignalSourceFactory&) = delete;
    SignalSourceFactory& operator=(const SignalSourceFactory&) = delete;
    
    /**
     * @brief Register known device types and their creators
     */
    void registerDeviceTypes();
    
    // Type aliases
    using DeviceCreator = std::function<std::unique_ptr<SignalSourceDevice>()>;
    using CreatorMap = std::map<DeviceType, DeviceCreator>;
    using TypeNameMap = std::map<DeviceType, std::string>;
    
    CreatorMap creators_;      ///< Map of device types to creator functions
    TypeNameMap typeNames_;    ///< Map of device types to type names
};

} // namespace devices
} // namespace tdoa 