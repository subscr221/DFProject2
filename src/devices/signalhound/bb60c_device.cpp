/**
 * @file bb60c_device.cpp
 * @brief Implementation of the BB60C device wrapper
 */

#include "../../../external/signalhound/wrapper/bb60c_device.h"
#include <stdexcept>
#include <cstring>
#include <thread>
#include <chrono>
#include <iostream>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>

// Include the actual SignalHound BB60 API headers
// The location may vary based on your project setup
#include "bb_api.h"

// Buffer size constants
#define DEFAULT_BUFFER_SIZE 16384     // Default buffer size for I/Q samples (16K samples)
#define MAX_BUFFER_COUNT 32           // Maximum number of buffers in the circular queue
#define BB60C_MAX_BUFFER_SIZE 262144  // Max buffer size for a single fetch (from BB60C API docs)

namespace tdoa {
namespace devices {

// Structure for buffered I/Q data
struct IQBuffer {
    std::vector<float> data;      // For float mode
    std::vector<int16_t> rawData; // For raw mode (16-bit shorts)
    double timestamp;             // Timestamp for the first sample
    bool isFloat;                 // Whether data is float or raw
    size_t sampleCount;           // Number of I/Q samples (complex pairs)
    
    // Constructor for pre-allocated buffers
    IQBuffer(size_t capacity, bool useFloat) 
        : isFloat(useFloat)
        , sampleCount(0)
        , timestamp(0.0)
    {
        if (useFloat) {
            data.resize(capacity * 2); // Complex samples have I and Q components
        } else {
            rawData.resize(capacity * 2);
        }
    }
};

// Performance metrics for streaming
struct StreamMetrics {
    std::atomic<uint64_t> totalSamples{0};
    std::atomic<uint64_t> totalBytes{0};
    std::atomic<uint64_t> droppedBuffers{0};
    std::atomic<uint64_t> callbackTimeUs{0};
    std::atomic<uint64_t> callbackCount{0};
    
    std::chrono::steady_clock::time_point startTime;
    
    // Reset all metrics
    void reset() {
        totalSamples = 0;
        totalBytes = 0;
        droppedBuffers = 0;
        callbackTimeUs = 0;
        callbackCount = 0;
        startTime = std::chrono::steady_clock::now();
    }
    
    // Get current throughput in samples per second
    double getSampleRate() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsedSeconds = std::chrono::duration<double>(now - startTime).count();
        if (elapsedSeconds <= 0.0) return 0.0;
        return static_cast<double>(totalSamples) / elapsedSeconds;
    }
    
    // Get current throughput in bytes per second
    double getByteRate() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsedSeconds = std::chrono::duration<double>(now - startTime).count();
        if (elapsedSeconds <= 0.0) return 0.0;
        return static_cast<double>(totalBytes) / elapsedSeconds;
    }
    
    // Get average callback processing time in microseconds
    double getAvgCallbackTimeUs() const {
        uint64_t count = callbackCount.load();
        if (count == 0) return 0.0;
        return static_cast<double>(callbackTimeUs.load()) / count;
    }
};

// Static function to get available BB60C devices
std::vector<std::string> BB60CDevice::getDeviceList() {
    std::vector<std::string> deviceList;
    int deviceCount = 0;
    int status = bbGetDeviceCount(&deviceCount);
    
    if (status != bbNoError) {
        // Return empty list if error occurs
        return deviceList;
    }
    
    for (int i = 0; i < deviceCount; i++) {
        bb_handle_t tempHandle = nullptr;
        // Try to open the device to get its serial number
        status = bbOpenDevice(&tempHandle);
        
        if (status == bbNoError && tempHandle) {
            char serialStr[256];
            bbGetSerialNumber(tempHandle, serialStr);
            deviceList.push_back(serialStr);
            bbCloseDevice(tempHandle);
        }
    }
    
    return deviceList;
}

