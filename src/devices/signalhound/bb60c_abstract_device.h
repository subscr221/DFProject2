/**
 * @file bb60c_abstract_device.h
 * @brief SignalHound BB60C implementation of the device abstraction layer
 */

#pragma once

#include "../signal_source_device.h"
#include "../../../external/signalhound/wrapper/bb60c_device.h"
#include <memory>
#include <string>
#include <map>
#include <filesystem>

namespace tdoa {
namespace devices {

/**
 * @class BB60CParams
 * @brief BB60C specific parameters
 */
class BB60CParams : public SignalSourceDevice::DeviceParams {
public:
    // IO port modes
    enum class Port1Mode {
        PulseTrigger = 0,    ///< Generate pulse on trigger (default)
        FrameSync = 1,       ///< Generate pulse on frame sync
        DeviceIO = 2,        ///< Direct device I/O control
        ExternalReference = 3 ///< External reference input
    };
    
    enum class Port2Mode {
        TriggerInput = 0,    ///< External trigger input (default)
        DeviceIO = 4,        ///< Direct device I/O control
        OutputReference = 6  ///< 10MHz output reference
    };
    
    // Gain control modes
    enum class GainMode {
        Auto = 0,         ///< Automatic gain control (default)
        Manual = 1,       ///< Manual gain control
        FastAttack = 2,   ///< Fast attack AGC
        SlowAttack = 3    ///< Slow attack AGC
    };
    
    // Attenuation settings
    enum class Attenuation {
        Auto = 0,      ///< Automatic attenuation (default)
        Low = 1,       ///< Low attenuation
        Medium = 2,    ///< Medium attenuation
        High = 3       ///< High attenuation
    };
    
    // RF filter mode
    enum class RFFilterMode {
        Auto = 0,      ///< Automatic filter selection (default)
        LowFreq = 1,   ///< Force low frequency filter
        HighFreq = 2   ///< Force high frequency filter
    };
    
    // Decimation controls sample rate
    int decimation;            ///< Decimation factor (1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192)
    Port1Mode port1Mode;       ///< Mode for digital I/O port 1
    Port2Mode port2Mode;       ///< Mode for digital I/O port 2
    GainMode gainMode;         ///< Gain control mode
    int rfGain;                ///< RF gain value (manual mode only)
    Attenuation attenuationMode; ///< RF attenuation mode
    RFFilterMode rfFilterMode; ///< RF input filter mode
    double referenceLevel;     ///< Reference level in dBm
    
    /**
     * @brief Default constructor with reasonable values
     */
    BB60CParams()
        : decimation(4)                // 10 MS/s default
        , port1Mode(Port1Mode::PulseTrigger)
        , port2Mode(Port2Mode::TriggerInput)
        , gainMode(GainMode::Auto)
        , rfGain(0)
        , attenuationMode(Attenuation::Auto)
        , rfFilterMode(RFFilterMode::Auto)
        , referenceLevel(-20.0)        // -20 dBm
    {}
};

/**
 * @class BB60CAbstractDevice
 * @brief BB60C implementation of the SignalSourceDevice interface
 */
class BB60CAbstractDevice : public SignalSourceDevice {
public:
    /**
     * @brief Constructor
     */
    BB60CAbstractDevice();
    
    /**
     * @brief Destructor
     */
    ~BB60CAbstractDevice() override;
    
    /**
     * @brief Get available BB60C devices
     * @return Vector of device information structures
     */
    std::vector<DeviceInfo> getAvailableDevices() override;
    
    /**
     * @brief Open the BB60C device
     * @param serialNumber Optional serial number to open a specific device
     * @return Operation result
     */
    OperationResult open(const std::string& serialNumber = "") override;
    
    /**
     * @brief Close the BB60C device
     * @return Operation result
     */
    OperationResult close() override;
    
    /**
     * @brief Check if BB60C device is open
     * @return True if device is open
     */
    bool isOpen() const override;
    
    /**
     * @brief Get BB60C device information
     * @return Device information structure
     */
    DeviceInfo getDeviceInfo() const override;
    
    /**
     * @brief Set BB60C specific parameters
     * @param params Device parameters (must be BB60CParams)
     * @return Operation result
     */
    OperationResult setParams(const DeviceParams& params) override;
    
    /**
     * @brief Configure streaming parameters
     * @param config Streaming configuration
     * @return Operation result
     */
    OperationResult configureStreaming(const StreamingConfig& config) override;
    
    /**
     * @brief Start streaming I/Q data
     * @param callback Function to receive I/Q data
     * @param userData User data pointer passed to callback
     * @return Operation result
     */
    OperationResult startStreaming(StreamingCallback callback, void* userData = nullptr) override;
    
    /**
     * @brief Stop streaming I/Q data
     * @return Operation result
     */
    OperationResult stopStreaming() override;
    
    /**
     * @brief Get streaming performance metrics
     * @return Streaming metrics structure
     */
    StreamingMetrics getStreamingMetrics() const override;
    
    /**
     * @brief Reset device to default state
     * @return Operation result
     */
    OperationResult reset() override;

    /**
     * @brief Save current configuration to a profile
     * @param profileName Name of the profile
     * @return Operation result
     */
    OperationResult saveProfile(const std::string& profileName);
    
    /**
     * @brief Load configuration from a profile
     * @param profileName Name of the profile
     * @return Operation result
     */
    OperationResult loadProfile(const std::string& profileName);
    
    /**
     * @brief Delete a configuration profile
     * @param profileName Name of the profile
     * @return Operation result
     */
    OperationResult deleteProfile(const std::string& profileName);
    
    /**
     * @brief List available configuration profiles
     * @return Vector of profile names
     */
    std::vector<std::string> listProfiles() const;
    
    /**
     * @brief Optimize parameters for a specific use case
     * @param useCase Predefined use case name
     * @return Operation result
     */
    OperationResult optimizeForUseCase(const std::string& useCase);

private:
    std::unique_ptr<BB60CDevice> device_;    ///< Underlying BB60C device
    BB60CParams currentParams_;              ///< Current device parameters
    StreamingConfig currentConfig_;          ///< Current streaming configuration
    StreamingCallback userCallback_;         ///< User callback for I/Q data
    void* userData_;                         ///< User data for callback
    std::string profileDirectory_;           ///< Directory for configuration profiles
    
    /**
     * @brief Validate streaming configuration parameters
     * @param config Configuration to validate
     * @return True if configuration is valid
     */
    bool validateStreamingConfig(const StreamingConfig& config);
    
    /**
     * @brief Validate BB60C parameters
     * @param params Parameters to validate
     * @return True if parameters are valid
     */
    bool validateParams(const BB60CParams& params);
    
    /**
     * @brief Callback function for BB60C device
     * @param data I/Q data buffer
     * @param length Number of samples
     * @param timestamp Timestamp
     */
    void deviceCallback(const void* data, size_t length, double timestamp);
    
    /**
     * @brief Calculate optimal decimation for a target sample rate
     * @param sampleRate Desired sample rate in samples/second
     * @return Optimal decimation factor
     */
    int calculateDecimation(double sampleRate);
    
    /**
     * @brief Calculate actual sample rate from decimation
     * @param decimation Decimation factor
     * @return Actual sample rate in samples/second
     */
    double calculateSampleRate(int decimation);
    
    /**
     * @brief Check if directory exists, create if not
     * @param dir Directory path
     * @return True if directory exists or was created
     */
    bool ensureDirectoryExists(const std::string& dir);
};

} // namespace devices
} // namespace tdoa 