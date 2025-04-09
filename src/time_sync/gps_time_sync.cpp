/**
 * @file gps_time_sync.cpp
 * @brief Implementation of GPS-based time synchronization
 */

#include "gps_time_sync.h"
#include "kalman_filter.h"
#include "allan_variance.h"
#include "temperature_compensation.h"
#include "../devices/gps/gps_device.h"
#include <iostream>
#include <cmath>
#include <deque>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <ctime>

namespace tdoa {
namespace time_sync {

/**
 * @class GPSTimeSync::Impl
 * @brief Implementation details for GPSTimeSync
 */
class GPSTimeSync::Impl {
public:
    /**
     * @brief Constructor
     */
    Impl() 
        : status_(SyncStatus::Unknown)
        , gpsDevice_(nullptr)
        , running_(false)
        , ppsCaptureLatency_(0.0)
        , antennaDelay_(0.0)
        , cableDelay_(0.0)
        , receiverDelay_(0.0)
        , maxHoldoverTime_(60.0)  // Default 60 second holdover
        , driftThreshold_(500.0)  // Default 500 ppb drift threshold
        , temperatureCompensationEnabled_(false)
        , temperatureCoefficient_(0.0)
        , timeOffset_(0)
        , timeUncertainty_(1000000.0)  // 1ms initial uncertainty
        , measurementCount_(0)
        , kalmanFilter_(1.0e-10, 1.0e-6)  // Initial process and measurement noise
        , allanVariance_(1024)  // Store up to 1024 samples
        , temperatureCompensation_(-0.2)  // Default temp coefficient
        , currentTemperature_(25.0)  // Default temperature
    {
    }
    
    /**
     * @brief Destructor
     */
    ~Impl() {
        stop();
    }
    
    /**
     * @brief Initialize time synchronization
     * @param devicePath Path to the GPS device
     * @return True if initialization successful
     */
    bool initialize(const std::string& devicePath) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Parse device type and path
        size_t colonPos = devicePath.find(':');
        std::string deviceType = "GPSD";  // Default type
        std::string actualPath = devicePath;
        
        if (colonPos != std::string::npos) {
            deviceType = devicePath.substr(0, colonPos);
            actualPath = devicePath.substr(colonPos + 1);
        }
        
        try {
            // Create GPS device
            gpsDevice_ = devices::createGPSDevice(deviceType);
            
            // Set up callbacks
            gpsDevice_->registerDataCallback([this](const devices::GPSData& data) {
                this->handleGPSData(data);
            });
            
            gpsDevice_->registerPPSCallback([this](uint64_t timestamp) {
                this->handlePPS(timestamp);
            });
            
            // Open device
            if (!gpsDevice_->open(actualPath)) {
                std::cerr << "Failed to open GPS device" << std::endl;
                return false;
            }
            
            // Reset Kalman filter
            kalmanFilter_.reset();
            
            // Reset Allan variance calculator
            allanVariance_.reset();
            
            // Initialize temperature compensation
            temperatureCompensation_.setEnabled(temperatureCompensationEnabled_);
            temperatureCompensation_.setCoefficient(temperatureCoefficient_);
            
            status_ = SyncStatus::Unsynchronized;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error initializing GPS time sync: " << e.what() << std::endl;
            status_ = SyncStatus::Error;
            return false;
        }
    }
    
    /**
     * @brief Start time synchronization
     * @return True if started successfully
     */
    bool start() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!gpsDevice_) {
            std::cerr << "GPS device not initialized" << std::endl;
            return false;
        }
        
        if (running_) {
            // Already running
            return true;
        }
        
        running_ = true;
        status_ = SyncStatus::Acquiring;
        
        // Clear measurement history
        timeOffsets_.clear();
        measurementCount_ = 0;
        
        // Reset Kalman filter
        kalmanFilter_.reset();
        
        // Start measurement thread
        measurementThread_ = std::thread(&Impl::measurementThreadFunc, this);
        
        // Initialize calibration data
        lastPpsTime_ = 0;
        timeUncertainty_ = 1000000.0;  // 1ms initial uncertainty
        
