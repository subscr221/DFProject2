/**
 * @file time_sync_interface.h
 * @brief Interfaces for precise time synchronization between nodes using GPS
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>

namespace tdoa {
namespace time_sync {

/**
 * @brief Time synchronization status
 */
enum class SyncStatus {
    Unknown,        ///< Status unknown or not initialized
    Unsynchronized, ///< Not synchronized to any time reference
    Acquiring,      ///< Acquiring synchronization
    Synchronized,   ///< Synchronized to time reference
    Holdover,       ///< In holdover mode (was synchronized, but reference lost)
    Error           ///< Error in time synchronization
};

/**
 * @brief Synchronization source type
 */
enum class SyncSource {
    None,           ///< No synchronization source
    GPS,            ///< GPS-based synchronization
    PTP,            ///< Precision Time Protocol (PTP/IEEE 1588)
    NTP,            ///< Network Time Protocol
    Manual,         ///< Manual time setting
    LocalOscillator ///< Local high-stability oscillator
};

/**
 * @brief Time reference information
 */
struct TimeReference {
    std::chrono::system_clock::time_point timestamp; ///< System timestamp
    uint64_t nanoseconds;                           ///< Nanoseconds since epoch
    double uncertainty;                             ///< Uncertainty in nanoseconds
    SyncSource source;                              ///< Source of time reference
    SyncStatus status;                              ///< Synchronization status
    
    TimeReference() 
        : nanoseconds(0)
        , uncertainty(1000000.0)  // 1ms default uncertainty
        , source(SyncSource::None)
        , status(SyncStatus::Unknown)
    {}
};

/**
 * @brief Statistics for time synchronization
 */
struct SyncStatistics {
    double allanDeviation;          ///< Allan deviation in parts per billion (ppb)
    double offsetFromReference;     ///< Offset from reference in nanoseconds
    double driftRate;               ///< Drift rate in ppb
    double temperatureCoefficient;  ///< Temperature coefficient in ppb/°C
    uint32_t ppsCount;              ///< Count of PPS signals received
    uint32_t missedPps;             ///< Count of missed PPS signals
    double lastSyncDuration;        ///< Duration since last synchronization in seconds
    
    SyncStatistics()
        : allanDeviation(0.0)
        , offsetFromReference(0.0)
        , driftRate(0.0)
        , temperatureCoefficient(0.0)
        , ppsCount(0)
        , missedPps(0)
        , lastSyncDuration(0.0)
    {}
};

/**
 * @brief Callback for time synchronization events
 */
using SyncEventCallback = std::function<void(const TimeReference&, const std::string&)>;

/**
 * @class TimeSync
 * @brief Interface for time synchronization components
 */
class TimeSync {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~TimeSync() = default;
    
    /**
     * @brief Initialize time synchronization
     * @param devicePath Path to the device (e.g., serial port)
     * @return True if initialization successful
     */
    virtual bool initialize(const std::string& devicePath) = 0;
    
    /**
     * @brief Start time synchronization
     * @return True if started successfully
     */
    virtual bool start() = 0;
    
    /**
     * @brief Stop time synchronization
     * @return True if stopped successfully
     */
    virtual bool stop() = 0;
    
    /**
     * @brief Get current time reference
     * @return Current time reference information
     */
    virtual TimeReference getTimeReference() const = 0;
    
    /**
     * @brief Get synchronization statistics
     * @return Current synchronization statistics
     */
    virtual SyncStatistics getStatistics() const = 0;
    
    /**
     * @brief Get synchronization status
     * @return Current synchronization status
     */
    virtual SyncStatus getStatus() const = 0;
    
    /**
     * @brief Register callback for synchronization events
     * @param callback Function to call on synchronization events
     */
    virtual void registerEventCallback(SyncEventCallback callback) = 0;
    
    /**
     * @brief Get a precise timestamp with nanosecond resolution
     * @return High precision timestamp in nanoseconds since epoch
     */
    virtual uint64_t getPreciseTimestamp() const = 0;
    
    /**
     * @brief Calculate time difference between two nodes
     * @param localTime Local time reference
     * @param remoteTime Remote time reference
     * @return Time difference in nanoseconds (positive if remote is ahead)
     */
    virtual double calculateTimeDifference(
        const TimeReference& localTime, 
        const TimeReference& remoteTime) const = 0;
    
    /**
     * @brief Calibrate time synchronization
     * @param offsetNanoseconds Known offset in nanoseconds to apply
     * @return True if calibration was successful
     */
    virtual bool calibrate(double offsetNanoseconds) = 0;
};

/**
 * @brief Factory function to create time synchronization instance
 * @param sourceType Synchronization source type
 * @return Unique pointer to time synchronization interface
 */
std::unique_ptr<TimeSync> createTimeSync(SyncSource sourceType);

} // namespace time_sync
} // namespace tdoa 