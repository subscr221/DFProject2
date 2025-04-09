/**
 * @file time_difference_extractor.cpp
 * @brief Implementation of time difference extractor
 */

#include "time_difference_extractor.h"
#include "../correlation/cross_correlation.h"
#include <vector>
#include <map>
#include <unordered_map>
#include <deque>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <chrono>
#include <thread>
#include <mutex>
#include <stdexcept>

namespace tdoa {
namespace time_difference {

/**
 * @class TimeDifferenceExtractor::Impl
 * @brief Implementation details for TimeDifferenceExtractor
 */
struct TimeDifferenceExtractor::Impl {
    // Configuration
    TimeDifferenceConfig config;
    
    // Signal sources
    std::unordered_map<std::string, SignalSource> sources;
    std::string referenceSourceId;
    
    // Correlation processors
    std::unordered_map<std::string, correlation::SegmentedCorrelator> correlators;
    
    // History of time differences
    std::unordered_map<std::string, std::deque<TimeDifference>> timeDifferenceHistory;
    
    // Calibration data
    std::unordered_map<std::string, std::vector<TimeDifference>> calibrationData;
    
    // Callback function
    TimeDifferenceCallback timeDifferenceCallback;
    
    // Mutex for thread safety
    mutable std::mutex mutex;
    
    // Calibration thread
    std::thread calibrationThread;
    bool calibrationRunning;
    
    /**
     * @brief Constructor
     * @param config Configuration for time difference extraction
     */
    Impl(const TimeDifferenceConfig& config)
        : config(config)
        , calibrationRunning(false)
    {
    }
    
    /**
     * @brief Destructor
     */
    ~Impl() {
        stopCalibration();
    }
    
    /**
     * @brief Add a signal source
     * @param source Signal source information
     * @return True if source was added successfully
     */
    bool addSource(const SignalSource& source) {
        if (source.id.empty()) {
            return false;
        }
        
        std::lock_guard<std::mutex> lock(mutex);
        
        // Add source
        sources[source.id] = source;
        
        // Create a correlator for this source (if it's not the reference)
        if (!referenceSourceId.empty() && source.id != referenceSourceId) {
            const auto& refSource = sources[referenceSourceId];
            
            // Create correlation for this pair
            const std::string pairKey = getPairKey(referenceSourceId, source.id);
            correlators.emplace(pairKey, correlation::SegmentedCorrelator(config.correlationConfig));
            
            // Create empty history for this pair
            timeDifferenceHistory[pairKey] = std::deque<TimeDifference>();
        }
        
        // If this is the first source, set it as reference
        if (referenceSourceId.empty()) {
            referenceSourceId = source.id;
        }
        
        return true;
    }
    
    /**
     * @brief Remove a signal source
     * @param sourceId Signal source ID
     * @return True if source was removed successfully
     */
    bool removeSource(const std::string& sourceId) {
        std::lock_guard<std::mutex> lock(mutex);
        
        // Check if source exists
        auto it = sources.find(sourceId);
        if (it == sources.end()) {
            return false;
        }
        
        // Remove correlators and history for this source
        for (auto it2 = correlators.begin(); it2 != correlators.end();) {
            const std::string& pairKey = it2->first;
            if (pairKey.find(sourceId) != std::string::npos) {
                it2 = correlators.erase(it2);
                timeDifferenceHistory.erase(pairKey);
            } else {
                ++it2;
            }
        }
        
        // Remove source
        sources.erase(it);
        
        // If we removed the reference source, select a new one if available
        if (sourceId == referenceSourceId) {
            if (!sources.empty()) {
                referenceSourceId = sources.begin()->first;
            } else {
                referenceSourceId.clear();
            }
        }
        
        return true;
    }
    
    /**
     * @brief Get signal source
     * @param sourceId Signal source ID
     * @return Signal source (empty ID if not found)
     */
    SignalSource getSource(const std::string& sourceId) const {
        std::lock_guard<std::mutex> lock(mutex);
        
        auto it = sources.find(sourceId);
        if (it == sources.end()) {
            return SignalSource();  // Return empty source
        }
        
        return it->second;
    }
    
