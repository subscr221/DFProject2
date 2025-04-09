/**
 * @file signal.cpp
 * @brief Implementation of the Signal class
 */

#include "signal.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>

namespace tdoa {
namespace signal {

// Utility function to calculate buffer size based on format and sample count
static size_t calculateBufferSize(DataFormat format, size_t sampleCount) {
    switch (format) {
        case DataFormat::ComplexFloat32:
            return sampleCount * sizeof(std::complex<float>);
        case DataFormat::ComplexInt16:
            return sampleCount * sizeof(std::complex<int16_t>);
        case DataFormat::ComplexInt8:
            return sampleCount * sizeof(std::complex<int8_t>);
        case DataFormat::Raw:
            return sampleCount; // For raw format, sample count is byte count
        default:
            throw std::runtime_error("Unknown data format");
    }
}

// Constructor with pre-allocated buffer
Signal::Signal(DataFormat format, size_t sampleCount)
    : format_(format)
    , sampleCount_(sampleCount)
    , bufferSize_(calculateBufferSize(format, sampleCount))
    , dataBuffer_(bufferSize_)
    , centerFrequency_(0.0)
    , sampleRate_(0.0)
    , bandwidth_(0.0)
    , timestamp_(0.0)
{
    // Zero-initialize the buffer
    std::fill(dataBuffer_.begin(), dataBuffer_.end(), 0);
}

// Constructor with existing data
Signal::Signal(const void* data, size_t dataSize, DataFormat format, size_t sampleCount)
    : format_(format)
    , sampleCount_(sampleCount)
    , bufferSize_(calculateBufferSize(format, sampleCount))
    , dataBuffer_(bufferSize_)
    , centerFrequency_(0.0)
    , sampleRate_(0.0)
    , bandwidth_(0.0)
    , timestamp_(0.0)
{
    // Check if dataSize matches the expected buffer size
    if (dataSize != bufferSize_) {
        throw std::runtime_error("Data size does not match expected buffer size for the given format and sample count");
    }
    
    // Copy the data
    std::memcpy(dataBuffer_.data(), data, dataSize);
}

// Destructor
Signal::~Signal() {
    // Vector destructor will handle cleanup
}

// Get raw data pointer
void* Signal::data() {
    return dataBuffer_.data();
}

// Get raw data pointer (const version)
const void* Signal::data() const {
    return dataBuffer_.data();
}

// Get data as complex float samples
std::complex<float>* Signal::complexFloat() {
    if (format_ != DataFormat::ComplexFloat32) {
        return nullptr;
    }
    return reinterpret_cast<std::complex<float>*>(dataBuffer_.data());
}

// Get data as complex float samples (const version)
const std::complex<float>* Signal::complexFloat() const {
    if (format_ != DataFormat::ComplexFloat32) {
        return nullptr;
    }
    return reinterpret_cast<const std::complex<float>*>(dataBuffer_.data());
}

// Get data as complex int16 samples
std::complex<int16_t>* Signal::complexInt16() {
    if (format_ != DataFormat::ComplexInt16) {
        return nullptr;
    }
    return reinterpret_cast<std::complex<int16_t>*>(dataBuffer_.data());
}

// Get data as complex int16 samples (const version)
const std::complex<int16_t>* Signal::complexInt16() const {
    if (format_ != DataFormat::ComplexInt16) {
        return nullptr;
    }
    return reinterpret_cast<const std::complex<int16_t>*>(dataBuffer_.data());
}

// Get data as complex int8 samples
std::complex<int8_t>* Signal::complexInt8() {
    if (format_ != DataFormat::ComplexInt8) {
        return nullptr;
    }
    return reinterpret_cast<std::complex<int8_t>*>(dataBuffer_.data());
}

// Get data as complex int8 samples (const version)
const std::complex<int8_t>* Signal::complexInt8() const {
    if (format_ != DataFormat::ComplexInt8) {
        return nullptr;
    }
    return reinterpret_cast<const std::complex<int8_t>*>(dataBuffer_.data());
}

// Convert signal to a different data format
std::shared_ptr<Signal> Signal::convertToFormat(DataFormat targetFormat) const {
    // If already in the target format, just clone
    if (format_ == targetFormat) {
        return clone();
    }
    
    // Create a new signal with the target format
    auto result = std::make_shared<Signal>(targetFormat, sampleCount_);
    
    // Copy metadata
    result->setCenterFrequency(centerFrequency_);
    result->setSampleRate(sampleRate_);
    result->setBandwidth(bandwidth_);
    result->setTimestamp(timestamp_);
    result->setSourceInfo(sourceInfo_);
    result->setId(id_);
    
    // Copy all metadata
    for (const auto& pair : metadata_) {
        result->setMetadata(pair.first, pair.second);
    }
    
    // Convert the data based on source and target formats
    // This is a simplified implementation - in real code you'd need more robust conversion
    
    // Source is ComplexFloat32
    if (format_ == DataFormat::ComplexFloat32) {
        const auto* srcData = complexFloat();
        
        if (targetFormat == DataFormat::ComplexInt16) {
            auto* dstData = result->complexInt16();
            for (size_t i = 0; i < sampleCount_; i++) {
                // Scale float [-1.0, 1.0] to int16 range
                float real = std::max(-1.0f, std::min(1.0f, srcData[i].real()));
                float imag = std::max(-1.0f, std::min(1.0f, srcData[i].imag()));
                dstData[i] = std::complex<int16_t>(
                    static_cast<int16_t>(real * 32767.0f),
                    static_cast<int16_t>(imag * 32767.0f)
                );
            }
        }
        else if (targetFormat == DataFormat::ComplexInt8) {
            auto* dstData = result->complexInt8();
            for (size_t i = 0; i < sampleCount_; i++) {
                // Scale float [-1.0, 1.0] to int8 range
                float real = std::max(-1.0f, std::min(1.0f, srcData[i].real()));
                float imag = std::max(-1.0f, std::min(1.0f, srcData[i].imag()));
                dstData[i] = std::complex<int8_t>(
                    static_cast<int8_t>(real * 127.0f),
                    static_cast<int8_t>(imag * 127.0f)
                );
            }
        }
        else if (targetFormat == DataFormat::Raw) {
            // For raw format, just copy the bytes
            std::memcpy(result->data(), data(), bufferSize_);
        }
    }
    // Source is ComplexInt16
    else if (format_ == DataFormat::ComplexInt16) {
        const auto* srcData = complexInt16();
        
        if (targetFormat == DataFormat::ComplexFloat32) {
            auto* dstData = result->complexFloat();
            for (size_t i = 0; i < sampleCount_; i++) {
                // Scale int16 to float [-1.0, 1.0]
                dstData[i] = std::complex<float>(
                    static_cast<float>(srcData[i].real()) / 32767.0f,
                    static_cast<float>(srcData[i].imag()) / 32767.0f
                );
            }
        }
        else if (targetFormat == DataFormat::ComplexInt8) {
            auto* dstData = result->complexInt8();
            for (size_t i = 0; i < sampleCount_; i++) {
                // Scale int16 to int8 (with potential quality loss)
                dstData[i] = std::complex<int8_t>(
                    static_cast<int8_t>(srcData[i].real() >> 8),
                    static_cast<int8_t>(srcData[i].imag() >> 8)
                );
            }
        }
        else if (targetFormat == DataFormat::Raw) {
            // For raw format, just copy the bytes
            std::memcpy(result->data(), data(), bufferSize_);
        }
    }
    // Source is ComplexInt8
    else if (format_ == DataFormat::ComplexInt8) {
        const auto* srcData = complexInt8();
        
        if (targetFormat == DataFormat::ComplexFloat32) {
            auto* dstData = result->complexFloat();
            for (size_t i = 0; i < sampleCount_; i++) {
                // Scale int8 to float [-1.0, 1.0]
                dstData[i] = std::complex<float>(
                    static_cast<float>(srcData[i].real()) / 127.0f,
                    static_cast<float>(srcData[i].imag()) / 127.0f
                );
            }
        }
        else if (targetFormat == DataFormat::ComplexInt16) {
            auto* dstData = result->complexInt16();
            for (size_t i = 0; i < sampleCount_; i++) {
                // Scale int8 to int16
                dstData[i] = std::complex<int16_t>(
                    static_cast<int16_t>(srcData[i].real()) << 8,
                    static_cast<int16_t>(srcData[i].imag()) << 8
                );
            }
        }
        else if (targetFormat == DataFormat::Raw) {
            // For raw format, just copy the bytes
            std::memcpy(result->data(), data(), bufferSize_);
        }
    }
    // Source is Raw
    else if (format_ == DataFormat::Raw) {
        // For raw source, just copy the bytes
        std::memcpy(result->data(), data(), bufferSize_);
        
        // Set a metadata flag indicating this was converted from raw
        result->setMetadata("converted_from_raw", "true");
    }
    
    return result;
}

// Create a slice of this signal (shallow copy with offset)
std::shared_ptr<Signal> Signal::slice(size_t startSample, size_t sliceSampleCount) const {
    // Check bounds
    if (startSample >= sampleCount_ || startSample + sliceSampleCount > sampleCount_) {
        throw std::out_of_range("Slice range is out of bounds");
    }
    
    // Create a new signal
    auto result = std::make_shared<Signal>(format_, sliceSampleCount);
    
    // Copy metadata
    result->setCenterFrequency(centerFrequency_);
    result->setSampleRate(sampleRate_);
    result->setBandwidth(bandwidth_);
    result->setSourceInfo(sourceInfo_);
    
    // Adjust timestamp based on the start sample offset
    double timeOffset = (sampleRate_ > 0.0) ? (static_cast<double>(startSample) / sampleRate_) : 0.0;
    result->setTimestamp(timestamp_ + timeOffset);
    
    // Set ID with slice info
    result->setId(id_ + "_slice_" + std::to_string(startSample) + "_" + std::to_string(sliceSampleCount));
    
    // Copy all metadata
    for (const auto& pair : metadata_) {
        result->setMetadata(pair.first, pair.second);
    }
    
    // Set slice metadata
    result->setMetadata("slice_start", std::to_string(startSample));
    result->setMetadata("slice_count", std::to_string(sliceSampleCount));
    result->setMetadata("original_id", id_);
    
    // Copy the data slice
    size_t bytesPerSample = 0;
    switch (format_) {
        case DataFormat::ComplexFloat32:
            bytesPerSample = sizeof(std::complex<float>);
            break;
        case DataFormat::ComplexInt16:
            bytesPerSample = sizeof(std::complex<int16_t>);
            break;
        case DataFormat::ComplexInt8:
            bytesPerSample = sizeof(std::complex<int8_t>);
            break;
        case DataFormat::Raw:
            bytesPerSample = 1; // For raw format, 1 byte per sample
            break;
    }
    
    size_t byteOffset = startSample * bytesPerSample;
    size_t byteCount = sliceSampleCount * bytesPerSample;
    
    std::memcpy(result->data(), static_cast<const uint8_t*>(data()) + byteOffset, byteCount);
    
    return result;
}

// Clone this signal (deep copy)
std::shared_ptr<Signal> Signal::clone() const {
    // Create a new signal with the same format and sample count
    auto result = std::make_shared<Signal>(format_, sampleCount_);
    
    // Copy metadata
    result->setCenterFrequency(centerFrequency_);
    result->setSampleRate(sampleRate_);
    result->setBandwidth(bandwidth_);
    result->setTimestamp(timestamp_);
    result->setSourceInfo(sourceInfo_);
    result->setId(id_ + "_clone");
    
    // Copy all metadata
    for (const auto& pair : metadata_) {
        result->setMetadata(pair.first, pair.second);
    }
    
    // Copy the data
    std::memcpy(result->data(), data(), bufferSize_);
    
    return result;
}

// Get a reference to a sample by index (for ComplexFloat32 format only)
std::complex<float>& Signal::sampleAt(size_t index) {
    if (format_ != DataFormat::ComplexFloat32) {
        throw std::runtime_error("Signal format is not ComplexFloat32");
    }
    
    if (index >= sampleCount_) {
        throw std::out_of_range("Sample index out of range");
    }
    
    return complexFloat()[index];
}

// Get a const reference to a sample by index (for ComplexFloat32 format only)
const std::complex<float>& Signal::sampleAt(size_t index) const {
    if (format_ != DataFormat::ComplexFloat32) {
        throw std::runtime_error("Signal format is not ComplexFloat32");
    }
    
    if (index >= sampleCount_) {
        throw std::out_of_range("Sample index out of range");
    }
    
    return complexFloat()[index];
}

} // namespace signal
} // namespace tdoa 