// Constructor
BB60CDevice::BB60CDevice()
    : handle_(nullptr)
    , isStreaming_(false)
    , shouldStopStreaming_(false)
    , bufferSize_(DEFAULT_BUFFER_SIZE)
    , metrics_(std::make_unique<StreamMetrics>())
{
    // Initialize default configuration
    currentConfig_ = IQConfig();
}

// Destructor
BB60CDevice::~BB60CDevice() {
    // Ensure the device is closed properly
    close();
}

// Move constructor
BB60CDevice::BB60CDevice(BB60CDevice&& other) noexcept
    : handle_(other.handle_)
    , isStreaming_(other.isStreaming_.load())
    , shouldStopStreaming_(other.shouldStopStreaming_.load())
    , currentConfig_(other.currentConfig_)
    , serialNumber_(std::move(other.serialNumber_))
    , dataCallback_(std::move(other.dataCallback_))
    , bufferSize_(other.bufferSize_)
    , metrics_(std::move(other.metrics_))
{
    // Reset other's handle to prevent double-close
    other.handle_ = nullptr;
    other.isStreaming_ = false;
    other.shouldStopStreaming_ = false;
}

// Move assignment operator
BB60CDevice& BB60CDevice::operator=(BB60CDevice&& other) noexcept {
    if (this != &other) {
        // Close current device if open
        close();
        
        // Move resources
        handle_ = other.handle_;
        isStreaming_ = other.isStreaming_.load();
        shouldStopStreaming_ = other.shouldStopStreaming_.load();
        currentConfig_ = other.currentConfig_;
        serialNumber_ = std::move(other.serialNumber_);
        dataCallback_ = std::move(other.dataCallback_);
        bufferSize_ = other.bufferSize_;
        metrics_ = std::move(other.metrics_);
        
        // Reset other's handle
        other.handle_ = nullptr;
        other.isStreaming_ = false;
        other.shouldStopStreaming_ = false;
    }
    return *this;
}

// Open the device
void BB60CDevice::open(const std::string& serialNumber) {
    // Lock for thread safety
    std::lock_guard<std::mutex> lock(deviceMutex_);
    
    // Check if already open
    if (handle_) {
        throw std::runtime_error("Device is already open");
    }
    
    int status;
    
    if (serialNumber.empty()) {
        // Open any available device
        status = bbOpenDevice(&handle_);
    } else {
        // Open specific device by serial number
        status = bbOpenDeviceBySerialNumber(&handle_, serialNumber.c_str());
    }
    
    checkStatus(status, "open");
    
    // Get and store the serial number
    char serialStr[256];
    status = bbGetSerialNumber(handle_, serialStr);
    checkStatus(status, "getSerialNumber");
    serialNumber_ = serialStr;
    
    // Initialize the device
    status = bbPreset(handle_);
    checkStatus(status, "preset");
}

// Close the device
void BB60CDevice::close() {
    // Lock for thread safety
    std::lock_guard<std::mutex> lock(deviceMutex_);
    
    // Check if open
    if (handle_) {
        // Stop streaming if active
        if (isStreaming_) {
            stopIQStreaming();
        }
        
        // Close the device
        bbCloseDevice(handle_);
        handle_ = nullptr;
        serialNumber_.clear();
    }
}

// Check if device is open
bool BB60CDevice::isOpen() const {
    return handle_ != nullptr;
}

// Get the device serial number
std::string BB60CDevice::getSerialNumber() const {
    if (!handle_) {
        throw std::runtime_error("Device is not open");
    }
    return serialNumber_;
}

// Get the device firmware version
std::string BB60CDevice::getFirmwareVersion() const {
    if (!handle_) {
        throw std::runtime_error("Device is not open");
    }
    
    char versionStr[256];
    int status = bbGetFirmwareVersion(handle_, versionStr);
    
    if (status != bbNoError) {
        throw std::runtime_error(std::string("Failed to get firmware version: ") + 
                                bbGetErrorString(status));
    }
    
    return versionStr;
}