    /**
     * @brief Set reference source
     * @param sourceId Signal source ID to use as reference
     * @return True if reference was set successfully
     */
    bool setReferenceSource(const std::string& sourceId) {
        std::lock_guard<std::mutex> lock(mutex);
        
        // Check if source exists
        if (sources.find(sourceId) == sources.end()) {
            return false;
        }
        
        // Change reference source
        referenceSourceId = sourceId;
        
        // Recreate correlators for all sources
        correlators.clear();
        timeDifferenceHistory.clear();
        
        for (const auto& source : sources) {
            if (source.first != referenceSourceId) {
                const std::string pairKey = getPairKey(referenceSourceId, source.first);
                correlators.emplace(pairKey, correlation::SegmentedCorrelator(config.correlationConfig));
                timeDifferenceHistory[pairKey] = std::deque<TimeDifference>();
            }
        }
        
        return true;
    }
    
    /**
     * @brief Get reference source ID
     * @return Reference source ID
     */
    std::string getReferenceSource() const {
        std::lock_guard<std::mutex> lock(mutex);
        return referenceSourceId;
    }
    
    /**
     * @brief Process signal segments from multiple sources
     * @param signals Map of source ID to signal segment
     * @param timestamp Timestamp for the signals
     * @return Set of time differences
     */
    TimeDifferenceSet processSignals(
        const std::map<std::string, std::vector<double>>& signals,
        uint64_t timestamp) {
        
        std::lock_guard<std::mutex> lock(mutex);
        
        // Check if we have a reference source
        if (referenceSourceId.empty()) {
            return TimeDifferenceSet();  // No reference source
        }
        
        // Check if reference source has data
        auto refIt = signals.find(referenceSourceId);
        if (refIt == signals.end()) {
            return TimeDifferenceSet();  // No reference signal
        }
        
        const std::vector<double>& refSignal = refIt->second;
        
        // Create result set
        TimeDifferenceSet result;
        result.timestamp = timestamp;
        result.referenceId = referenceSourceId;
        
        // Process each source's signal against the reference
        for (const auto& pair : signals) {
            const std::string& sourceId = pair.first;
            const std::vector<double>& signal = pair.second;
            
            // Skip reference source
            if (sourceId == referenceSourceId) {
                continue;
            }
            
            // Get source
            auto sourceIt = sources.find(sourceId);
            if (sourceIt == sources.end()) {
                continue;  // Unknown source
            }
            
            const SignalSource& source = sourceIt->second;
            
            // Get correlator for this pair
            const std::string pairKey = getPairKey(referenceSourceId, sourceId);
            auto correlatorIt = correlators.find(pairKey);
            if (correlatorIt == correlators.end()) {
                // Create new correlator
                correlators.emplace(pairKey, correlation::SegmentedCorrelator(config.correlationConfig));
                correlatorIt = correlators.find(pairKey);
                timeDifferenceHistory[pairKey] = std::deque<TimeDifference>();
            }
            
            // Process signals
            correlation::CorrelationResult corrResult = correlatorIt->second.processSegment(refSignal, signal);
            
            // Check if we have any peaks
            if (corrResult.peaks.empty()) {
                continue;  // No peaks detected
            }
            
            // Find the best peak
            auto bestPeak = std::max_element(
                corrResult.peaks.begin(), corrResult.peaks.end(),
                [](const auto& a, const auto& b) { return a.confidence < b.confidence; });
            
            // Check if peak is above threshold
            if (bestPeak->confidence < config.detectionThreshold) {
                continue;  // Peak too weak
            }
            
            // Calculate time difference in seconds
            double timeDiff = correlation::samplesToTime(bestPeak->delay, config.correlationConfig.sampleRate);
            
            // Adjust time delay based on correlation lag window
            timeDiff -= (refSignal.size() + signal.size() - 1) / 2.0 / config.correlationConfig.sampleRate;
            
            // Apply clock correction if enabled
            if (config.clockCorrectionMethod != ClockCorrectionMethod::None) {
                timeDiff = applyClockCorrection(timeDiff, source, timestamp);
            }
            
            // Calculate uncertainty based on peak confidence
            double uncertainty = (1.0 - bestPeak->confidence) * 1.0e-6;  // Scale to typical range
            
            // Create time difference object
            TimeDifference diff(referenceSourceId, sourceId, timeDiff, uncertainty, 
                               bestPeak->confidence, timestamp);
            
            // Add to history
            auto& history = timeDifferenceHistory[pairKey];
            history.push_back(diff);
            
            // Limit history size
            while (history.size() > static_cast<size_t>(config.historySize)) {
                history.pop_front();
            }
            
            // Validate measurement if enabled
            if (config.enableStatisticalValidation && history.size() >= 3) {
                if (!validateMeasurement(diff, history)) {
                    continue;  // Invalid measurement
                }
            }
            
            // Add to result
            result.differences.push_back(diff);
        }
        
        // Call callback if registered
        if (!result.differences.empty() && timeDifferenceCallback) {
            timeDifferenceCallback(result);
        }
        
        return result;
    }
    
