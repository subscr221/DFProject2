/**
 * @file signal_tracker.h
 * @brief Multi-signal tracking and continuity management
 */

#pragma once

#include "signal.h"
#include "signal_metadata.h"
#include "signal_prioritizer.h"
#include <memory>
#include <vector>
#include <map>
#include <deque>
#include <mutex>
#include <chrono>

namespace tdoa {
namespace signal {

/**
 * @struct TrackingConfig
 * @brief Configuration for signal tracking
 */
struct TrackingConfig {
    double timeWindow = 5.0;            ///< Time window for track history (seconds)
    double frequencyTolerance = 1e3;    ///< Frequency tolerance for track matching (Hz)
    double bandwidthTolerance = 0.2;    ///< Bandwidth tolerance for track matching (ratio)
    double powerTolerance = 10.0;       ///< Power level tolerance for track matching (dB)
    size_t maxTracks = 100;             ///< Maximum number of active tracks
    bool enablePrediction = true;       ///< Enable track prediction
    bool enableMerging = true;          ///< Enable track merging for overlapping signals
    double mergingThreshold = 0.8;      ///< Threshold for track merging (similarity score)
};

/**
 * @struct TrackPoint
 * @brief Single point in a signal track
 */
struct TrackPoint {
    std::chrono::system_clock::time_point timestamp;
    double frequency;      ///< Center frequency (Hz)
    double bandwidth;      ///< Bandwidth (Hz)
    double power;         ///< Signal power (dBm)
    double snr;           ///< Signal-to-noise ratio (dB)
    double confidence;    ///< Detection confidence
    SignalClass signalClass; ///< Signal classification
    std::map<std::string, std::string> metadata; ///< Additional metadata
};

/**
 * @struct Track
 * @brief Continuous track of a signal over time
 */
struct Track {
    std::string id;                     ///< Unique track ID
    std::deque<TrackPoint> points;      ///< Track points in chronological order
    SignalClass primaryClass;           ///< Primary signal classification
    std::map<SignalClass, double> classConfidence; ///< Confidence in each classification
    bool active;                        ///< Whether track is currently active
    std::chrono::system_clock::time_point lastUpdate; ///< Last update timestamp
    std::map<std::string, std::string> metadata; ///< Track metadata
};

/**
 * @class SignalTracker
 * @brief Manages multiple signal tracks and their continuity
 */
class SignalTracker {
public:
    using TrackUpdateCallback = std::function<void(const Track&)>;
    using TrackEndCallback = std::function<void(const Track&)>;

    /**
     * @brief Constructor
     * @param config Tracking configuration
     */
    explicit SignalTracker(const TrackingConfig& config);

    /**
     * @brief Initialize the tracker
     * @return True if initialization successful
     */
    bool initialize();

    /**
     * @brief Update tracks with new signal detections
     * @param signals Vector of detected signals
     * @return Vector of updated track IDs
     */
    std::vector<std::string> updateTracks(const std::vector<DetectedSignal>& signals);

    /**
     * @brief Get track by ID
     * @param trackId Track ID
     * @return Track if found, nullptr otherwise
     */
    std::shared_ptr<Track> getTrack(const std::string& trackId) const;

    /**
     * @brief Get all active tracks
     * @return Vector of active tracks
     */
    std::vector<std::shared_ptr<Track>> getActiveTracks() const;

    /**
     * @brief Set callback for track updates
     * @param callback Function to call when track is updated
     */
    void setTrackUpdateCallback(TrackUpdateCallback callback);

    /**
     * @brief Set callback for track end
     * @param callback Function to call when track ends
     */
    void setTrackEndCallback(TrackEndCallback callback);

    /**
     * @brief Update tracker configuration
     * @param config New configuration
     */
    void updateConfig(const TrackingConfig& config);

    /**
     * @brief Get current configuration
     * @return Current tracking configuration
     */
    TrackingConfig getConfig() const;

    /**
     * @brief Get tracker statistics
     * @return Map of statistic name to value
     */
    std::map<std::string, double> getStats() const;

private:
    /**
     * @brief Match signals to existing tracks
     * @param signals Vector of detected signals
     * @return Map of signal index to matching track ID
     */
    std::map<size_t, std::string> matchSignalsToTracks(
        const std::vector<DetectedSignal>& signals);

    /**
     * @brief Create new track from signal
     * @param signal Detected signal
     * @return New track ID
     */
    std::string createTrack(const DetectedSignal& signal);

    /**
     * @brief Update track with new signal
     * @param trackId Track ID
     * @param signal New signal detection
     */
    void updateTrack(const std::string& trackId, const DetectedSignal& signal);

    /**
     * @brief Predict next track point
     * @param track Track to predict
     * @return Predicted track point
     */
    TrackPoint predictNextPoint(const Track& track);

    /**
     * @brief Check if tracks should be merged
     * @param track1 First track
     * @param track2 Second track
     * @return True if tracks should be merged
     */
    bool shouldMergeTracks(const Track& track1, const Track& track2);

    /**
     * @brief Merge two tracks
     * @param track1Id First track ID
     * @param track2Id Second track ID
     * @return ID of merged track
     */
    std::string mergeTracks(const std::string& track1Id, const std::string& track2Id);

    /**
     * @brief Clean up old tracks and points
     */
    void cleanupTracks();

    /**
     * @brief Generate unique track ID
     * @return Unique track ID
     */
    std::string generateTrackId() const;

private:
    TrackingConfig config_;
    std::map<std::string, std::shared_ptr<Track>> tracks_;
    TrackUpdateCallback updateCallback_;
    TrackEndCallback endCallback_;
    mutable std::mutex mutex_;
    std::map<std::string, double> stats_;
};

} // namespace signal
} // namespace tdoa 