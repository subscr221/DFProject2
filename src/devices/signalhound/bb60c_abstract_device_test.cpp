/**
 * @file bb60c_abstract_device_test.cpp
 * @brief Test program for the BB60C abstract device
 */

#include "bb60c_abstract_device.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>

using namespace tdoa::devices;

// Counter for callback invocations
std::atomic<size_t> callbackCount{0};
std::atomic<size_t> totalSamples{0};

// Callback function for I/Q data
void iqDataCallback(const void* data, size_t length, double timestamp, void* userData) {
    callbackCount++;
    totalSamples += length;
    
    if (callbackCount % 10 == 0) {
        std::cout << "Received " << length << " I/Q samples, timestamp: " 
                  << std::fixed << std::setprecision(6) << timestamp << std::endl;
        
        // Print the first few samples if float format
        const float* samples = static_cast<const float*>(data);
        if (samples) {
            std::cout << "  First few samples: ";
            for (size_t i = 0; i < std::min(length, size_t(3)); i++) {
                std::cout << std::fixed << std::setprecision(3) 
                          << samples[i*2] << "+" << samples[i*2+1] << "i ";
            }
            std::cout << std::endl;
        }
    }
}

// Test configuration profile saving and loading
bool testProfiles(BB60CAbstractDevice& device) {
    std::cout << "\nTesting configuration profiles..." << std::endl;
    
    try {
        // Set up test configurations for different use cases
        device.optimizeForUseCase("sensitivity");
        auto result = device.saveProfile("sensitivity");
        if (result != SignalSourceDevice::OperationResult::Success) {
            std::cerr << "Failed to save sensitivity profile: " 
                      << SignalSourceDevice::resultToString(result) << std::endl;
            return false;
        }
        
        device.optimizeForUseCase("speed");
        result = device.saveProfile("speed");
        if (result != SignalSourceDevice::OperationResult::Success) {
            std::cerr << "Failed to save speed profile: " 
                      << SignalSourceDevice::resultToString(result) << std::endl;
            return false;
        }
        
        // List available profiles
        auto profiles = device.listProfiles();
        std::cout << "Available profiles: ";
        for (const auto& profile : profiles) {
            std::cout << profile << " ";
        }
        std::cout << std::endl;
        
        // Load a profile
        result = device.loadProfile("sensitivity");
        if (result != SignalSourceDevice::OperationResult::Success) {
            std::cerr << "Failed to load sensitivity profile: " 
                      << SignalSourceDevice::resultToString(result) << std::endl;
            return false;
        }
        
        std::cout << "Successfully loaded sensitivity profile" << std::endl;
        
        // Delete a profile
        result = device.deleteProfile("speed");
        if (result != SignalSourceDevice::OperationResult::Success) {
            std::cerr << "Failed to delete speed profile: " 
                      << SignalSourceDevice::resultToString(result) << std::endl;
            return false;
        }
        
        std::cout << "Successfully deleted speed profile" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error in profile tests: " << e.what() << std::endl;
        return false;
    }
}