    /**
     * @brief Process signal segments from multiple sources (complex version)
     * @param signals Map of source ID to complex signal segment
     * @param timestamp Timestamp for the signals
     * @return Set of time differences
     */
    TimeDifferenceSet processSignals(
        const std::map<std::string, std::vector<std::complex<double>>>& signals,
        uint64_t timestamp) {
        
        std::lock_guard<std::mutex> lock(mutex);
        
        // Check if we have a reference source
        if (referenceSourceId.empty()) {
            return TimeDifferenceSet();  // No reference source
        }
        
        // Check if reference source has data
        auto refIt = signals.find(referenceSourceId);
        if (refIt == signals.end()) {
            return TimeDifferenceSet();  // No reference signal
        }
        
        const std::vector<std::complex<double>>& refSignal = refIt->second;
        
        // Create result set
        TimeDifferenceSet result;
        result.timestamp = timestamp;
        result.referenceId = referenceSourceId;
        
        // Process each source's signal against the reference
        for (const auto& pair : signals) {
            const std::string& sourceId = pair.first;
            const std::vector<std::complex<double>>& signal = pair.second;
            
            // Skip reference source
            if (sourceId == referenceSourceId) {
                continue;
            }
            
            // Get source
            auto sourceIt = sources.find(sourceId);
            if (sourceIt == sources.end()) {
                continue;  // Unknown source
            }
            
            const SignalSource& source = sourceIt->second;
            
            // Get correlator for this pair
            const std::string pairKey = getPairKey(referenceSourceId, sourceId);
            auto correlatorIt = correlators.find(pairKey);
            if (correlatorIt == correlators.end()) {
                // Create new correlator
                correlators.emplace(pairKey, correlation::SegmentedCorrelator(config.correlationConfig));
                correlatorIt = correlators.find(pairKey);
                timeDifferenceHistory[pairKey] = std::deque<TimeDifference>();
            }
            
            // Process signals
            correlation::CorrelationResult corrResult = correlatorIt->second.processSegment(refSignal, signal);
            
            // Check if we have any peaks
            if (corrResult.peaks.empty()) {
                continue;  // No peaks detected
            }
            
            // Find the best peak
            auto bestPeak = std::max_element(
                corrResult.peaks.begin(), corrResult.peaks.end(),
                [](const auto& a, const auto& b) { return a.confidence < b.confidence; });
            
            // Check if peak is above threshold
            if (bestPeak->confidence < config.detectionThreshold) {
                continue;  // Peak too weak
            }
            
            // Calculate time difference in seconds
            double timeDiff = correlation::samplesToTime(bestPeak->delay, config.correlationConfig.sampleRate);
            
            // Adjust time delay based on correlation lag window
            timeDiff -= (refSignal.size() + signal.size() - 1) / 2.0 / config.correlationConfig.sampleRate;
            
            // Apply clock correction if enabled
            if (config.clockCorrectionMethod != ClockCorrectionMethod::None) {
                timeDiff = applyClockCorrection(timeDiff, source, timestamp);
            }
            
            // Calculate uncertainty based on peak confidence
            double uncertainty = (1.0 - bestPeak->confidence) * 1.0e-6;  // Scale to typical range
            
            // Create time difference object
            TimeDifference diff(referenceSourceId, sourceId, timeDiff, uncertainty, 
                               bestPeak->confidence, timestamp);
            
            // Add to history
            auto& history = timeDifferenceHistory[pairKey];
            history.push_back(diff);
            
            // Limit history size
            while (history.size() > static_cast<size_t>(config.historySize)) {
                history.pop_front();
            }
            
            // Validate measurement if enabled
            if (config.enableStatisticalValidation && history.size() >= 3) {
                if (!validateMeasurement(diff, history)) {
                    continue;  // Invalid measurement
                }
            }
            
            // Add to result
            result.differences.push_back(diff);
        }
        
        // Call callback if registered
        if (!result.differences.empty() && timeDifferenceCallback) {
            timeDifferenceCallback(result);
        }
        
        return result;
    }

