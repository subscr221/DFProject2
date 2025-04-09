/**
 * @file signal.h
 * @brief Base Signal class for representing I/Q data with metadata
 */

#pragma once

#include <vector>
#include <complex>
#include <memory>
#include <string>
#include <map>
#include <chrono>
#include <functional>
#include <iostream>

namespace tdoa {
namespace signal {

/**
 * @brief Signal data format enumeration
 */
enum class DataFormat {
    ComplexFloat32,    ///< std::complex<float> (8 bytes per sample)
    ComplexInt16,      ///< std::complex<int16_t> (4 bytes per sample)
    ComplexInt8,       ///< std::complex<int8_t> (2 bytes per sample)
    Raw                ///< Raw byte data (format specified in metadata)
};

/**
 * @brief Signal source information
 */
struct SourceInfo {
    std::string deviceType;     ///< Type of device that produced the signal
    std::string deviceId;       ///< Unique identifier for the source device
    std::string locationId;     ///< Identifier for the location of the device
    double latitude;            ///< Latitude of the device in degrees
    double longitude;           ///< Longitude of the device in degrees
    double altitude;            ///< Altitude of the device in meters
    
    /**
     * @brief Default constructor with empty values
     */
    SourceInfo()
        : deviceType("")
        , deviceId("")
        , locationId("")
        , latitude(0.0)
        , longitude(0.0)
        , altitude(0.0)
    {}
};

/**
 * @brief Base class for signal samples with metadata
 * 
 * This class represents a chunk of signal data with associated metadata,
 * such as timestamp, frequency, and other parameters. It supports different
 * data formats and provides methods for accessing and manipulating the data.
 */
class Signal {
public:
    /**
     * @brief Create a signal with pre-allocated buffer
     * @param format Data format
     * @param sampleCount Number of complex samples to allocate
     */
    Signal(DataFormat format, size_t sampleCount);
    
    /**
     * @brief Create a signal from existing data
     * @param data Raw data buffer (will be copied)
     * @param dataSize Size of data buffer in bytes
     * @param format Data format
     * @param sampleCount Number of complex samples in the data
     */
    Signal(const void* data, size_t dataSize, DataFormat format, size_t sampleCount);
    
    /**
     * @brief Destructor
     */
    virtual ~Signal();
    
    /**
     * @brief Get raw data pointer
     * @return Pointer to raw data
     */
    void* data();
    
    /**
     * @brief Get raw data pointer (const version)
     * @return Const pointer to raw data
     */
    const void* data() const;
    
    /**
     * @brief Get data as complex float samples
     * @return Pointer to complex float samples or nullptr if format doesn't match
     */
    std::complex<float>* complexFloat();
    
    /**
     * @brief Get data as complex float samples (const version)
     * @return Const pointer to complex float samples or nullptr if format doesn't match
     */
    const std::complex<float>* complexFloat() const;
    
    /**
     * @brief Get data as complex int16 samples
     * @return Pointer to complex int16 samples or nullptr if format doesn't match
     */
    std::complex<int16_t>* complexInt16();
    
    /**
     * @brief Get data as complex int16 samples (const version)
     * @return Const pointer to complex int16 samples or nullptr if format doesn't match
     */
    const std::complex<int16_t>* complexInt16() const;
    
    /**
     * @brief Get data as complex int8 samples
     * @return Pointer to complex int8 samples or nullptr if format doesn't match
     */
    std::complex<int8_t>* complexInt8();
    
    /**
     * @brief Get data as complex int8 samples (const version)
     * @return Const pointer to complex int8 samples or nullptr if format doesn't match
     */
    const std::complex<int8_t>* complexInt8() const;
    
    /**
     * @brief Convert signal to a different data format
     * @param format Target format
     * @return New signal with converted data
     */
    std::shared_ptr<Signal> convertToFormat(DataFormat format) const;
    
    /**
     * @brief Get the data format
     * @return Data format enumeration
     */
    DataFormat getFormat() const { return format_; }
    
    /**
     * @brief Get the number of complex samples
     * @return Sample count
     */
    size_t getSampleCount() const { return sampleCount_; }
    
    /**
     * @brief Get the size of the data buffer in bytes
     * @return Buffer size in bytes
     */
    size_t getBufferSize() const { return bufferSize_; }
    
    /**
     * @brief Get the center frequency
     * @return Center frequency in Hz
     */
    double getCenterFrequency() const { return centerFrequency_; }
    
    /**
     * @brief Set the center frequency
     * @param frequency Center frequency in Hz
     */
    void setCenterFrequency(double frequency) { centerFrequency_ = frequency; }
    