// Test parameter optimization for different use cases
bool testOptimization(BB60CAbstractDevice& device) {
    std::cout << "\nTesting parameter optimization..." << std::endl;
    
    try {
        // Test different use cases
        std::vector<std::string> useCases = {
            "sensitivity", "speed", "balanced", "tdoa"
        };
        
        for (const auto& useCase : useCases) {
            std::cout << "Optimizing for use case: " << useCase << std::endl;
            
            auto result = device.optimizeForUseCase(useCase);
            if (result != SignalSourceDevice::OperationResult::Success) {
                std::cerr << "Failed to optimize for " << useCase << ": " 
                          << SignalSourceDevice::resultToString(result) << std::endl;
                return false;
            }
            
            // Wait a moment for settings to apply
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Start and stop streaming to verify configuration
            callbackCount = 0;
            totalSamples = 0;
            
            result = device.startStreaming(iqDataCallback);
            if (result != SignalSourceDevice::OperationResult::Success) {
                std::cerr << "Failed to start streaming for " << useCase << ": " 
                          << SignalSourceDevice::resultToString(result) << std::endl;
                return false;
            }
            
            // Stream for a short time
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Check metrics
            auto metrics = device.getStreamingMetrics();
            std::cout << "  Sample rate: " << std::fixed << std::setprecision(2) 
                      << metrics.sampleRate / 1.0e6 << " MS/s" << std::endl;
            std::cout << "  Data rate: " << std::fixed << std::setprecision(2) 
                      << metrics.dataRate / 1.0e6 << " MB/s" << std::endl;
            std::cout << "  Callbacks: " << callbackCount << std::endl;
            std::cout << "  Total samples: " << totalSamples << std::endl;
            
            // Stop streaming
            result = device.stopStreaming();
            if (result != SignalSourceDevice::OperationResult::Success) {
                std::cerr << "Failed to stop streaming for " << useCase << ": " 
                          << SignalSourceDevice::resultToString(result) << std::endl;
                return false;
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error in optimization tests: " << e.what() << std::endl;
        return false;
    }
}

// Main test function
int main() {
    std::cout << "BB60C Abstract Device Test" << std::endl;
    std::cout << "=========================" << std::endl;
    
    // Create device
    BB60CAbstractDevice device;
    
    // Get available devices
    auto devices = device.getAvailableDevices();
    
    std::cout << "Found " << devices.size() << " BB60C devices:" << std::endl;
    for (const auto& info : devices) {
        std::cout << "  - " << info.modelName << " (" << info.serialNumber << ")" << std::endl;
        std::cout << "    Frequency range: " << info.capabilities.minFrequency / 1e3 
                  << " kHz - " << info.capabilities.maxFrequency / 1e6 << " MHz" << std::endl;
        std::cout << "    Max sample rate: " << info.capabilities.maxSampleRate / 1e6 
                  << " MS/s" << std::endl;
    }
    
    if (devices.empty()) {
        std::cout << "No devices found, exiting test" << std::endl;
        return 0;
    }
    
    // Open the first device
    std::cout << "\nOpening device..." << std::endl;
    auto result = device.open();
    
    if (result != SignalSourceDevice::OperationResult::Success) {
        std::cerr << "Failed to open device: " 
                  << SignalSourceDevice::resultToString(result) << std::endl;
        return 1;
    }
    
    // Get device info
    auto info = device.getDeviceInfo();
    std::cout << "Connected to " << info.modelName << " (" << info.serialNumber << ")" << std::endl;
    std::cout << "Firmware version: " << info.firmwareVersion << std::endl;
    
    // Reset device
    std::cout << "\nResetting device..." << std::endl;
    result = device.reset();
    
    if (result != SignalSourceDevice::OperationResult::Success) {
        std::cerr << "Failed to reset device: " 
                  << SignalSourceDevice::resultToString(result) << std::endl;
        return 1;
    }
    
    // Set parameters
    std::cout << "\nSetting device parameters..." << std::endl;
    BB60CParams params;
    params.decimation = 4;  // 10 MS/s
    params.port1Mode = BB60CParams::Port1Mode::PulseTrigger;
    params.port2Mode = BB60CParams::Port2Mode::TriggerInput;
    params.gainMode = BB60CParams::GainMode::Auto;
    params.attenuationMode = BB60CParams::Attenuation::Auto;
    params.referenceLevel = -30.0;
    
    result = device.setParams(params);
    if (result != SignalSourceDevice::OperationResult::Success) {
        std::cerr << "Failed to set parameters: " 
                  << SignalSourceDevice::resultToString(result) << std::endl;
        return 1;
    }
    
    // Configure streaming
    std::cout << "\nConfiguring streaming..." << std::endl;
    SignalSourceDevice::StreamingConfig config;
    config.centerFrequency = 915.0e6;  // 915 MHz
    config.bandwidth = 5.0e6;          // 5 MHz
    config.sampleRate = 10.0e6;        // 10 MS/s
    config.format = SignalSourceDevice::DataFormat::Float32;
    config.bufferSize = 32768;         // 32K samples
    
    result = device.configureStreaming(config);
    if (result != SignalSourceDevice::OperationResult::Success) {
        std::cerr << "Failed to configure streaming: " 
                  << SignalSourceDevice::resultToString(result) << std::endl;
        return 1;
    }
    
    // Start streaming
    std::cout << "\nStarting I/Q streaming..." << std::endl;
    callbackCount = 0;
    totalSamples = 0;
    
    result = device.startStreaming(iqDataCallback);
    if (result != SignalSourceDevice::OperationResult::Success) {
        std::cerr << "Failed to start streaming: " 
                  << SignalSourceDevice::resultToString(result) << std::endl;
        return 1;
    }
    
    // Stream for a few seconds
    std::cout << "Streaming for 3 seconds..." << std::endl;
    for (int i = 0; i < 3; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Get metrics
        auto metrics = device.getStreamingMetrics();
        std::cout << "Metrics at " << (i+1) << "s:" << std::endl;
        std::cout << "  Sample rate: " << std::fixed << std::setprecision(2) 
                  << metrics.sampleRate / 1.0e6 << " MS/s" << std::endl;
        std::cout << "  Data rate: " << std::fixed << std::setprecision(2) 
                  << metrics.dataRate / 1.0e6 << " MB/s" << std::endl;
        std::cout << "  Dropped buffers: " << metrics.droppedBuffers << std::endl;
        std::cout << "  Callbacks: " << callbackCount << std::endl;
        std::cout << "  Total samples: " << totalSamples << std::endl;
    }
    
    // Stop streaming
    std::cout << "\nStopping streaming..." << std::endl;
    result = device.stopStreaming();
    if (result != SignalSourceDevice::OperationResult::Success) {
        std::cerr << "Failed to stop streaming: " 
                  << SignalSourceDevice::resultToString(result) << std::endl;
        return 1;
    }
    
    // Test profiles
    bool profilesOk = testProfiles(device);
    
    // Test optimization
    bool optimizationOk = testOptimization(device);
    
    // Close device
    std::cout << "\nClosing device..." << std::endl;
    result = device.close();
    if (result != SignalSourceDevice::OperationResult::Success) {
        std::cerr << "Failed to close device: " 
                  << SignalSourceDevice::resultToString(result) << std::endl;
        return 1;
    }
    
    // Print overall result
    std::cout << "\nTest summary:" << std::endl;
    std::cout << "  Basic functionality: " << (result == SignalSourceDevice::OperationResult::Success ? "PASSED" : "FAILED") << std::endl;
    std::cout << "  Configuration profiles: " << (profilesOk ? "PASSED" : "FAILED") << std::endl;
    std::cout << "  Parameter optimization: " << (optimizationOk ? "PASSED" : "FAILED") << std::endl;
    
    return (result == SignalSourceDevice::OperationResult::Success && profilesOk && optimizationOk) ? 0 : 1;
} 