    // Helper methods
    
    /**
     * @brief Generate a unique key for a source pair
     * @param id1 First source ID
     * @param id2 Second source ID
     * @return Unique key
     */
    std::string getPairKey(const std::string& id1, const std::string& id2) const {
        return id1 + "_" + id2;
    }
    
    /**
     * @brief Apply clock correction to time difference
     * @param timeDiff Uncorrected time difference
     * @param source Signal source
     * @param timestamp Measurement timestamp
     * @return Corrected time difference
     */
    double applyClockCorrection(double timeDiff, const SignalSource& source, uint64_t timestamp) const {
        double correctedDiff = timeDiff;
        
        // Apply cable and antenna delays
        correctedDiff -= source.cableDelay + source.antennaDelay;
        
        // Apply clock offset
        correctedDiff -= source.clockOffset;
        
        // Apply clock drift if using linear or Kalman correction
        if (config.clockCorrectionMethod == ClockCorrectionMethod::Linear || 
            config.clockCorrectionMethod == ClockCorrectionMethod::Kalman) {
            
            // Calculate elapsed time in seconds since first measurement
            double elapsedSec = timestamp * 1e-9;  // Convert ns to seconds
            correctedDiff -= source.clockDrift * elapsedSec;
        }
        
        return correctedDiff;
    }
    
    /**
     * @brief Validate a measurement against history
     * @param diff Time difference to validate
     * @param history History of measurements for this pair
     * @return True if measurement is valid
     */
    bool validateMeasurement(const TimeDifference& diff, 
                             const std::deque<TimeDifference>& history) const {
        if (history.size() < 3) {
            return true;  // Not enough history for validation
        }
        
        // Calculate mean and standard deviation of recent measurements
        std::vector<double> recentDiffs;
        for (auto it = history.rbegin(); it != history.rend() && recentDiffs.size() < 5; ++it) {
            recentDiffs.push_back(it->timeDiff);
        }
        
        double mean = std::accumulate(recentDiffs.begin(), recentDiffs.end(), 0.0) / 
                     recentDiffs.size();
        
        double sumSqDiff = 0.0;
        for (const auto& val : recentDiffs) {
            sumSqDiff += std::pow(val - mean, 2);
        }
        double stdDev = std::sqrt(sumSqDiff / recentDiffs.size());
        
        // Add a minimum standard deviation to avoid false positives with very stable signals
        stdDev = std::max(stdDev, 1e-9);
        
        // Calculate Z-score
        double zScore = std::abs(diff.timeDiff - mean) / stdDev;
        
        // Check if measurement is an outlier
        return zScore <= config.outlierThreshold;
    }
    
