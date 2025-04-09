/**
 * @file time_difference_extractor.h
 * @brief Module for extracting time differences from signals
 */

#pragma once

#include "../correlation/cross_correlation.h"
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <memory>
#include <mutex>

namespace tdoa {
namespace time_difference {

/**
 * @struct SignalSource
 * @brief Structure to represent a signal source (receiver)
 */
struct SignalSource {
    std::string id;            ///< Unique identifier
    double x;                  ///< X position in meters
    double y;                  ///< Y position in meters
    double z;                  ///< Z position in meters
    double clockOffset;        ///< Clock offset in seconds
    double clockDrift;         ///< Clock drift in seconds/second
    double cableDelay;         ///< Cable delay in seconds
    double antennaDelay;       ///< Antenna delay in seconds
    
    /**
     * @brief Constructor with default values
     */
    SignalSource()
        : id("")
        , x(0.0)
        , y(0.0)
        , z(0.0)
        , clockOffset(0.0)
        , clockDrift(0.0)
        , cableDelay(0.0)
        , antennaDelay(0.0)
    {}
    
    /**
     * @brief Constructor with ID and position
     */
    SignalSource(const std::string& sourceId, double xPos, double yPos, double zPos = 0.0)
        : id(sourceId)
        , x(xPos)
        , y(yPos)
        , z(zPos)
        , clockOffset(0.0)
        , clockDrift(0.0)
        , cableDelay(0.0)
        , antennaDelay(0.0)
    {}
};

/**
 * @struct TimeDifference
 * @brief Structure to represent a time difference between two receivers
 */
struct TimeDifference {
    std::string sourceId1;     ///< First source ID
    std::string sourceId2;     ///< Second source ID
    double timeDiff;           ///< Time difference in seconds
    double uncertainty;        ///< Uncertainty in seconds
    double confidence;         ///< Confidence value (0-1)
    uint64_t timestamp;        ///< Timestamp when measurement was taken (ns since epoch)
    
    /**
     * @brief Constructor with default values
     */
    TimeDifference()
        : sourceId1("")
        , sourceId2("")
        , timeDiff(0.0)
        , uncertainty(0.0)
        , confidence(0.0)
        , timestamp(0)
    {}
    
    /**
     * @brief Constructor with source IDs and time difference
     */
    TimeDifference(
        const std::string& id1,
        const std::string& id2,
        double diff,
        double uncert = 0.0,
        double conf = 1.0,
        uint64_t time = 0)
        : sourceId1(id1)
        , sourceId2(id2)
        , timeDiff(diff)
        , uncertainty(uncert)
        , confidence(conf)
        , timestamp(time)
    {}
};

/**
 * @struct TimeDifferenceSet
 * @brief Structure to represent a set of time differences from multiple receivers
 */
struct TimeDifferenceSet {
    std::vector<TimeDifference> differences;  ///< Vector of time differences
    uint64_t timestamp;                       ///< Timestamp for the set
    std::string referenceId;                  ///< Reference source ID
};

/**
 * @enum CalibrationMode
 * @brief Calibration modes for the time difference extractor
 */
enum class CalibrationMode {
    None,           ///< No calibration
    Manual,         ///< Manual calibration
    Automatic,      ///< Automatic calibration
    Continuous      ///< Continuous calibration
};

/**
 * @enum ClockCorrectionMethod
 * @brief Methods for clock correction
 */
enum class ClockCorrectionMethod {
    None,           ///< No correction
    Offset,         ///< Offset only correction
    Linear,         ///< Linear drift correction
    Kalman          ///< Kalman filter correction
};

/**
 * @struct TimeDifferenceConfig
 * @brief Configuration for time difference extraction
 */
struct TimeDifferenceConfig {
    correlation::CorrelationConfig correlationConfig; ///< Configuration for correlation
    CalibrationMode calibrationMode;                  ///< Calibration mode
    ClockCorrectionMethod clockCorrectionMethod;      ///< Clock correction method
    double detectionThreshold;                        ///< Detection threshold (0-1)
    double outlierThreshold;                          ///< Outlier threshold (sigmas)
    int historySize;                                  ///< Number of measurements to keep in history
    bool enableStatisticalValidation;                 ///< Whether to validate measurements statistically
    