        return true;
    }
    
    /**
     * @brief Stop time synchronization
     * @return True if stopped successfully
     */
    bool stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            if (!running_) {
                return true;
            }
            
            running_ = false;
        }
        
        // Wait for measurement thread to exit
        if (measurementThread_.joinable()) {
            measurementThread_.join();
        }
        
        // Close GPS device
        if (gpsDevice_) {
            gpsDevice_->close();
        }
        
        status_ = SyncStatus::Unsynchronized;
        return true;
    }
    
    /**
     * @brief Get current time reference
     * @return Current time reference information
     */
    TimeReference getTimeReference() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        TimeReference ref;
        ref.timestamp = std::chrono::system_clock::now();
        ref.nanoseconds = getPreciseTimestamp();
        ref.uncertainty = timeUncertainty_;
        ref.source = SyncSource::GPS;
        ref.status = status_;
        
        return ref;
    }
    
    /**
     * @brief Get synchronization statistics
     * @return Current synchronization statistics
     */
    SyncStatistics getStatistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        SyncStatistics stats;
        
        // Get Allan deviation from calculator
        if (allanVariance_.getSampleCount() >= 3) {
            stats.allanDeviation = allanVariance_.calculateDeviation(1.0); // 1-second tau
        }
        
        // Get drift rate from Kalman filter
        stats.driftRate = kalmanFilter_.getDrift();
        
        // Get offset from Kalman filter
        stats.offsetFromReference = kalmanFilter_.getOffset();
        
        // Get temperature coefficient
        stats.temperatureCoefficient = temperatureCompensation_.getCoefficient();
        
        // Set measurement counts
        stats.ppsCount = measurementCount_;
        stats.missedPps = calculateMissedPps();
        
        // Calculate time since last sync
        if (lastPpsTime_ > 0) {
            auto now = std::chrono::high_resolution_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                now.time_since_epoch()).count();
            stats.lastSyncDuration = (timestamp - lastPpsTime_) / 1.0e9;
        }
        
        return stats;
    }
    
    /**
     * @brief Get synchronization status
     * @return Current synchronization status
     */
    SyncStatus getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return status_;
    }
    
    /**
     * @brief Register callback for synchronization events
     * @param callback Function to call on synchronization events
     */
    void registerEventCallback(SyncEventCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        eventCallback_ = callback;
    }
    
    /**
     * @brief Get a precise timestamp with nanosecond resolution
     * @return High precision timestamp in nanoseconds since epoch
     */
    uint64_t getPreciseTimestamp() const {
        // Get current high-resolution time
        auto now = std::chrono::high_resolution_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();
        
        // Get Kalman filter prediction for current time
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Apply Kalman filter offset
        int64_t kalmanOffset = static_cast<int64_t>(kalmanFilter_.predict(timestamp));
        
        // Apply temperature compensation if enabled
        int64_t tempCompensation = 0;
        if (temperatureCompensationEnabled_) {
            double compPpb = temperatureCompensation_.getCompensation(currentTemperature_);
            
            // Convert from ppb to ns adjustment (over 1 second interval)
            if (lastPpsTime_ > 0) {
                double secondsSinceLastPps = (timestamp - lastPpsTime_) / 1.0e9;
                tempCompensation = static_cast<int64_t>(secondsSinceLastPps * compPpb / 1.0e9);
            }
        }
        
        return timestamp + kalmanOffset - tempCompensation;
    }
    
    /**
     * @brief Calculate time difference between two nodes
     * @param localTime Local time reference
     * @param remoteTime Remote time reference
     * @return Time difference in nanoseconds (positive if remote is ahead)
     */
    double calculateTimeDifference(
        const TimeReference& localTime,
        const TimeReference& remoteTime) const {
        
        // Direct nanosecond comparison
        int64_t diff = static_cast<int64_t>(remoteTime.nanoseconds) - 
                      static_cast<int64_t>(localTime.nanoseconds);
        
        // Calculate combined uncertainty
        double uncertainty = std::sqrt(
            localTime.uncertainty * localTime.uncertainty +
            remoteTime.uncertainty * remoteTime.uncertainty);
        
        // If difference is smaller than combined uncertainty, it might be noise
        if (std::abs(diff) < uncertainty) {
            std::cout << "Time difference (" << diff << " ns) is smaller than "
                     << "combined uncertainty (" << uncertainty << " ns)" << std::endl;
        }
        
        return static_cast<double>(diff);
    }
    
    /**
     * @brief Calibrate time synchronization
     * @param offsetNanoseconds Known offset in nanoseconds to apply
     * @return True if calibration was successful
     */
    bool calibrate(double offsetNanoseconds) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Get current time
        auto now = std::chrono::high_resolution_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();
        
        // Apply offset to Kalman filter
        kalmanFilter_.update(timestamp, offsetNanoseconds, timeUncertainty_);
        
        // Update uncertainty from Kalman filter
        timeUncertainty_ = kalmanFilter_.getUncertainty();
        
        // Log calibration event
        if (eventCallback_) {
            TimeReference ref = getTimeReference();
            eventCallback_(ref, "Manual calibration applied: " + 
                          std::to_string(offsetNanoseconds) + " ns");
        }
        
        return true;
    }
    
    /**
     * @brief Configure temperature compensation
     * @param enable Enable/disable temperature compensation
     * @param coefficient Temperature coefficient in ppb/°C
     * @return True if configuration successful
     */
    bool configureTemperatureCompensation(bool enable, double coefficient) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        temperatureCompensationEnabled_ = enable;
        temperatureCompensation_.setEnabled(enable);
        
        if (enable && coefficient != 0.0) {
            temperatureCoefficient_ = coefficient;
            temperatureCompensation_.setCoefficient(coefficient);
        }
        
        return true;
    }
    
    /**
     * @brief Configure fallback behavior during GPS signal loss
     * @param maxHoldoverTime Maximum time in seconds to operate in holdover mode
     * @param driftThreshold Maximum acceptable drift in ppb before going to error state
     * @return True if configuration successful
     */
    bool configureHoldover(double maxHoldoverTime, double driftThreshold) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        maxHoldoverTime_ = maxHoldoverTime;
        driftThreshold_ = driftThreshold;
        
        return true;
    }
    
    /**
     * @brief Set calibration values for systematic delays
     * @param antennaDelay Antenna delay in nanoseconds
     * @param cableDelay Cable delay in nanoseconds
     * @param receiverDelay Receiver processing delay in nanoseconds
     * @return True if calibration values were set successfully
     */
    bool setDelayCalibration(double antennaDelay, double cableDelay, double receiverDelay) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        antennaDelay_ = antennaDelay;
        cableDelay_ = cableDelay;
        receiverDelay_ = receiverDelay;
        
        // Apply total calibration to the offset
        double totalDelay = antennaDelay + cableDelay + receiverDelay;
        
        // Apply offset to Kalman filter
        auto now = std::chrono::high_resolution_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();
        
        // Negative offset because delays make the signal arrive later than it should
        kalmanFilter_.update(timestamp, -totalDelay, timeUncertainty_);
        
        // Update uncertainty from Kalman filter
        timeUncertainty_ = kalmanFilter_.getUncertainty();
        
        // Log calibration event
        if (eventCallback_) {
            TimeReference ref = getTimeReference();
            eventCallback_(ref, "Delay calibration applied: " + 
                          std::to_string(totalDelay) + " ns");
        }
        
        return true;
    }