    /**
     * @brief Stop calibration thread
     */
    void stopCalibration() {
        if (calibrationRunning) {
            calibrationRunning = false;
            if (calibrationThread.joinable()) {
                calibrationThread.join();
            }
        }
    }
};

// TimeDifferenceExtractor implementation (delegates to Impl)

TimeDifferenceExtractor::TimeDifferenceExtractor(const TimeDifferenceConfig& config)
    : pImpl(std::make_unique<Impl>(config))
{
}

TimeDifferenceExtractor::~TimeDifferenceExtractor() = default;

bool TimeDifferenceExtractor::addSource(const SignalSource& source) {
    return pImpl->addSource(source);
}

bool TimeDifferenceExtractor::removeSource(const std::string& sourceId) {
    return pImpl->removeSource(sourceId);
}

SignalSource TimeDifferenceExtractor::getSource(const std::string& sourceId) const {
    return pImpl->getSource(sourceId);
}

bool TimeDifferenceExtractor::setReferenceSource(const std::string& sourceId) {
    return pImpl->setReferenceSource(sourceId);
}

std::string TimeDifferenceExtractor::getReferenceSource() const {
    return pImpl->getReferenceSource();
}

TimeDifferenceSet TimeDifferenceExtractor::processSignals(
    const std::map<std::string, std::vector<double>>& signals,
    uint64_t timestamp) {
    return pImpl->processSignals(signals, timestamp);
}

TimeDifferenceSet TimeDifferenceExtractor::processSignals(
    const std::map<std::string, std::vector<std::complex<double>>>& signals,
    uint64_t timestamp) {
    return pImpl->processSignals(signals, timestamp);
}

void TimeDifferenceExtractor::setTimeDifferenceCallback(TimeDifferenceCallback callback) {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    pImpl->timeDifferenceCallback = callback;
}

TimeDifferenceConfig TimeDifferenceExtractor::getConfig() const {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    return pImpl->config;
}

void TimeDifferenceExtractor::setConfig(const TimeDifferenceConfig& config) {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    pImpl->config = config;
    
    // Update correlator configurations
    for (auto& correlator : pImpl->correlators) {
        correlator.second.setConfig(config.correlationConfig);
    }
}

void TimeDifferenceExtractor::reset() {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    
    // Reset all correlators
    for (auto& correlator : pImpl->correlators) {
        correlator.second.reset();
    }
    
    // Clear history
    pImpl->timeDifferenceHistory.clear();
}

bool TimeDifferenceExtractor::setCableDelay(const std::string& sourceId, double delay) {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    
    auto it = pImpl->sources.find(sourceId);
    if (it == pImpl->sources.end()) {
        return false;
    }
    
    it->second.cableDelay = delay;
    return true;
}

bool TimeDifferenceExtractor::setAntennaDelay(const std::string& sourceId, double delay) {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    
    auto it = pImpl->sources.find(sourceId);
    if (it == pImpl->sources.end()) {
        return false;
    }
    
    it->second.antennaDelay = delay;
    return true;
}

bool TimeDifferenceExtractor::setClockOffset(const std::string& sourceId, double offset) {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    
    auto it = pImpl->sources.find(sourceId);
    if (it == pImpl->sources.end()) {
        return false;
    }
    
    it->second.clockOffset = offset;
    return true;
}

bool TimeDifferenceExtractor::setClockDrift(const std::string& sourceId, double drift) {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    
    auto it = pImpl->sources.find(sourceId);
    if (it == pImpl->sources.end()) {
        return false;
    }
    
    it->second.clockDrift = drift;
    return true;
}

std::vector<TimeDifference> TimeDifferenceExtractor::getRecentTimeDifferences() const {
    std::lock_guard<std::mutex> lock(pImpl->mutex);
    
    std::vector<TimeDifference> result;
    
    for (const auto& pair : pImpl->timeDifferenceHistory) {
        const auto& history = pair.second;
        if (!history.empty()) {
            result.push_back(history.back());
        }
    }
    
    return result;
}

bool TimeDifferenceExtractor::addCalibrationMeasurement(const TimeDifference& timeDiff) {
    // This would be implemented for manual calibration
    return false;
}

bool TimeDifferenceExtractor::startAutomaticCalibration(
    const std::vector<std::string>& sourcesToCalibrate,
    const std::vector<std::string>& referenceSources,
    double durationSeconds) {
    
    // This would be implemented for automatic calibration
    return false;
}

} // namespace time_difference
} // namespace tdoa 