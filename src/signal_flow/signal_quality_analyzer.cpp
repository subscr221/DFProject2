#include "signal_quality_analyzer.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace tdoa {
namespace signal {

SignalQualityAnalyzer::SignalQualityAnalyzer(const QualityConfig& config)
    : config_(config) {
}

bool SignalQualityAnalyzer::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        // Initialize statistics
        stats_["total_signals_analyzed"] = 0;
        stats_["total_tracks_analyzed"] = 0;
        stats_["signals_passed"] = 0;
        stats_["signals_failed"] = 0;
        stats_["average_quality_score"] = 0.0;
        stats_["processing_time"] = 0.0;

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error initializing SignalQualityAnalyzer: " << e.what() << std::endl;
        return false;
    }
}

QualityMetrics SignalQualityAnalyzer::analyzeSignal(std::shared_ptr<Signal> signal) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto startTime = std::chrono::high_resolution_clock::now();

    QualityMetrics metrics;
    try {
        // Calculate basic quality scores
        metrics.snrScore = calculateSnrScore(signal);
        metrics.confidenceScore = signal->getMetadata("confidence", "0.0");
        metrics.phaseNoiseScore = calculatePhaseNoiseScore(signal);
        
        // Set other scores to neutral values since we don't have track history
        metrics.stabilityScore = 0.5;
        metrics.anomalyScore = 1.0;  // No anomalies detected in single signal
        metrics.trendScore = 0.5;    // Neutral trend for single signal

        // Calculate overall score
        metrics.overallScore = calculateOverallScore(metrics);

        // Validate metrics
        metrics.validationFlags = validateQuality(metrics);

        // Update statistics
        stats_["total_signals_analyzed"]++;
        if (metrics.overallScore >= config_.minQualityScore) {
            stats_["signals_passed"]++;
        } else {
            stats_["signals_failed"]++;
        }

        double totalScore = stats_["average_quality_score"] * (stats_["total_signals_analyzed"] - 1);
        stats_["average_quality_score"] = (totalScore + metrics.overallScore) / 
                                        stats_["total_signals_analyzed"];

        // Notify quality update
        if (qualityCallback_) {
            qualityCallback_(signal->getId(), metrics);
        }

        // Notify validation results
        if (validationCallback_ && !metrics.validationFlags.empty()) {
            validationCallback_(signal->getId(), metrics.validationFlags);
        }

        // Update processing time
        auto endTime = std::chrono::high_resolution_clock::now();
        stats_["processing_time"] = std::chrono::duration<double, std::milli>(
            endTime - startTime).count();

        return metrics;
    }
    catch (const std::exception& e) {
        std::cerr << "Error in analyzeSignal: " << e.what() << std::endl;
        metrics.overallScore = 0.0;
        metrics.validationFlags.push_back("Error: Analysis failed - " + std::string(e.what()));
        return metrics;
    }
}

QualityMetrics SignalQualityAnalyzer::analyzeTrack(const Track& track) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto startTime = std::chrono::high_resolution_clock::now();

    QualityMetrics metrics;
    try {
        if (track.points.empty()) {
            metrics.overallScore = 0.0;
            metrics.validationFlags.push_back("Error: Empty track");
            return metrics;
        }

        // Calculate average SNR and confidence scores
        double totalSnr = 0.0;
        double totalConfidence = 0.0;
        for (const auto& point : track.points) {
            totalSnr += point.snr;
            totalConfidence += point.confidence;
        }
        metrics.snrScore = std::min(1.0, totalSnr / (track.points.size() * config_.minSnr));
        metrics.confidenceScore = totalConfidence / track.points.size();

        // Calculate stability and trend scores
        metrics.stabilityScore = calculateStabilityScore(track);
        metrics.anomalyScore = config_.enableAnomalyDetection ? detectAnomalies(track) : 1.0;
        metrics.trendScore = config_.enableTrendAnalysis ? analyzeTrends(track) : 0.5;

        // Calculate phase noise score from most recent point
        metrics.phaseNoiseScore = track.points.empty() ? 0.0 : 
            std::stod(track.points.back().metadata.at("phase_noise_score"));

        // Calculate overall score
        metrics.overallScore = calculateOverallScore(metrics);

        // Add additional track-specific metrics
        metrics.additionalMetrics["track_duration"] = std::chrono::duration<double>(
            track.points.back().timestamp - track.points.front().timestamp).count();
        metrics.additionalMetrics["point_count"] = track.points.size();
        metrics.additionalMetrics["update_rate"] = track.points.size() / 
            metrics.additionalMetrics["track_duration"];

        // Validate metrics
        metrics.validationFlags = validateQuality(metrics);

        // Update statistics
        stats_["total_tracks_analyzed"]++;
        if (metrics.overallScore >= config_.minQualityScore) {
            stats_["signals_passed"]++;
        } else {
            stats_["signals_failed"]++;
        }

        // Notify quality update
        if (qualityCallback_) {
            qualityCallback_(track.id, metrics);
        }

        // Notify validation results
        if (validationCallback_ && !metrics.validationFlags.empty()) {
            validationCallback_(track.id, metrics.validationFlags);
        }

        // Update processing time
        auto endTime = std::chrono::high_resolution_clock::now();
        stats_["processing_time"] = std::chrono::duration<double, std::milli>(
            endTime - startTime).count();

        return metrics;
    }
    catch (const std::exception& e) {
        std::cerr << "Error in analyzeTrack: " << e.what() << std::endl;
        metrics.overallScore = 0.0;
        metrics.validationFlags.push_back("Error: Analysis failed - " + std::string(e.what()));
        return metrics;
    }
}