    /**
     * @brief Get the sample rate
     * @return Sample rate in samples per second
     */
    double getSampleRate() const { return sampleRate_; }
    
    /**
     * @brief Set the sample rate
     * @param rate Sample rate in samples per second
     */
    void setSampleRate(double rate) { sampleRate_ = rate; }
    
    /**
     * @brief Get the bandwidth
     * @return Bandwidth in Hz
     */
    double getBandwidth() const { return bandwidth_; }
    
    /**
     * @brief Set the bandwidth
     * @param bandwidth Bandwidth in Hz
     */
    void setBandwidth(double bandwidth) { bandwidth_ = bandwidth; }
    
    /**
     * @brief Get the timestamp
     * @return Timestamp in seconds since epoch
     */
    double getTimestamp() const { return timestamp_; }
    
    /**
     * @brief Set the timestamp
     * @param timestamp Timestamp in seconds since epoch
     */
    void setTimestamp(double timestamp) { timestamp_ = timestamp; }
    
    /**
     * @brief Get the signal source information
     * @return Source information structure
     */
    const SourceInfo& getSourceInfo() const { return sourceInfo_; }
    
    /**
     * @brief Set the signal source information
     * @param info Source information structure
     */
    void setSourceInfo(const SourceInfo& info) { sourceInfo_ = info; }
    
    /**
     * @brief Get the signal ID
     * @return Signal ID string
     */
    const std::string& getId() const { return id_; }
    
    /**
     * @brief Set the signal ID
     * @param id Signal ID string
     */
    void setId(const std::string& id) { id_ = id; }
    
    /**
     * @brief Set a metadata value
     * @param key Metadata key
     * @param value Metadata value
     */
    void setMetadata(const std::string& key, const std::string& value) {
        metadata_[key] = value;
    }
    
    /**
     * @brief Get a metadata value
     * @param key Metadata key
     * @return Metadata value or empty string if not found
     */
    std::string getMetadata(const std::string& key) const {
        auto it = metadata_.find(key);
        if (it != metadata_.end()) {
            return it->second;
        }
        return "";
    }
    
    /**
     * @brief Check if metadata key exists
     * @param key Metadata key
     * @return True if key exists
     */
    bool hasMetadata(const std::string& key) const {
        return metadata_.find(key) != metadata_.end();
    }
    
    /**
     * @brief Get all metadata
     * @return Metadata map
     */
    const std::map<std::string, std::string>& getMetadata() const {
        return metadata_;
    }
    
    /**
     * @brief Create a slice of this signal (shallow copy)
     * @param startSample Starting sample index
     * @param sampleCount Number of samples in the slice
     * @return New signal object sharing the same data buffer
     */
    std::shared_ptr<Signal> slice(size_t startSample, size_t sampleCount) const;
    
    /**
     * @brief Clone this signal (deep copy)
     * @return New signal object with copied data
     */
    std::shared_ptr<Signal> clone() const;
    
    /**
     * @brief Calculate the duration of the signal
     * @return Duration in seconds
     */
    double getDuration() const {
        return sampleRate_ > 0.0 ? static_cast<double>(sampleCount_) / sampleRate_ : 0.0;
    }
    
    /**
     * @brief Get a reference to a sample by index (for ComplexFloat32 format only)
     * @param index Sample index
     * @return Reference to sample
     * @throws std::runtime_error if format is not ComplexFloat32
     */
    std::complex<float>& sampleAt(size_t index);
    
    /**
     * @brief Get a const reference to a sample by index (for ComplexFloat32 format only)
     * @param index Sample index
     * @return Const reference to sample
     * @throws std::runtime_error if format is not ComplexFloat32
     */
    const std::complex<float>& sampleAt(size_t index) const;
    
private:
    DataFormat format_;               ///< Data format
    size_t sampleCount_;              ///< Number of complex samples
    size_t bufferSize_;               ///< Size of the data buffer in bytes
    std::vector<uint8_t> dataBuffer_; ///< Data buffer
    
    double centerFrequency_;          ///< Center frequency in Hz
    double sampleRate_;               ///< Sample rate in samples per second
    double bandwidth_;                ///< Bandwidth in Hz
    double timestamp_;                ///< Timestamp in seconds since epoch
    
    SourceInfo sourceInfo_;           ///< Signal source information
    std::string id_;                  ///< Signal ID
    std::map<std::string, std::string> metadata_; ///< Additional metadata
};

/**
 * @brief Callback function type for signal processing
 */
using SignalCallback = std::function<void(std::shared_ptr<Signal>)>;

} // namespace signal
} // namespace tdoa 