/**
 * @file signal_quality_analyzer.h
 * @brief Signal quality assessment and validation
 */

#pragma once

#include "signal.h"
#include "signal_metadata.h"
#include "signal_tracker.h"
#include <memory>
#include <vector>
#include <map>
#include <functional>
#include <mutex>

namespace tdoa {
namespace signal {

/**
 * @struct QualityConfig
 * @brief Configuration for signal quality assessment
 */
struct QualityConfig {
    double minSnr = 6.0;                ///< Minimum SNR for valid signal (dB)
    double minConfidence = 0.6;         ///< Minimum confidence level
    double maxPhaseNoise = -80.0;       ///< Maximum phase noise (dBc/Hz)
    double maxFrequencyDrift = 1e3;     ///< Maximum frequency drift (Hz)
    double minStability = 0.9;          ///< Minimum signal stability score
    double minQualityScore = 0.7;       ///< Minimum overall quality score
    bool enableAnomalyDetection = true; ///< Enable anomaly detection
    bool enableTrendAnalysis = true;    ///< Enable trend analysis
    size_t minSampleCount = 10;         ///< Minimum samples for trend analysis
};

/**
 * @struct QualityMetrics
 * @brief Comprehensive signal quality metrics
 */
struct QualityMetrics {
    double snrScore;           ///< SNR quality score (0-1)
    double confidenceScore;    ///< Confidence score (0-1)
    double phaseNoiseScore;    ///< Phase noise score (0-1)
    double stabilityScore;     ///< Signal stability score (0-1)
    double anomalyScore;       ///< Anomaly detection score (0-1)
    double trendScore;         ///< Trend analysis score (0-1)
    double overallScore;       ///< Overall quality score (0-1)
    std::map<std::string, double> additionalMetrics; ///< Additional quality metrics
    std::vector<std::string> validationFlags;  ///< Validation warning flags
};

/**
 * @class SignalQualityAnalyzer
 * @brief Analyzes and validates signal quality
 */
class SignalQualityAnalyzer {
public:
    using QualityUpdateCallback = std::function<void(const std::string&, const QualityMetrics&)>;
    using ValidationCallback = std::function<void(const std::string&, const std::vector<std::string>&)>;

    /**
     * @brief Constructor
     * @param config Quality assessment configuration
     */
    explicit SignalQualityAnalyzer(const QualityConfig& config);

    /**
     * @brief Initialize the analyzer
     * @return True if initialization successful
     */
    bool initialize();

    /**
     * @brief Analyze signal quality
     * @param signal Signal to analyze
     * @return Quality metrics
     */
    QualityMetrics analyzeSignal(std::shared_ptr<Signal> signal);

    /**
     * @brief Analyze track quality
     * @param track Signal track to analyze
     * @return Quality metrics
     */
    QualityMetrics analyzeTrack(const Track& track);

    /**
     * @brief Validate signal against quality thresholds
     * @param metrics Quality metrics to validate
     * @return List of validation flags/warnings
     */
    std::vector<std::string> validateQuality(const QualityMetrics& metrics) const;

    /**
     * @brief Set callback for quality updates
     * @param callback Function to call when quality metrics are updated
     */
    void setQualityUpdateCallback(QualityUpdateCallback callback);

    /**
     * @brief Set callback for validation warnings
     * @param callback Function to call when validation flags are generated
     */
    void setValidationCallback(ValidationCallback callback);

    /**
     * @brief Update analyzer configuration
     * @param config New configuration
     */
    void updateConfig(const QualityConfig& config);

    /**
     * @brief Get current configuration
     * @return Current quality configuration
     */
    QualityConfig getConfig() const;

    /**
     * @brief Get analyzer statistics
     * @return Map of statistic name to value
     */
    std::map<std::string, double> getStats() const;

private:
    /**
     * @brief Calculate SNR quality score
     * @param signal Signal to analyze
     * @return SNR score (0-1)
     */
    double calculateSnrScore(std::shared_ptr<Signal> signal) const;

    /**
     * @brief Calculate phase noise score
     * @param signal Signal to analyze
     * @return Phase noise score (0-1)
     */
    double calculatePhaseNoiseScore(std::shared_ptr<Signal> signal) const;

    /**
     * @brief Calculate signal stability score
     * @param track Signal track to analyze
     * @return Stability score (0-1)
     */
    double calculateStabilityScore(const Track& track) const;

    /**
     * @brief Detect signal anomalies
     * @param track Signal track to analyze
     * @return Anomaly score (0-1)
     */
    double detectAnomalies(const Track& track) const;

    /**
     * @brief Analyze signal trends
     * @param track Signal track to analyze
     * @return Trend score (0-1)
     */
    double analyzeTrends(const Track& track) const;

    /**
     * @brief Calculate overall quality score
     * @param metrics Individual quality metrics
     * @return Overall score (0-1)
     */
    double calculateOverallScore(const QualityMetrics& metrics) const;

private:
    QualityConfig config_;
    QualityUpdateCallback qualityCallback_;
    ValidationCallback validationCallback_;
    mutable std::mutex mutex_;
    std::map<std::string, double> stats_;
};

} // namespace signal
} // namespace tdoa 