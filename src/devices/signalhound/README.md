# SignalHound BB60C Integration

This module provides a C/C++ wrapper around the SignalHound BB60C API, enabling device discovery, initialization, configuration, and I/Q data streaming for the TDOA Direction Finder project.

## Components

- `bb60c_device.cpp/h`: Core device wrapper implementing the hardware interface
- `bb60c_abstract_device.cpp/h`: Abstraction layer implementation for BB60C device
- `signal_source_device.h`: Abstract base class for all signal source devices
- `signal_source_factory.cpp/h`: Factory for creating signal source devices
- `bb60c_device_test.cpp`: Unit tests for the core device wrapper
- `bb60c_abstract_device_test.cpp`: Unit tests for the abstraction layer
- `signal_source_factory_test.cpp`: Unit tests for the factory

## Features

- Device discovery and enumeration
- Device initialization and configuration
- I/Q data streaming with callback-based architecture
- Multi-threaded streaming with circular buffer management
- Performance monitoring and metrics collection
- Error handling with descriptive error messages
- Thread-safe operation with proper resource management
- Parameter validation with range checking
- Configuration profile system for saving and loading device settings
- Automatic parameter optimization for different use cases
- Hardware abstraction layer for supporting multiple device types

## Usage

### Device Creation with Factory

```cpp
// Get factory instance
auto& factory = SignalSourceFactory::getInstance();

// Create BB60C device through abstraction layer
auto device = factory.createDevice("BB60C");

// Check if device was created
if (!device) {
    std::cerr << "Failed to create device" << std::endl;
    return 1;
}

// Get available devices
auto devices = device->getAvailableDevices();

// Open the first device if available
if (!devices.empty()) {
    auto result = device->open();
    // Check result...
}
```

### Device Configuration and Streaming

```cpp
// Configure streaming parameters
SignalSourceDevice::StreamingConfig config;
config.centerFrequency = 915.0e6;  // 915 MHz
config.bandwidth = 5.0e6;          // 5 MHz
config.sampleRate = 10.0e6;        // 10 MS/s
config.format = SignalSourceDevice::DataFormat::Float32;
config.bufferSize = 32768;         // 32K samples

// Apply configuration
auto result = device->configureStreaming(config);

// Define I/Q data callback
auto iqCallback = [](const void* data, size_t length, double timestamp, void* userData) {
    // Process I/Q data
    const float* samples = static_cast<const float*>(data);
    // Process samples...
};

// Start streaming
result = device->startStreaming(iqCallback);

// Monitor performance
auto metrics = device->getStreamingMetrics();
std::cout << "Sample rate: " << metrics.sampleRate / 1.0e6 << " MS/s" << std::endl;
std::cout << "Data rate: " << metrics.dataRate / 1.0e6 << " MB/s" << std::endl;
```

### Configuration Profiles

```cpp
// Optimize for a specific use case
device->optimizeForUseCase("tdoa");

// Save the current configuration as a profile
device->saveProfile("tdoa_config");

// List available profiles
auto profiles = device->listProfiles();

// Load a previously saved profile
device->loadProfile("tdoa_config");
```

## Device Parameter Optimization

The BB60C implementation provides automatic parameter optimization for different use cases:

- **sensitivity**: Optimizes for maximum sensitivity with lower sample rate and narrower bandwidth
- **speed**: Optimizes for maximum data throughput with highest sample rate and bandwidth
- **balanced**: Provides a balanced configuration for general use
- **tdoa**: Specific configuration optimized for TDOA direction finding

## Streaming Performance

The I/Q streaming infrastructure is designed to handle high data rates up to 40MS/s with minimal data loss:

- Multi-threaded architecture with producer/consumer pattern
- Pre-allocated buffer pool for efficient memory management 
- Circular buffer system to handle bursts and prevent data loss
- Performance metrics to monitor data throughput and identify bottlenecks
- Configurable buffer sizes to optimize for different operating conditions

## Build and Test

To build the library and test programs:

1. Ensure you have the SignalHound BB60C SDK installed
2. Update the include and library paths in CMakeLists.txt if needed
3. Build using CMake:

```
mkdir build
cd build
cmake ..
make
```

Run the test programs:

```
./bb60c_device_test
./bb60c_abstract_device_test
./signal_source_factory_test
```

## Implementation Status

- [x] Device discovery and enumeration
- [x] Device initialization and basic control
- [x] Configuration interface for I/Q streaming
- [x] Multi-threaded I/Q data streaming infrastructure
- [x] Circular buffer system for high-speed data throughput
- [x] Performance monitoring and metrics
- [x] Error handling and resource management
- [x] Parameter validation with range checking
- [x] Configuration profile system
- [x] Automatic parameter optimization
- [x] Hardware abstraction layer
- [ ] GPS time synchronization

## Dependencies

- SignalHound BB60C API (v5.0+)
- C++17 compatible compiler
- CMake 3.10+
- nlohmann_json library for configuration profiles 