std::vector<std::string> SignalQualityAnalyzer::validateQuality(
    const QualityMetrics& metrics) const {
    
    std::vector<std::string> flags;

    // Check SNR
    if (metrics.snrScore < config_.minSnr / 20.0) {  // Normalize to 0-1 scale
        flags.push_back("Low SNR: " + std::to_string(metrics.snrScore * 20.0) + " dB");
    }

    // Check confidence
    if (metrics.confidenceScore < config_.minConfidence) {
        flags.push_back("Low confidence: " + std::to_string(metrics.confidenceScore));
    }

    // Check stability
    if (metrics.stabilityScore < config_.minStability) {
        flags.push_back("Poor stability: " + std::to_string(metrics.stabilityScore));
    }

    // Check for anomalies
    if (metrics.anomalyScore < 0.8) {
        flags.push_back("Anomalies detected: Score " + std::to_string(metrics.anomalyScore));
    }

    // Check overall quality
    if (metrics.overallScore < config_.minQualityScore) {
        flags.push_back("Below quality threshold: " + std::to_string(metrics.overallScore));
    }

    return flags;
}

double SignalQualityAnalyzer::calculateSnrScore(std::shared_ptr<Signal> signal) const {
    try {
        double snr = std::stod(signal->getMetadata("snr", "0.0"));
        return std::min(1.0, snr / config_.minSnr);
    }
    catch (const std::exception&) {
        return 0.0;
    }
}

double SignalQualityAnalyzer::calculatePhaseNoiseScore(std::shared_ptr<Signal> signal) const {
    try {
        double phaseNoise = std::stod(signal->getMetadata("phase_noise", "0.0"));
        // Convert phase noise to score (lower phase noise is better)
        return std::max(0.0, 1.0 - (phaseNoise - config_.maxPhaseNoise) / 40.0);
    }
    catch (const std::exception&) {
        return 0.0;
    }
}

double SignalQualityAnalyzer::calculateStabilityScore(const Track& track) const {
    if (track.points.size() < 2) {
        return 1.0;  // Single point is considered stable
    }

    std::vector<double> freqVariations;
    std::vector<double> powerVariations;
    
    for (size_t i = 1; i < track.points.size(); ++i) {
        const auto& prev = track.points[i-1];
        const auto& curr = track.points[i];
        
        // Calculate frequency and power variations
        double freqDrift = std::abs(curr.frequency - prev.frequency);
        double powerDrift = std::abs(curr.power - prev.power);
        
        freqVariations.push_back(freqDrift);
        powerVariations.push_back(powerDrift);
    }

    // Calculate stability metrics
    double avgFreqDrift = std::accumulate(freqVariations.begin(), freqVariations.end(), 0.0) / 
                         freqVariations.size();
    double avgPowerDrift = std::accumulate(powerVariations.begin(), powerVariations.end(), 0.0) / 
                          powerVariations.size();

    // Convert to stability scores (0-1)
    double freqStability = std::max(0.0, 1.0 - avgFreqDrift / config_.maxFrequencyDrift);
    double powerStability = std::max(0.0, 1.0 - avgPowerDrift / 10.0);  // 10 dB variation threshold

    // Combine scores with equal weight
    return (freqStability + powerStability) / 2.0;
}

