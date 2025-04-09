/**
 * @file bb60c_abstract_device.cpp
 * @brief Implementation of the BB60C device abstraction layer
 */

#include "bb60c_abstract_device.h"
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <cmath>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace tdoa {
namespace devices {

// Maximum raw sample rate for BB60C
constexpr double BB60C_MAX_SAMPLE_RATE = 40.0e6;

// Valid decimation values for BB60C
const std::vector<int> VALID_DECIMATION_VALUES = {
    1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192
};

// Constructor
BB60CAbstractDevice::BB60CAbstractDevice()
    : device_(std::make_unique<BB60CDevice>())
    , userData_(nullptr)
{
    // Set up profile directory
    profileDirectory_ = "config/bb60c_profiles";
    ensureDirectoryExists(profileDirectory_);
}

// Destructor
BB60CAbstractDevice::~BB60CAbstractDevice() {
    // Make sure device is closed
    if (isOpen()) {
        close();
    }
}

// Get available BB60C devices
std::vector<SignalSourceDevice::DeviceInfo> BB60CAbstractDevice::getAvailableDevices() {
    std::vector<DeviceInfo> deviceInfoList;
    
    // Get list of device serial numbers
    auto serialNumbers = BB60CDevice::getDeviceList();
    
    // Create DeviceInfo structures for each device
    for (const auto& serial : serialNumbers) {
        DeviceInfo info;
        info.serialNumber = serial;
        info.modelName = "BB60C";
        
        // Set device capabilities
        info.capabilities.minFrequency = 9.0e3;      // 9 kHz
        info.capabilities.maxFrequency = 6.0e9;      // 6 GHz
        info.capabilities.maxBandwidth = 27.0e6;     // 27 MHz
        info.capabilities.maxSampleRate = 40.0e6;    // 40 MS/s
        info.capabilities.supportedFormats = {
            DataFormat::Float32,
            DataFormat::Int16
        };
        info.capabilities.hasTimeStamping = true;
        info.capabilities.hasTriggerIO = true;
        
        deviceInfoList.push_back(info);
    }
    
    return deviceInfoList;
}

// Open the BB60C device
SignalSourceDevice::OperationResult BB60CAbstractDevice::open(const std::string& serialNumber) {
    try {
        // Try to open the device
        device_->open(serialNumber);
        
        // Get device info
        if (device_->isOpen()) {
            return OperationResult::Success;
        } else {
            return OperationResult::DeviceNotFound;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error opening BB60C device: " << e.what() << std::endl;
        return OperationResult::HardwareError;
    }
}

// Close the BB60C device
SignalSourceDevice::OperationResult BB60CAbstractDevice::close() {
    try {
        if (!device_->isOpen()) {
            return OperationResult::DeviceNotOpen;
        }
        
        // Stop streaming if active
        if (device_) {
            device_->close();
        }
        
        return OperationResult::Success;
    } catch (const std::exception& e) {
        std::cerr << "Error closing BB60C device: " << e.what() << std::endl;
        return OperationResult::HardwareError;
    }
}

// Check if BB60C device is open
bool BB60CAbstractDevice::isOpen() const {
    return device_ && device_->isOpen();
}

// Get BB60C device information
SignalSourceDevice::DeviceInfo BB60CAbstractDevice::getDeviceInfo() const {
    DeviceInfo info;
    
    if (!isOpen()) {
        return info;
    }
    
    try {
        info.serialNumber = device_->getSerialNumber();
        info.modelName = "BB60C";
        info.firmwareVersion = device_->getFirmwareVersion();
        
        // Set device capabilities
        info.capabilities.minFrequency = 9.0e3;      // 9 kHz
        info.capabilities.maxFrequency = 6.0e9;      // 6 GHz
        info.capabilities.maxBandwidth = 27.0e6;     // 27 MHz
        info.capabilities.maxSampleRate = 40.0e6;    // 40 MS/s
        info.capabilities.supportedFormats = {
            DataFormat::Float32,
            DataFormat::Int16
        };
        info.capabilities.hasTimeStamping = true;
        info.capabilities.hasTriggerIO = true;
    } catch (const std::exception& e) {
        std::cerr << "Error getting BB60C device info: " << e.what() << std::endl;
    }
    
    return info;
}

// Set BB60C specific parameters
SignalSourceDevice::OperationResult BB60CAbstractDevice::setParams(const DeviceParams& params) {
    try {
        // Check if device is open
        if (!isOpen()) {
            return OperationResult::DeviceNotOpen;
        }
        
        // Cast to BB60CParams
        const BB60CParams* bb60cParams = dynamic_cast<const BB60CParams*>(&params);
        if (!bb60cParams) {
            return OperationResult::InvalidParameter;
        }
        
        // Validate parameters
        if (!validateParams(*bb60cParams)) {
            return OperationResult::InvalidParameter;
        }
        
        // Store parameters
        currentParams_ = *bb60cParams;
        
        // Apply I/O port configuration
        device_->configureIO(
            static_cast<int>(bb60cParams->port1Mode),
            static_cast<int>(bb60cParams->port2Mode)
        );
        
        // Apply other parameters when configuring for streaming
        
        return OperationResult::Success;
    } catch (const std::exception& e) {
        std::cerr << "Error setting BB60C parameters: " << e.what() << std::endl;
        return OperationResult::HardwareError;
    }
}

// Configure streaming parameters
SignalSourceDevice::OperationResult BB60CAbstractDevice::configureStreaming(const StreamingConfig& config) {
    try {
        // Check if device is open
        if (!isOpen()) {
            return OperationResult::DeviceNotOpen;
        }
        
        // Validate configuration
        if (!validateStreamingConfig(config)) {
            return OperationResult::InvalidParameter;
        }
        
        // Store configuration
        currentConfig_ = config;
        
        // Configure device for I/Q streaming
        BB60CDevice::IQConfig iqConfig;
        iqConfig.centerFreq = config.centerFrequency;
        iqConfig.decimation = currentParams_.decimation;
        iqConfig.bandwidth = config.bandwidth;
        iqConfig.useFloat = (config.format == DataFormat::Float32);
        
        // Apply configuration
        device_->configureIQ(iqConfig);
        
        // Set buffer size
        device_->setBufferSize(config.bufferSize);
        
        return OperationResult::Success;
    } catch (const std::exception& e) {
        std::cerr << "Error configuring BB60C streaming: " << e.what() << std::endl;
        return OperationResult::HardwareError;
    }
}

// Start streaming I/Q data
SignalSourceDevice::OperationResult BB60CAbstractDevice::startStreaming(StreamingCallback callback, void* userData) {
    try {
        // Check if device is open
        if (!isOpen()) {
            return OperationResult::DeviceNotOpen;
        }
        
        // Store callback and user data
        userCallback_ = callback;
        userData_ = userData;
        
        // Start I/Q streaming
        device_->startIQStreaming(
            [this](const void* data, size_t length, double timestamp) {
                deviceCallback(data, length, timestamp);
            },
            currentConfig_.enableTimeStamp
        );
        
        return OperationResult::Success;
    } catch (const std::exception& e) {
        std::cerr << "Error starting BB60C streaming: " << e.what() << std::endl;
        return OperationResult::HardwareError;
    }
}

// Stop streaming I/Q data
SignalSourceDevice::OperationResult BB60CAbstractDevice::stopStreaming() {
    try {
        // Check if device is open
        if (!isOpen()) {
            return OperationResult::DeviceNotOpen;
        }
        
        // Stop streaming
        device_->stopIQStreaming();
        
        return OperationResult::Success;
    } catch (const std::exception& e) {
        std::cerr << "Error stopping BB60C streaming: " << e.what() << std::endl;
        return OperationResult::HardwareError;
    }
}

// Get streaming performance metrics
SignalSourceDevice::StreamingMetrics BB60CAbstractDevice::getStreamingMetrics() const {
    StreamingMetrics metrics;
    
    try {
        if (!isOpen()) {
            return metrics;
        }
        
        // Get metrics from device
        auto deviceMetrics = device_->getStreamingMetrics();
        
        // Convert to abstract metrics
        metrics.sampleRate = deviceMetrics.sampleRate;
        metrics.dataRate = deviceMetrics.dataRate;
        metrics.droppedBuffers = deviceMetrics.droppedBuffers;
        metrics.avgCallbackTime = deviceMetrics.avgCallbackTime;
    } catch (const std::exception& e) {
        std::cerr << "Error getting BB60C streaming metrics: " << e.what() << std::endl;
    }
    
    return metrics;
}

// Reset device to default state
SignalSourceDevice::OperationResult BB60CAbstractDevice::reset() {
    try {
        // Check if device is open
        if (!isOpen()) {
            return OperationResult::DeviceNotOpen;
        }
        
        // Reset device
        device_->reset();
        
        // Reset parameters to defaults
        currentParams_ = BB60CParams();
        currentConfig_ = StreamingConfig();
        
        return OperationResult::Success;
    } catch (const std::exception& e) {
        std::cerr << "Error resetting BB60C device: " << e.what() << std::endl;
        return OperationResult::HardwareError;
    }
}

// Save current configuration to a profile
SignalSourceDevice::OperationResult BB60CAbstractDevice::saveProfile(const std::string& profileName) {
    try {
        // Ensure profile name is valid
        if (profileName.empty() || profileName.find_first_of("/\\:*?\"<>|") != std::string::npos) {
            return OperationResult::InvalidParameter;
        }
        
        // Create JSON object for configuration
        json configJson;
        
        // Add streaming configuration
        configJson["streaming"] = {
            {"centerFrequency", currentConfig_.centerFrequency},
            {"bandwidth", currentConfig_.bandwidth},
            {"sampleRate", currentConfig_.sampleRate},
            {"format", static_cast<int>(currentConfig_.format)},
            {"enableTimeStamp", currentConfig_.enableTimeStamp},
            {"bufferSize", currentConfig_.bufferSize}
        };
        
        // Add device parameters
        configJson["parameters"] = {
            {"decimation", currentParams_.decimation},
            {"port1Mode", static_cast<int>(currentParams_.port1Mode)},
            {"port2Mode", static_cast<int>(currentParams_.port2Mode)},
            {"gainMode", static_cast<int>(currentParams_.gainMode)},
            {"rfGain", currentParams_.rfGain},
            {"attenuationMode", static_cast<int>(currentParams_.attenuationMode)},
            {"rfFilterMode", static_cast<int>(currentParams_.rfFilterMode)},
            {"referenceLevel", currentParams_.referenceLevel}
        };
        
        // Save to file
        std::string filename = profileDirectory_ + "/" + profileName + ".json";
        std::ofstream file(filename);
        if (!file.is_open()) {
            return OperationResult::InternalError;
        }
        
        file << configJson.dump(4); // Pretty-print with 4-space indent
        file.close();
        
        return OperationResult::Success;
    } catch (const std::exception& e) {
        std::cerr << "Error saving BB60C profile: " << e.what() << std::endl;
        return OperationResult::InternalError;
    }
}

// Load configuration from a profile
SignalSourceDevice::OperationResult BB60CAbstractDevice::loadProfile(const std::string& profileName) {
    try {
        // Check if device is open
        if (!isOpen()) {
            return OperationResult::DeviceNotOpen;
        }
        
        // Load from file
        std::string filename = profileDirectory_ + "/" + profileName + ".json";
        std::ifstream file(filename);
        if (!file.is_open()) {
            return OperationResult::InvalidParameter;
        }
        
        // Parse JSON
        json configJson;
        file >> configJson;
        file.close();
        
        // Extract streaming configuration
        StreamingConfig streamConfig;
        streamConfig.centerFrequency = configJson["streaming"]["centerFrequency"];
        streamConfig.bandwidth = configJson["streaming"]["bandwidth"];
        streamConfig.sampleRate = configJson["streaming"]["sampleRate"];
        streamConfig.format = static_cast<DataFormat>(configJson["streaming"]["format"].get<int>());
        streamConfig.enableTimeStamp = configJson["streaming"]["enableTimeStamp"];
        streamConfig.bufferSize = configJson["streaming"]["bufferSize"];
        
        // Extract device parameters
        BB60CParams params;
        params.decimation = configJson["parameters"]["decimation"];
        params.port1Mode = static_cast<BB60CParams::Port1Mode>(configJson["parameters"]["port1Mode"].get<int>());
        params.port2Mode = static_cast<BB60CParams::Port2Mode>(configJson["parameters"]["port2Mode"].get<int>());
        params.gainMode = static_cast<BB60CParams::GainMode>(configJson["parameters"]["gainMode"].get<int>());
        params.rfGain = configJson["parameters"]["rfGain"];
        params.attenuationMode = static_cast<BB60CParams::Attenuation>(configJson["parameters"]["attenuationMode"].get<int>());
        params.rfFilterMode = static_cast<BB60CParams::RFFilterMode>(configJson["parameters"]["rfFilterMode"].get<int>());
        params.referenceLevel = configJson["parameters"]["referenceLevel"];
        
        // Apply configuration
        if (setParams(params) != OperationResult::Success) {
            return OperationResult::InvalidParameter;
        }
        
        if (configureStreaming(streamConfig) != OperationResult::Success) {
            return OperationResult::InvalidParameter;
        }
        
        return OperationResult::Success;
    } catch (const std::exception& e) {
        std::cerr << "Error loading BB60C profile: " << e.what() << std::endl;
        return OperationResult::InternalError;
    }
}

// Delete a configuration profile
SignalSourceDevice::OperationResult BB60CAbstractDevice::deleteProfile(const std::string& profileName) {
    try {
        // Ensure profile name is valid
        if (profileName.empty() || profileName.find_first_of("/\\:*?\"<>|") != std::string::npos) {
            return OperationResult::InvalidParameter;
        }
        
        // Delete file
        std::string filename = profileDirectory_ + "/" + profileName + ".json";
        if (!fs::exists(filename)) {
            return OperationResult::InvalidParameter;
        }
        
        if (!fs::remove(filename)) {
            return OperationResult::InternalError;
        }
        
        return OperationResult::Success;
    } catch (const std::exception& e) {
        std::cerr << "Error deleting BB60C profile: " << e.what() << std::endl;
        return OperationResult::InternalError;
    }
}

// List available configuration profiles
std::vector<std::string> BB60CAbstractDevice::listProfiles() const {
    std::vector<std::string> profiles;
    
    try {
        if (!fs::exists(profileDirectory_)) {
            return profiles;
        }
        
        // List all JSON files in profile directory
        for (const auto& entry : fs::directory_iterator(profileDirectory_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                // Add file name without extension
                profiles.push_back(entry.path().stem().string());
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error listing BB60C profiles: " << e.what() << std::endl;
    }
    
    return profiles;
}

// Optimize parameters for a specific use case
SignalSourceDevice::OperationResult BB60CAbstractDevice::optimizeForUseCase(const std::string& useCase) {
    // Check if device is open
    if (!isOpen()) {
        return OperationResult::DeviceNotOpen;
    }
    
    try {
        // Set parameters based on use case
        BB60CParams params;
        StreamingConfig config;
        
        if (useCase == "sensitivity") {
            // Optimize for maximum sensitivity
            params.gainMode = BB60CParams::GainMode::Auto;
            params.attenuationMode = BB60CParams::Attenuation::Low;
            params.referenceLevel = -50.0;
            params.decimation = 16;  // Lower sample rate for better sensitivity
            
            config.centerFrequency = currentConfig_.centerFrequency;
            config.bandwidth = 1.0e6;  // Narrower bandwidth for better SNR
            config.sampleRate = BB60C_MAX_SAMPLE_RATE / params.decimation;
            config.bufferSize = 32768;
            
        } else if (useCase == "speed") {
            // Optimize for maximum sample rate
            params.gainMode = BB60CParams::GainMode::FastAttack;
            params.attenuationMode = BB60CParams::Attenuation::Auto;
            params.referenceLevel = -20.0;
            params.decimation = 1;  // Maximum sample rate
            
            config.centerFrequency = currentConfig_.centerFrequency;
            config.bandwidth = 27.0e6;  // Maximum bandwidth
            config.sampleRate = BB60C_MAX_SAMPLE_RATE;
            config.bufferSize = 65536;  // Larger buffer for high data rates
            
        } else if (useCase == "balanced") {
            // Balanced configuration
            params.gainMode = BB60CParams::GainMode::Auto;
            params.attenuationMode = BB60CParams::Attenuation::Auto;
            params.referenceLevel = -30.0;
            params.decimation = 4;  // Good balance of sample rate and performance
            
            config.centerFrequency = currentConfig_.centerFrequency;
            config.bandwidth = 5.0e6;  // Medium bandwidth
            config.sampleRate = BB60C_MAX_SAMPLE_RATE / params.decimation;
            config.bufferSize = 32768;
            
        } else if (useCase == "tdoa") {
            // Optimized for TDOA direction finding
            params.gainMode = BB60CParams::GainMode::FastAttack;
            params.attenuationMode = BB60CParams::Attenuation::Auto;
            params.referenceLevel = -30.0;
            params.decimation = 8;  // Balance between sample rate and data volume
            
            config.centerFrequency = currentConfig_.centerFrequency;
            config.bandwidth = 2.5e6;  // Moderate bandwidth for TDOA
            config.sampleRate = BB60C_MAX_SAMPLE_RATE / params.decimation;
            config.enableTimeStamp = true;  // Enable time stamping for TDOA
            config.bufferSize = 32768;
            
        } else {
            return OperationResult::InvalidParameter;
        }
        
        // Apply the optimized configuration
        OperationResult result = setParams(params);
        if (result != OperationResult::Success) {
            return result;
        }
        
        return configureStreaming(config);
    } catch (const std::exception& e) {
        std::cerr << "Error optimizing BB60C parameters: " << e.what() << std::endl;
        return OperationResult::InternalError;
    }
}

// Validate streaming configuration parameters
bool BB60CAbstractDevice::validateStreamingConfig(const StreamingConfig& config) {
    // Check center frequency range
    if (config.centerFrequency < 9.0e3 || config.centerFrequency > 6.0e9) {
        std::cerr << "Invalid center frequency: " << config.centerFrequency << " Hz" << std::endl;
        return false;
    }
    
    // Check bandwidth
    if (config.bandwidth <= 0.0 || config.bandwidth > 27.0e6) {
        std::cerr << "Invalid bandwidth: " << config.bandwidth << " Hz" << std::endl;
        return false;
    }
    
    // Check sample rate
    if (config.sampleRate <= 0.0 || config.sampleRate > BB60C_MAX_SAMPLE_RATE) {
        std::cerr << "Invalid sample rate: " << config.sampleRate << " Hz" << std::endl;
        return false;
    }
    
    // Check data format
    if (config.format != DataFormat::Float32 && config.format != DataFormat::Int16) {
        std::cerr << "Unsupported data format for BB60C" << std::endl;
        return false;
    }
    
    // Check buffer size
    if (config.bufferSize < 1024 || config.bufferSize > 1048576) {
        std::cerr << "Invalid buffer size: " << config.bufferSize << std::endl;
        return false;
    }
    
    // All checks passed
    return true;
}

// Validate BB60C parameters
bool BB60CAbstractDevice::validateParams(const BB60CParams& params) {
    // Check decimation value
    bool validDecimation = false;
    for (int dec : VALID_DECIMATION_VALUES) {
        if (params.decimation == dec) {
            validDecimation = true;
            break;
        }
    }
    
    if (!validDecimation) {
        std::cerr << "Invalid decimation value: " << params.decimation << std::endl;
        return false;
    }
    
    // Check reference level
    if (params.referenceLevel < -130.0 || params.referenceLevel > 20.0) {
        std::cerr << "Invalid reference level: " << params.referenceLevel << " dBm" << std::endl;
        return false;
    }
    
    // Check RF gain (only if in manual mode)
    if (params.gainMode == BB60CParams::GainMode::Manual) {
        if (params.rfGain < -30 || params.rfGain > 30) {
            std::cerr << "Invalid RF gain: " << params.rfGain << std::endl;
            return false;
        }
    }
    
    // All checks passed
    return true;
}

// Callback function for BB60C device
void BB60CAbstractDevice::deviceCallback(const void* data, size_t length, double timestamp) {
    // Forward to user callback if set
    if (userCallback_) {
        userCallback_(data, length, timestamp, userData_);
    }
}

// Calculate optimal decimation for a target sample rate
int BB60CAbstractDevice::calculateDecimation(double sampleRate) {
    if (sampleRate <= 0.0) {
        return 4; // Default decimation
    }
    
    // Find the decimation value that gives the closest sample rate
    double targetRate = BB60C_MAX_SAMPLE_RATE / sampleRate;
    
    // Find the closest valid decimation value
    int bestDecimation = 4; // Default
    double bestDiff = std::abs(4.0 - targetRate);
    
    for (int dec : VALID_DECIMATION_VALUES) {
        double diff = std::abs(static_cast<double>(dec) - targetRate);
        if (diff < bestDiff) {
            bestDiff = diff;
            bestDecimation = dec;
        }
    }
    
    return bestDecimation;
}

// Calculate actual sample rate from decimation
double BB60CAbstractDevice::calculateSampleRate(int decimation) {
    // Ensure decimation is valid
    bool validDecimation = false;
    for (int dec : VALID_DECIMATION_VALUES) {
        if (decimation == dec) {
            validDecimation = true;
            break;
        }
    }
    
    if (!validDecimation) {
        return 0.0;
    }
    
    // Calculate sample rate
    return BB60C_MAX_SAMPLE_RATE / decimation;
}

// Check if directory exists, create if not
bool BB60CAbstractDevice::ensureDirectoryExists(const std::string& dir) {
    try {
        if (!fs::exists(dir)) {
            return fs::create_directories(dir);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error creating directory '" << dir << "': " << e.what() << std::endl;
        return false;
    }
}

} // namespace devices
} // namespace tdoa 