/**
 * @file bb60c_device_test.cpp
 * @brief Unit tests for the BB60C device wrapper
 */

#include "../../../external/signalhound/wrapper/bb60c_device.h"
#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <atomic>
#include <iomanip>

using namespace tdoa::devices;

// Statistics for I/Q data callback
struct IQStats {
    std::atomic<size_t> callCount{0};
    std::atomic<size_t> totalSamples{0};
    std::atomic<size_t> minSamples{std::numeric_limits<size_t>::max()};
    std::atomic<size_t> maxSamples{0};
    std::atomic<double> lastTimestamp{0.0};
    
    void reset() {
        callCount = 0;
        totalSamples = 0;
        minSamples = std::numeric_limits<size_t>::max();
        maxSamples = 0;
        lastTimestamp = 0.0;
    }
};

// Global stats for testing
IQStats g_iqStats;

// Improved I/Q data callback for testing with statistics
void iqDataCallback(const void* data, size_t length, double timestamp) {
    // Update statistics
    g_iqStats.callCount++;
    g_iqStats.totalSamples += length;
    g_iqStats.lastTimestamp = timestamp;
    
    // Update min/max samples
    size_t currentMin = g_iqStats.minSamples.load();
    if (length < currentMin) {
        g_iqStats.minSamples = length;
    }
    
    size_t currentMax = g_iqStats.maxSamples.load();
    if (length > currentMax) {
        g_iqStats.maxSamples = length;
    }
    
    // For float data, we could analyze the samples here
    if (g_iqStats.callCount % 10 == 0) {
        std::cout << "Received " << length << " I/Q samples, timestamp: " 
                  << std::fixed << std::setprecision(6) << timestamp 
                  << " (callback #" << g_iqStats.callCount << ")" << std::endl;
        
        // Print a few samples for verification if we have float data
        const float* samples = static_cast<const float*>(data);
        if (samples && length > 0) {
            std::cout << "  First few samples: ";
            for (size_t i = 0; i < std::min(length, size_t(5)); i++) {
                std::cout << std::fixed << std::setprecision(3) 
                          << samples[i*2] << "+" << samples[i*2+1] << "i ";
            }
            std::cout << std::endl;
        }
    }
}

