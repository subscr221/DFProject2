/**
 * @file gps_time_sync.h
 * @brief GPS-based time synchronization implementation
 */

#pragma once

#include "time_sync_interface.h"
#include "../devices/gps/gps_device.h"
#include <mutex>
#include <thread>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <vector>

namespace tdoa {
namespace time_sync {

/**
 * @class GPSTimeSync
 * @brief Implementation of time synchronization using GPS receivers
 * 
 * This class provides precise time synchronization between nodes using
 * GPS receivers with PPS (Pulse Per Second) signals. It handles 
 * timing calibration, stability measurement, and compensation for
 * hardware delays.
 */
class GPSTimeSync : public TimeSync {
public:
    /**
     * @brief Constructor
     */
    GPSTimeSync();
    
    /**
     * @brief Destructor
     */
    ~GPSTimeSync() override;
    
    /**
     * @brief Initialize time synchronization
     * @param devicePath Path to the GPS device (e.g., serial port)
     * @return True if initialization successful
     */
    bool initialize(const std::string& devicePath) override;
    
    /**
     * @brief Start time synchronization
     * @return True if started successfully
     */
    bool start() override;
    
    /**
     * @brief Stop time synchronization
     * @return True if stopped successfully
     */
    bool stop() override;
    
    /**
     * @brief Get current time reference
     * @return Current time reference information
     */
    TimeReference getTimeReference() const override;
    
    /**
     * @brief Get synchronization statistics
     * @return Current synchronization statistics
     */
    SyncStatistics getStatistics() const override;
    
    /**
     * @brief Get synchronization status
     * @return Current synchronization status
     */
    SyncStatus getStatus() const override;
    
    /**
     * @brief Register callback for synchronization events
     * @param callback Function to call on synchronization events
     */
    void registerEventCallback(SyncEventCallback callback) override;
    
    /**
     * @brief Get a precise timestamp with nanosecond resolution
     * @return High precision timestamp in nanoseconds since epoch
     */
    uint64_t getPreciseTimestamp() const override;
    
    /**
     * @brief Calculate time difference between two nodes
     * @param localTime Local time reference
     * @param remoteTime Remote time reference
     * @return Time difference in nanoseconds (positive if remote is ahead)
     */
    double calculateTimeDifference(
        const TimeReference& localTime, 
        const TimeReference& remoteTime) const override;
    
    /**
     * @brief Calibrate time synchronization
     * @param offsetNanoseconds Known offset in nanoseconds to apply
     * @return True if calibration was successful
     */
    bool calibrate(double offsetNanoseconds) override;
    
    /**
     * @brief Configure temperature compensation
     * @param enable Enable/disable temperature compensation
     * @param coefficient Temperature coefficient in ppb/°C
     * @return True if configuration successful
     */
    bool configureTemperatureCompensation(bool enable, double coefficient = 0.0);
    
    /**
     * @brief Configure fallback behavior during GPS signal loss
     * @param maxHoldoverTime Maximum time in seconds to operate in holdover mode
     * @param driftThreshold Maximum acceptable drift in ppb before going to error state
     * @return True if configuration successful
     */
    bool configureHoldover(double maxHoldoverTime, double driftThreshold);
    
    /**
     * @brief Set calibration values for systematic delays
     * @param antennaDelay Antenna delay in nanoseconds
     * @param cableDelay Cable delay in nanoseconds
     * @param receiverDelay Receiver processing delay in nanoseconds
     * @return True if calibration values were set successfully
     */
    bool setDelayCalibration(double antennaDelay, double cableDelay, double receiverDelay);

private:
    // Forward declaration of implementation class (PIMPL idiom)
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace time_sync
} // namespace tdoa 