private:
    mutable std::mutex mutex_;             ///< Mutex for thread safety
    SyncStatus status_;                   ///< Current synchronization status
    std::unique_ptr<devices::GPSDevice> gpsDevice_; ///< GPS device instance
    
    std::atomic<bool> running_;           ///< Thread control flag
    std::thread measurementThread_;       ///< Thread for time measurements
    
    double ppsCaptureLatency_;            ///< Measured PPS capture latency in ns
    double antennaDelay_;                 ///< Antenna delay in nanoseconds
    double cableDelay_;                   ///< Cable delay in nanoseconds
    double receiverDelay_;                ///< Receiver processing delay in nanoseconds
    
    double maxHoldoverTime_;              ///< Maximum holdover time in seconds
    double driftThreshold_;               ///< Maximum drift threshold in ppb
    
    bool temperatureCompensationEnabled_; ///< Temperature compensation flag
    double temperatureCoefficient_;       ///< Temperature coefficient in ppb/°C
    double currentTemperature_;           ///< Current temperature in °C
    
    int64_t timeOffset_;                  ///< Calibrated time offset in nanoseconds
    double timeUncertainty_;              ///< Time uncertainty in nanoseconds
    
    uint64_t lastPpsTime_;                ///< Timestamp of last PPS event
    devices::GPSData lastGpsData_;        ///< Last received GPS data
    
    std::deque<int64_t> timeOffsets_;     ///< History of time offset measurements
    uint32_t measurementCount_;           ///< Count of measurements received
    
    SyncEventCallback eventCallback_;     ///< Callback for sync events
    
    KalmanFilter kalmanFilter_;           ///< Kalman filter for timing
    AllanVariance allanVariance_;         ///< Allan variance calculator
    TemperatureCompensation temperatureCompensation_; ///< Temperature compensation
    
    /**
     * @brief Measurement thread function
     */
    void measurementThreadFunc() {
        while (running_) {
            // Sleep for a short time to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Check if we've lost PPS for too long
            checkPpsTimeout();
        }
    }
    
    /**
     * @brief Handle GPS data update
     * @param data GPS data
     */
    void handleGPSData(const devices::GPSData& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Store the latest GPS data
        lastGpsData_ = data;
        
        // Update status based on GPS fix
        if (data.fix && status_ == SyncStatus::Acquiring) {
            status_ = SyncStatus::Synchronized;
            
            // Notify via callback
            if (eventCallback_) {
                TimeReference ref = getTimeReference();
                eventCallback_(ref, "GPS synchronization achieved with " + 
                              std::to_string(data.satellites) + " satellites");
            }
        } else if (!data.fix && status_ == SyncStatus::Synchronized) {
            status_ = SyncStatus::Acquiring;
            
            // Notify via callback
            if (eventCallback_) {
                TimeReference ref = getTimeReference();
                eventCallback_(ref, "GPS fix lost, re-acquiring");
            }
        }
    }
    
    /**
     * @brief Handle PPS signal
     * @param timestamp High-resolution timestamp when PPS was detected
     */
    void handlePPS(uint64_t timestamp) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Increment PPS counter
        measurementCount_++;
        
        // Calculate expected UTC second boundary
        uint64_t utcSecondBoundary = 0;
        
        if (lastGpsData_.fix) {
            // Convert GPS time to UTC nanoseconds
            auto timePoint = lastGpsData_.toSystemTime();
            auto sinceEpoch = timePoint.time_since_epoch();
            uint64_t utcNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
                sinceEpoch).count();
            
            // Round to the next second boundary
            utcSecondBoundary = ((utcNanos / 1000000000) + 1) * 1000000000;
            
            // Calculate offset between the timestamp and the UTC second boundary
            int64_t offset = static_cast<int64_t>(utcSecondBoundary) - 
                           static_cast<int64_t>(timestamp);
            
            // Apply PPS offset from the GPS device
            offset -= static_cast<int64_t>(gpsDevice_->getPPSOffset());
            
            // Account for antenna, cable and receiver delays
            offset -= static_cast<int64_t>(antennaDelay_ + cableDelay_ + receiverDelay_);
            
            // Apply temperature compensation
            if (temperatureCompensationEnabled_) {
                double compPpb = temperatureCompensation_.getCompensation(currentTemperature_);
                
                // Convert from ppb to ns adjustment (over 1 second interval)
                int64_t tempAdjustment = static_cast<int64_t>(compPpb / 1000.0);
                offset -= tempAdjustment;
            }
            
            // Store the offset for Allan variance calculation
            allanVariance_.addSample(timestamp, offset);
            
            // Update Kalman filter with new measurement
            kalmanFilter_.update(timestamp, offset, timeUncertainty_);
            
            // Use Kalman filter's uncertainty estimate
            timeUncertainty_ = kalmanFilter_.getUncertainty();
            
            // Store the offset for backup calculations (keep last 60 values = 1 minute)
            timeOffsets_.push_back(offset);
            if (timeOffsets_.size() > 60) {
                timeOffsets_.pop_front();
            }
            
            // Update status
            if (status_ == SyncStatus::Acquiring && timeOffsets_.size() >= 5) {
                status_ = SyncStatus::Synchronized;
                
                // Notify via callback
                if (eventCallback_) {
                    TimeReference ref = getTimeReference();
                    eventCallback_(ref, "Time synchronization achieved with " + 
                                 std::to_string(timeUncertainty_) + " ns uncertainty");
                }
            } else if (status_ == SyncStatus::Holdover) {
                status_ = SyncStatus::Synchronized;
                
                // Notify via callback
                if (eventCallback_) {
                    TimeReference ref = getTimeReference();
                    eventCallback_(ref, "Recovered from holdover mode");
                }
            }
        }
        
        // Record timestamp of this PPS for timeout detection
        lastPpsTime_ = timestamp;
    }
    
    /**
     * @brief Check if PPS signal has been lost for too long
     */
    void checkPpsTimeout() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (lastPpsTime_ == 0 || !running_) {
            return;  // No PPS received yet or not running
        }
        
        // Get current time
        auto now = std::chrono::high_resolution_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();
        
        // Calculate time since last PPS
        double secondsSinceLastPps = static_cast<double>(timestamp - lastPpsTime_) / 1.0e9;
        
        // Check if we're in holdover mode and update status if needed
        if (status_ == SyncStatus::Synchronized && secondsSinceLastPps > 2.0) {
            status_ = SyncStatus::Holdover;
            
            // Notify via callback
            if (eventCallback_) {
                TimeReference ref = getTimeReference();
                eventCallback_(ref, "Entered holdover mode, PPS lost for " + 
                              std::to_string(secondsSinceLastPps) + " seconds");
            }
        } else if (status_ == SyncStatus::Holdover) {
            // Update uncertainty while in holdover based on Kalman filter's drift estimate
            double driftRatePpb = std::abs(kalmanFilter_.getDrift());
            double additionalUncertainty = secondsSinceLastPps * driftRatePpb / 1000.0;
            timeUncertainty_ = std::min(1.0e9, timeUncertainty_ + additionalUncertainty);
            
            // Check if we've been in holdover too long
            if (secondsSinceLastPps > maxHoldoverTime_ || 
                driftRatePpb > driftThreshold_) {
                
                status_ = SyncStatus::Error;
                
                // Notify via callback
                if (eventCallback_) {
                    TimeReference ref = getTimeReference();
                    eventCallback_(ref, "Holdover expired after " + 
                                 std::to_string(secondsSinceLastPps) + 
                                 " seconds, drift rate: " + 
                                 std::to_string(driftRatePpb) + " ppb");
                }
            }
        }
    }
    
    /**
     * @brief Calculate the number of missed PPS pulses
     * @return Number of missed PPS pulses
     */
    uint32_t calculateMissedPps() const {
        if (measurementCount_ < 2 || lastPpsTime_ == 0) {
            return 0;
        }
        
        // Get current time
        auto now = std::chrono::high_resolution_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();
        
        // Calculate time since first PPS in seconds
        double elapsedSeconds = static_cast<double>(timestamp - lastPpsTime_) / 1.0e9;
        
        // Expected PPS count is approximately elapsed time in seconds plus previous count
        uint32_t expectedIncrement = static_cast<uint32_t>(std::round(elapsedSeconds));
        uint32_t expectedCount = measurementCount_ + expectedIncrement;
        
        // Subtract 1 since the current second may not have elapsed yet
        if (expectedIncrement > 0) {
            expectedCount--;
        }
        
        // Missed count is the difference between expected and actual
        return (expectedCount > measurementCount_) ? 
                (expectedCount - measurementCount_) : 0;
    }
};