    /**
     * @brief Constructor with default values
     */
    TimeDifferenceConfig()
        : correlationConfig()
        , calibrationMode(CalibrationMode::None)
        , clockCorrectionMethod(ClockCorrectionMethod::None)
        , detectionThreshold(0.5)
        , outlierThreshold(3.0)
        , historySize(100)
        , enableStatisticalValidation(true)
    {}
};

/**
 * @class TimeDifferenceExtractor
 * @brief Class for extracting time differences from signals
 * 
 * This class handles the extraction of time differences between signals from
 * different receivers, including clock synchronization, calibration, and 
 * statistical validation.
 */
class TimeDifferenceExtractor {
public:
    /**
     * @brief Time difference callback
     */
    using TimeDifferenceCallback = std::function<void(const TimeDifferenceSet&)>;
    
    /**
     * @brief Constructor
     * @param config Configuration for time difference extraction
     */
    TimeDifferenceExtractor(const TimeDifferenceConfig& config = TimeDifferenceConfig());
    
    /**
     * @brief Destructor
     */
    ~TimeDifferenceExtractor();
    
    /**
     * @brief Add a signal source
     * @param source Signal source information
     * @return True if source was added successfully
     */
    bool addSource(const SignalSource& source);
    
    /**
     * @brief Remove a signal source
     * @param sourceId Signal source ID
     * @return True if source was removed successfully
     */
    bool removeSource(const std::string& sourceId);
    
    /**
     * @brief Get signal source
     * @param sourceId Signal source ID
     * @return Signal source (empty ID if not found)
     */
    SignalSource getSource(const std::string& sourceId) const;
    
    /**
     * @brief Set reference source
     * @param sourceId Signal source ID to use as reference
     * @return True if reference was set successfully
     */
    bool setReferenceSource(const std::string& sourceId);
    
    /**
     * @brief Get reference source ID
     * @return Reference source ID
     */
    std::string getReferenceSource() const;
    
    /**
     * @brief Process signal segments from multiple sources
     * @param signals Map of source ID to signal segment
     * @param timestamp Timestamp for the signals
     * @return Set of time differences
     */
    TimeDifferenceSet processSignals(
        const std::map<std::string, std::vector<double>>& signals,
        uint64_t timestamp);
    
    /**
     * @brief Process signal segments from multiple sources (complex version)
     * @param signals Map of source ID to complex signal segment
     * @param timestamp Timestamp for the signals
     * @return Set of time differences
     */
    TimeDifferenceSet processSignals(
        const std::map<std::string, std::vector<std::complex<double>>>& signals,
        uint64_t timestamp);
    
    /**
     * @brief Add a known time difference for calibration
     * @param timeDiff Time difference
     * @return True if calibration was successful
     */
    bool addCalibrationMeasurement(const TimeDifference& timeDiff);
    
    /**
     * @brief Get recent time differences
     * @return Vector of recent time differences
     */
    std::vector<TimeDifference> getRecentTimeDifferences() const;
    
    /**
     * @brief Set time difference callback
     * @param callback Function to call when new time differences are available
     */
    void setTimeDifferenceCallback(TimeDifferenceCallback callback);
    
    /**
     * @brief Get configuration
     * @return Current configuration
     */
    TimeDifferenceConfig getConfig() const;
    
    /**
     * @brief Set configuration
     * @param config New configuration
     */
    void setConfig(const TimeDifferenceConfig& config);
    
    /**
     * @brief Reset the extractor state
     */
    void reset();
    
    /**
     * @brief Set cable delay for a source
     * @param sourceId Signal source ID
     * @param delay Cable delay in seconds
     * @return True if delay was set successfully
     */
    bool setCableDelay(const std::string& sourceId, double delay);
    
    /**
     * @brief Set antenna delay for a source
     * @param sourceId Signal source ID
     * @param delay Antenna delay in seconds
     * @return True if delay was set successfully
     */
    bool setAntennaDelay(const std::string& sourceId, double delay);
    
    /**
     * @brief Set clock offset for a source
     * @param sourceId Signal source ID
     * @param offset Clock offset in seconds
     * @return True if offset was set successfully
     */
    bool setClockOffset(const std::string& sourceId, double offset);
    
    /**
     * @brief Set clock drift for a source
     * @param sourceId Signal source ID
     * @param drift Clock drift in seconds/second
     * @return True if drift was set successfully
     */
    bool setClockDrift(const std::string& sourceId, double drift);
    
    /**
     * @brief Start automatic calibration
     * @param sourcesToCalibrate Vector of source IDs to calibrate
     * @param referenceSources Vector of reference source IDs
     * @param durationSeconds Duration of calibration in seconds
     * @return True if calibration was started successfully
     */
    bool startAutomaticCalibration(
        const std::vector<std::string>& sourcesToCalibrate,
        const std::vector<std::string>& referenceSources,
        double durationSeconds);
    
private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace time_difference
} // namespace tdoa 