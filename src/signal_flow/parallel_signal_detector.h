#pragma once

#include "signal.h"
#include "signal_flow.h"
#include "signal_metadata.h"
#include "processing_component.h"
#include <memory>
#include <vector>
#include <functional>
#include <map>
#include <mutex>

namespace tdoa {
namespace signal {

/**
 * @struct DetectionConfig
 * @brief Configuration for parallel signal detection
 */
struct DetectionConfig {
    double minFrequency = 0.0;                  ///< Minimum frequency to detect (Hz)
    double maxFrequency = 6e9;                  ///< Maximum frequency to detect (Hz)
    double minBandwidth = 1e3;                  ///< Minimum signal bandwidth (Hz)
    double minSnr = 6.0;                        ///< Minimum SNR for detection (dB)
    double detectionThreshold = 0.7;            ///< Detection confidence threshold (0-1)
    size_t maxSignals = 100;                    ///< Maximum number of signals to detect
    bool enableAdaptiveThreshold = true;        ///< Enable adaptive thresholding
    bool enableSignalTracking = true;           ///< Enable signal continuity tracking
    double trackingTimeWindow = 1.0;            ///< Time window for signal tracking (seconds)
    double frequencyTolerance = 1e3;            ///< Frequency tolerance for tracking (Hz)
    double bandwidthTolerance = 0.2;            ///< Bandwidth tolerance for tracking (ratio)
};

/**
 * @struct DetectedSignal
 * @brief Information about a detected signal
 */
struct DetectedSignal {
    std::string id;                             ///< Unique signal identifier
    double centerFrequency;                     ///< Center frequency (Hz)
    double bandwidth;                           ///< Bandwidth (Hz)
    double snr;                                 ///< Signal-to-noise ratio (dB)
    double confidence;                          ///< Detection confidence (0-1)
    uint64_t timestamp;                         ///< Detection timestamp
    std::map<std::string, std::string> metadata; ///< Additional metadata
};

/**
 * @class ParallelSignalDetector
 * @brief Handles parallel detection of multiple signals
 */
class ParallelSignalDetector {
public:
    using DetectionCallback = std::function<void(const std::vector<DetectedSignal>&)>;

    /**
     * @brief Constructor
     * @param config Detection configuration
     */
    explicit ParallelSignalDetector(const DetectionConfig& config);

    /**
     * @brief Initialize the detector
     * @return True if initialization successful
     */
    bool initialize();

    /**
     * @brief Process a signal segment for detection
     * @param signal Input signal segment
     * @return Vector of detected signals
     */
    std::vector<DetectedSignal> processSegment(std::shared_ptr<Signal> signal);

    /**
     * @brief Process a signal segment asynchronously
     * @param signal Input signal segment
     * @param callback Callback for detection results
     * @return True if processing started successfully
     */
    bool processSegmentAsync(std::shared_ptr<Signal> signal, DetectionCallback callback);

    /**
     * @brief Update detector configuration
     * @param config New configuration
     */
    void updateConfig(const DetectionConfig& config);

    /**
     * @brief Get current detector configuration
     * @return Current configuration
     */
    DetectionConfig getConfig() const;

    /**
     * @brief Get detection statistics
     * @return Map of statistic name to value
     */
    std::map<std::string, double> getStats() const;

private:
    /**
     * @brief Process a frequency band for signal detection
     * @param signal Input signal
     * @param startFreq Start frequency
     * @param endFreq End frequency
     * @return Vector of detected signals in the band
     */
    std::vector<DetectedSignal> processBand(
        std::shared_ptr<Signal> signal,
        double startFreq,
        double endFreq
    );

    /**
     * @brief Track signal continuity
     * @param newSignals Newly detected signals
     * @return Updated signals with tracking information
     */
    std::vector<DetectedSignal> trackSignals(const std::vector<DetectedSignal>& newSignals);

    /**
     * @brief Generate a unique signal ID
     * @return Unique ID string
     */
    std::string generateSignalId() const;

private:
    DetectionConfig config_;
    std::unique_ptr<ProcessingChain> detectionChain_;
    std::map<std::string, DetectedSignal> signalHistory_;
    mutable std::mutex mutex_;
    std::map<std::string, double> stats_;
};

} // namespace signal
} // namespace tdoa 