double SignalQualityAnalyzer::detectAnomalies(const Track& track) const {
    if (track.points.size() < config_.minSampleCount) {
        return 1.0;  // Not enough points for anomaly detection
    }

    // Calculate moving averages and standard deviations
    std::vector<double> freqValues;
    std::vector<double> powerValues;
    
    for (const auto& point : track.points) {
        freqValues.push_back(point.frequency);
        powerValues.push_back(point.power);
    }

    // Calculate statistics
    double freqMean = std::accumulate(freqValues.begin(), freqValues.end(), 0.0) / freqValues.size();
    double powerMean = std::accumulate(powerValues.begin(), powerValues.end(), 0.0) / powerValues.size();

    double freqStdDev = std::sqrt(std::accumulate(freqValues.begin(), freqValues.end(), 0.0,
        [freqMean](double acc, double val) {
            double diff = val - freqMean;
            return acc + diff * diff;
        }) / freqValues.size());

    double powerStdDev = std::sqrt(std::accumulate(powerValues.begin(), powerValues.end(), 0.0,
        [powerMean](double acc, double val) {
            double diff = val - powerMean;
            return acc + diff * diff;
        }) / powerValues.size());

    // Count anomalies (points beyond 3 standard deviations)
    int anomalyCount = 0;
    for (size_t i = 0; i < track.points.size(); ++i) {
        if (std::abs(freqValues[i] - freqMean) > 3 * freqStdDev ||
            std::abs(powerValues[i] - powerMean) > 3 * powerStdDev) {
            anomalyCount++;
        }
    }

    // Convert to score (0-1, where 1 means no anomalies)
    return std::max(0.0, 1.0 - static_cast<double>(anomalyCount) / track.points.size());
}

double SignalQualityAnalyzer::analyzeTrends(const Track& track) const {
    if (track.points.size() < config_.minSampleCount) {
        return 0.5;  // Not enough points for trend analysis
    }

    // Calculate trends in frequency, power, and SNR
    std::vector<double> freqTrend;
    std::vector<double> powerTrend;
    std::vector<double> snrTrend;

    for (size_t i = 1; i < track.points.size(); ++i) {
        const auto& prev = track.points[i-1];
        const auto& curr = track.points[i];
        
        double dt = std::chrono::duration<double>(curr.timestamp - prev.timestamp).count();
        if (dt > 0) {
            freqTrend.push_back((curr.frequency - prev.frequency) / dt);
            powerTrend.push_back((curr.power - prev.power) / dt);
            snrTrend.push_back((curr.snr - prev.snr) / dt);
        }
    }

    // Calculate trend stability (lower variation is better)
    auto calculateTrendStability = [](const std::vector<double>& trend) {
        if (trend.empty()) return 1.0;
        double mean = std::accumulate(trend.begin(), trend.end(), 0.0) / trend.size();
        double variance = std::accumulate(trend.begin(), trend.end(), 0.0,
            [mean](double acc, double val) {
                double diff = val - mean;
                return acc + diff * diff;
            }) / trend.size();
        return std::exp(-std::sqrt(variance));  // Convert to 0-1 score
    };

    double freqStability = calculateTrendStability(freqTrend);
    double powerStability = calculateTrendStability(powerTrend);
    double snrStability = calculateTrendStability(snrTrend);

    // Combine scores with weights
    return 0.4 * freqStability + 0.3 * powerStability + 0.3 * snrStability;
}

double SignalQualityAnalyzer::calculateOverallScore(const QualityMetrics& metrics) const {
    // Weight factors for different metrics
    const double weights[] = {
        0.25,  // SNR
        0.15,  // Confidence
        0.15,  // Phase noise
        0.20,  // Stability
        0.15,  // Anomaly
        0.10   // Trend
    };

    // Calculate weighted sum
    double score = 
        weights[0] * metrics.snrScore +
        weights[1] * metrics.confidenceScore +
        weights[2] * metrics.phaseNoiseScore +
        weights[3] * metrics.stabilityScore +
        weights[4] * metrics.anomalyScore +
        weights[5] * metrics.trendScore;

    return std::max(0.0, std::min(1.0, score));
}

void SignalQualityAnalyzer::setQualityUpdateCallback(QualityUpdateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    qualityCallback_ = callback;
}

void SignalQualityAnalyzer::setValidationCallback(ValidationCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    validationCallback_ = callback;
}

void SignalQualityAnalyzer::updateConfig(const QualityConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

QualityConfig SignalQualityAnalyzer::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

std::map<std::string, double> SignalQualityAnalyzer::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

} // namespace signal
} // namespace tdoa 