// Synchronize device time with GPS
void BB60CDevice::syncWithGPS(const std::string& comPort, int baudRate) {
    std::lock_guard<std::mutex> lock(deviceMutex_);
    
    if (!handle_) {
        throw std::runtime_error("Device is not open");
    }
    
    // TODO: Implement GPS synchronization using the BB60C API
    // This will require interfacing with the GPS subsystem
    // For now, throw an unimplemented exception
    throw std::runtime_error("GPS synchronization not implemented yet");
}

// Configure device for I/Q streaming
void BB60CDevice::configureIQ(const IQConfig& config) {
    std::lock_guard<std::mutex> lock(deviceMutex_);
    
    if (!handle_) {
        throw std::runtime_error("Device is not open");
    }
    
    // Stop streaming if already active
    if (isStreaming_) {
        stopIQStreaming();
    }
    
    // Store the configuration
    currentConfig_ = config;
    
    // Calculate decimation and bandwidth
    int status = bbConfigureIQ(
        handle_,
        config.centerFreq,
        config.decimation,
        config.bandwidth,
        config.useFloat ? BB_TRUE : BB_FALSE
    );
    
    checkStatus(status, "configureIQ");
}

// Configure device I/O ports
void BB60CDevice::configureIO(int port1Mode, int port2Mode) {
    std::lock_guard<std::mutex> lock(deviceMutex_);
    
    if (!handle_) {
        throw std::runtime_error("Device is not open");
    }
    
    // Configure the device's I/O ports
    int status = bbConfigureIO(handle_, port1Mode, port2Mode);
    checkStatus(status, "configureIO");
}

// Set buffer size for I/Q streaming
void BB60CDevice::setBufferSize(size_t bufferSize) {
    // Ensure buffer size is reasonable
    if (bufferSize < 1024) {
        bufferSize = 1024;  // Minimum buffer size
    }
    if (bufferSize > BB60C_MAX_BUFFER_SIZE) {
        bufferSize = BB60C_MAX_BUFFER_SIZE;  // Maximum buffer size
    }
    
    bufferSize_ = bufferSize;
}

// Get streaming performance metrics
BB60CDevice::StreamingMetrics BB60CDevice::getStreamingMetrics() const {
    StreamingMetrics result;
    
    if (!metrics_) {
        return result;
    }
    
    result.sampleRate = metrics_->getSampleRate();
    result.dataRate = metrics_->getByteRate();
    result.droppedBuffers = metrics_->droppedBuffers.load();
    result.avgCallbackTime = metrics_->getAvgCallbackTimeUs();
    
    return result;
}

// Start I/Q streaming
void BB60CDevice::startIQStreaming(IQCallback callback, bool useTimeStamp) {
    std::lock_guard<std::mutex> lock(deviceMutex_);
    
    if (!handle_) {
        throw std::runtime_error("Device is not open");
    }
    
    if (isStreaming_) {
        throw std::runtime_error("I/Q streaming is already active");
    }
    
    // Store the callback
    dataCallback_ = callback;
    
    // Start the device in streaming mode
    int status = bbInitiate(handle_, BB_STREAMING, BB_STREAM_IQ);
    checkStatus(status, "startIQStreaming");
    
    // Reset streaming flags
    isStreaming_ = true;
    shouldStopStreaming_ = false;
    
    // Reset performance metrics
    metrics_->reset();
    
    // Start the streaming thread
    streamThread_ = std::thread(&BB60CDevice::streamingThread, this);
}

// Stop I/Q streaming
void BB60CDevice::stopIQStreaming() {
    // Set flag to stop streaming thread
    shouldStopStreaming_ = true;
    
    // Wait for streaming thread to finish
    if (streamThread_.joinable()) {
        streamThread_.join();
    }
    
    std::lock_guard<std::mutex> lock(deviceMutex_);
    
    if (!handle_) {
        return;
    }
    
    if (!isStreaming_) {
        return;
    }
    
    // Set the streaming flag
    isStreaming_ = false;
    
    // Stop the streaming
    int status = bbAbort(handle_);
    
    // No need to check status here, as we're stopping anyway
    
    // Clear the callback
    dataCallback_ = nullptr;
}