// GPSTimeSync implementation (just delegates to the Impl)

GPSTimeSync::GPSTimeSync() : pImpl(std::make_unique<Impl>()) {
}

GPSTimeSync::~GPSTimeSync() = default;

bool GPSTimeSync::initialize(const std::string& devicePath) {
    return pImpl->initialize(devicePath);
}

bool GPSTimeSync::start() {
    return pImpl->start();
}

bool GPSTimeSync::stop() {
    return pImpl->stop();
}

TimeReference GPSTimeSync::getTimeReference() const {
    return pImpl->getTimeReference();
}

SyncStatistics GPSTimeSync::getStatistics() const {
    return pImpl->getStatistics();
}

SyncStatus GPSTimeSync::getStatus() const {
    return pImpl->getStatus();
}

void GPSTimeSync::registerEventCallback(SyncEventCallback callback) {
    pImpl->registerEventCallback(callback);
}

uint64_t GPSTimeSync::getPreciseTimestamp() const {
    return pImpl->getPreciseTimestamp();
}

double GPSTimeSync::calculateTimeDifference(
    const TimeReference& localTime, 
    const TimeReference& remoteTime) const {
    return pImpl->calculateTimeDifference(localTime, remoteTime);
}

bool GPSTimeSync::calibrate(double offsetNanoseconds) {
    return pImpl->calibrate(offsetNanoseconds);
}

bool GPSTimeSync::configureTemperatureCompensation(bool enable, double coefficient) {
    return pImpl->configureTemperatureCompensation(enable, coefficient);
}

bool GPSTimeSync::configureHoldover(double maxHoldoverTime, double driftThreshold) {
    return pImpl->configureHoldover(maxHoldoverTime, driftThreshold);
}

bool GPSTimeSync::setDelayCalibration(double antennaDelay, double cableDelay, double receiverDelay) {
    return pImpl->setDelayCalibration(antennaDelay, cableDelay, receiverDelay);
}

} // namespace time_sync
} // namespace tdoa 