// Test device discovery and initialization
bool testDeviceDiscovery() {
    std::cout << "Testing device discovery..." << std::endl;
    
    try {
        // Get list of available devices
        auto deviceList = BB60CDevice::getDeviceList();
        
        // Print the device list
        std::cout << "Found " << deviceList.size() << " BB60C devices:" << std::endl;
        for (const auto& serial : deviceList) {
            std::cout << "  - " << serial << std::endl;
        }
        
        if (deviceList.empty()) {
            std::cout << "No devices found, skipping device tests" << std::endl;
            return true;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error in device discovery: " << e.what() << std::endl;
        return false;
    }
}

// Test device initialization and basic control
bool testDeviceInitialization() {
    std::cout << "Testing device initialization..." << std::endl;
    
    try {
        // Create device object
        BB60CDevice device;
        
        // Get list of available devices
        auto deviceList = BB60CDevice::getDeviceList();
        
        if (deviceList.empty()) {
            std::cout << "No devices found, skipping initialization test" << std::endl;
            return true;
        }
        
        // Open the first available device
        std::cout << "Opening device..." << std::endl;
        device.open();
        
        // Check if device is open
        if (!device.isOpen()) {
            std::cerr << "Failed to open device" << std::endl;
            return false;
        }
        
        // Get device serial number and firmware version
        std::cout << "Device serial number: " << device.getSerialNumber() << std::endl;
        std::cout << "Device firmware version: " << device.getFirmwareVersion() << std::endl;
        
        // Configure for I/Q streaming
        std::cout << "Configuring device for I/Q streaming..." << std::endl;
        BB60CDevice::IQConfig config;
        config.centerFreq = 915.0e6;  // 915 MHz
        config.bandwidth = 5.0e6;     // 5 MHz
        config.decimation = 4;        // Decimation factor
        config.useFloat = true;       // Use float data format
        
        device.configureIQ(config);
        
        // Configure I/O ports
        std::cout << "Configuring I/O ports..." << std::endl;
        device.configureIO(0, 0);  // Default modes
        
        // Reset the device
        std::cout << "Resetting device..." << std::endl;
        device.reset();
        
        // Close the device
        std::cout << "Closing device..." << std::endl;
        device.close();
        
        if (device.isOpen()) {
            std::cerr << "Device is still open after close()" << std::endl;
            return false;
        }
        
        std::cout << "Device initialization test passed" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error in device initialization: " << e.what() << std::endl;
        return false;
    }
}

// Test I/Q streaming with enhanced functionality
bool testIQStreaming() {
    std::cout << "Testing I/Q streaming..." << std::endl;
    
    try {
        // Create device object
        BB60CDevice device;
        
        // Get list of available devices
        auto deviceList = BB60CDevice::getDeviceList();
        
        if (deviceList.empty()) {
            std::cout << "No devices found, skipping streaming test" << std::endl;
            return true;
        }
        
        // Open the first available device
        device.open();
        
        // Configure buffer size
        device.setBufferSize(32768);  // 32K samples
        
        // Configure for I/Q streaming
        BB60CDevice::IQConfig config;
        config.centerFreq = 915.0e6;  // 915 MHz
        config.bandwidth = 5.0e6;     // 5 MHz
        config.decimation = 4;        // Decimation factor (40MS/s / 4 = 10MS/s)
        config.useFloat = true;       // Use float data format
        
        device.configureIQ(config);
        
        // Reset statistics
        g_iqStats.reset();
        
        // Start I/Q streaming
        std::cout << "Starting I/Q streaming..." << std::endl;
        device.startIQStreaming(iqDataCallback);
        
        // Stream for a longer time to test stability
        std::cout << "Streaming for 5 seconds..." << std::endl;
        
        // Check metrics periodically
        for (int i = 0; i < 5; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            auto metrics = device.getStreamingMetrics();
            std::cout << "Streaming metrics at " << (i+1) << "s:" << std::endl;
            std::cout << "  Sample rate: " << std::fixed << std::setprecision(2) 
                      << metrics.sampleRate / 1.0e6 << " MS/s" << std::endl;
            std::cout << "  Data rate: " << std::fixed << std::setprecision(2) 
                      << metrics.dataRate / 1.0e6 << " MB/s" << std::endl;
            std::cout << "  Dropped buffers: " << metrics.droppedBuffers << std::endl;
            std::cout << "  Avg callback time: " << std::fixed << std::setprecision(2) 
                      << metrics.avgCallbackTime << " µs" << std::endl;
            
            std::cout << "  Callback count: " << g_iqStats.callCount << std::endl;
            std::cout << "  Total samples: " << g_iqStats.totalSamples << std::endl;
            if (g_iqStats.callCount > 0) {
                std::cout << "  Min samples: " << g_iqStats.minSamples << std::endl;
                std::cout << "  Max samples: " << g_iqStats.maxSamples << std::endl;
                std::cout << "  Avg samples: " << g_iqStats.totalSamples / g_iqStats.callCount 
                          << std::endl;
            }
        }
        
        // Stop streaming
        std::cout << "Stopping I/Q streaming..." << std::endl;
        device.stopIQStreaming();
        
        // Close the device
        device.close();
        
        // Print final statistics
        std::cout << "I/Q streaming test completed" << std::endl;
        std::cout << "Final statistics:" << std::endl;
        std::cout << "  Total callbacks: " << g_iqStats.callCount << std::endl;
        std::cout << "  Total I/Q samples: " << g_iqStats.totalSamples << std::endl;
        if (g_iqStats.callCount > 0) {
            std::cout << "  Average samples per callback: " 
                      << g_iqStats.totalSamples / g_iqStats.callCount << std::endl;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error in I/Q streaming: " << e.what() << std::endl;
        return false;
    }
}

// Main test function
int main() {
    std::cout << "BB60C Device Wrapper Test" << std::endl;
    std::cout << "=======================" << std::endl;
    
    bool allTestsPassed = true;
    
    // Run tests
    allTestsPassed &= testDeviceDiscovery();
    allTestsPassed &= testDeviceInitialization();
    allTestsPassed &= testIQStreaming();
    
    // Print overall result
    std::cout << "\nTest summary: " << (allTestsPassed ? "PASSED" : "FAILED") << std::endl;
    
    return allTestsPassed ? 0 : 1;
} 