// Reset the device to default state
void BB60CDevice::reset() {
    std::lock_guard<std::mutex> lock(deviceMutex_);
    
    if (!handle_) {
        throw std::runtime_error("Device is not open");
    }
    
    // Stop streaming if active
    if (isStreaming_) {
        stopIQStreaming();
    }
    
    // Reset the device
    int status = bbPreset(handle_);
    checkStatus(status, "reset");
}

// Streaming thread implementation
void BB60CDevice::streamingThread() {
    // Create a circular buffer of I/Q buffers
    std::queue<std::shared_ptr<IQBuffer>> bufferQueue;
    
    // Create a pool of pre-allocated buffers
    std::vector<std::shared_ptr<IQBuffer>> bufferPool;
    for (int i = 0; i < MAX_BUFFER_COUNT; i++) {
        bufferPool.push_back(std::make_shared<IQBuffer>(
            bufferSize_, currentConfig_.useFloat));
    }
    
    // Main streaming loop
    while (!shouldStopStreaming_.load()) {
        try {
            // Get a buffer from the pool (or create a new one if needed)
            std::shared_ptr<IQBuffer> buffer;
            if (!bufferPool.empty()) {
                buffer = bufferPool.back();
                bufferPool.pop_back();
            } else {
                // If pool is exhausted, create a new buffer
                buffer = std::make_shared<IQBuffer>(
                    bufferSize_, currentConfig_.useFloat);
                
                // Track dropped buffers (pool exhaustion indicates callback is too slow)
                metrics_->droppedBuffers++;
            }
            
            // Get I/Q data from the device
            int returnLen = 0;
            int status;
            
            if (buffer->isFloat) {
                // Get floating-point data
                status = bbFetchRaw(
                    handle_,
                    buffer->data.data(),
                    buffer->data.size() / 2,  // Complex sample count
                    &returnLen
                );
            } else {
                // Get 16-bit integer data
                status = bbFetchRaw16(
                    handle_,
                    buffer->rawData.data(),
                    buffer->rawData.size() / 2,  // Complex sample count
                    &returnLen
                );
            }
            
            // Check for streaming errors
            if (status != bbNoError) {
                if (status == bbDeviceNotOpenErr) {
                    // Device was closed, exit thread
                    break;
                }
                
                std::cerr << "BB60C streaming error: " << bbGetErrorString(status) << std::endl;
                
                // Add buffer back to pool
                bufferPool.push_back(buffer);
                
                // Small delay before retrying
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            // Update buffer sample count (actual returned samples)
            buffer->sampleCount = returnLen;
            
            // Get current timestamp
            buffer->timestamp = 0.0;  // TODO: Implement actual GPS timestamping
            
            // Update metrics
            metrics_->totalSamples += returnLen;
            metrics_->totalBytes += returnLen * (buffer->isFloat ? sizeof(float) : sizeof(int16_t)) * 2;
            
            // Process the buffer with the callback
            if (dataCallback_) {
                auto startTime = std::chrono::high_resolution_clock::now();
                
                if (buffer->isFloat) {
                    dataCallback_(buffer->data.data(), buffer->sampleCount, buffer->timestamp);
                } else {
                    dataCallback_(buffer->rawData.data(), buffer->sampleCount, buffer->timestamp);
                }
                
                auto endTime = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                    endTime - startTime).count();
                
                metrics_->callbackTimeUs += duration;
                metrics_->callbackCount++;
            }
            
            // Return buffer to the pool
            bufferPool.push_back(buffer);
            
        } catch (const std::exception& e) {
            std::cerr << "Exception in streaming thread: " << e.what() << std::endl;
            
            // Small delay before retrying
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    // Clean up
    bufferPool.clear();
    
    // Signal that streaming is finished
    isStreaming_ = false;
}

// Check API status and throw exception if error
void BB60CDevice::checkStatus(int status, const char* functionName) {
    if (status != bbNoError) {
        std::string errorMsg = std::string("BB60C error in ") + functionName + 
                               ": " + bbGetErrorString(status);
        throw std::runtime_error(errorMsg);
    }
}

} // namespace devices
} // namespace tdoa 