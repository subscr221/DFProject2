/**
 * @file signal_source_factory_test.cpp
 * @brief Test program for the signal source factory
 */

#include "signal_source_factory.h"
#include <iostream>
#include <string>
#include <vector>

using namespace tdoa::devices;

int main() {
    std::cout << "Signal Source Factory Test" << std::endl;
    std::cout << "==========================" << std::endl;
    
    // Get factory instance
    auto& factory = SignalSourceFactory::getInstance();
    
    // Get supported device types
    std::cout << "Supported device types:" << std::endl;
    for (const auto& type : factory.getSupportedDeviceTypes()) {
        std::cout << "  - " << type << std::endl;
    }
    
    // Check if BB60C is supported
    std::cout << "BB60C supported: " 
              << (factory.isDeviceTypeSupported("BB60C") ? "Yes" : "No") << std::endl;
    
    // Create a BB60C device
    std::cout << "\nCreating BB60C device..." << std::endl;
    auto device = factory.createDevice("BB60C");
    
    if (!device) {
        std::cerr << "Failed to create BB60C device" << std::endl;
        return 1;
    }
    
    // Get available devices
    auto devices = device->getAvailableDevices();
    
    std::cout << "Found " << devices.size() << " BB60C devices:" << std::endl;
    for (const auto& info : devices) {
        std::cout << "  - " << info.modelName << " (" << info.serialNumber << ")" << std::endl;
        std::cout << "    Frequency range: " << info.capabilities.minFrequency / 1e3 
                  << " kHz - " << info.capabilities.maxFrequency / 1e6 << " MHz" << std::endl;
        std::cout << "    Max sample rate: " << info.capabilities.maxSampleRate / 1e6 
                  << " MS/s" << std::endl;
    }
    
    // Try to open the device if any available
    if (!devices.empty()) {
        std::cout << "\nOpening first available device..." << std::endl;
        auto result = device->open();
        
        if (result != SignalSourceDevice::OperationResult::Success) {
            std::cerr << "Failed to open device: " 
                    << SignalSourceDevice::resultToString(result) << std::endl;
            return 1;
        }
        
        // Get device info
        auto info = device->getDeviceInfo();
        std::cout << "Connected to " << info.modelName << " (" << info.serialNumber << ")" << std::endl;
        
        // Reset the device
        std::cout << "Resetting device..." << std::endl;
        result = device->reset();
        
        if (result != SignalSourceDevice::OperationResult::Success) {
            std::cerr << "Failed to reset device: " 
                    << SignalSourceDevice::resultToString(result) << std::endl;
            return 1;
        }
        
        // Close the device
        std::cout << "Closing device..." << std::endl;
        result = device->close();
        
        if (result != SignalSourceDevice::OperationResult::Success) {
            std::cerr << "Failed to close device: " 
                    << SignalSourceDevice::resultToString(result) << std::endl;
            return 1;
        }
    }
    
    std::cout << "\nTest completed successfully" << std::endl;
    return 0;
} 