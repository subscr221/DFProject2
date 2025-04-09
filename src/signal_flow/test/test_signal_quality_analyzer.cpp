#include "../signal_quality_analyzer.h"
#include "../parallel_signal_detector.h"
#include "../signal_factory.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <sstream>

using namespace tdoa::signal;

// Helper function to print quality metrics in a formatted way
void printQualityMetrics(const std::string& label, const QualityMetrics& metrics) {
    std::cout << "\n=== " << label << " ===\n";
    std::cout << std::fixed << std::setprecision(3)
              << "SNR Score:        " << metrics.snrScore << "\n"
              << "Confidence Score: " << metrics.confidenceScore << "\n"
              << "Phase Noise:      " << metrics.phaseNoiseScore << "\n"
              << "Stability:        " << metrics.stabilityScore << "\n"
              << "Anomaly Score:    " << metrics.anomalyScore << "\n"
              << "Trend Score:      " << metrics.trendScore << "\n"
              << "Overall Score:    " << metrics.overallScore << "\n";

    if (!metrics.validationFlags.empty()) {
        std::cout << "\nValidation Flags:\n";
        for (const auto& flag : metrics.validationFlags) {
            std::cout << "- " << flag << "\n";
        }
    }

    if (!metrics.additionalMetrics.empty()) {
        std::cout << "\nAdditional Metrics:\n";
        for (const auto& [key, value] : metrics.additionalMetrics) {
            std::cout << "- " << key << ": " << value << "\n";
        }
    }
}

// Helper function to print analyzer statistics
void printStats(const std::map<std::string, double>& stats) {
    std::cout << "\n=== Analyzer Statistics ===\n";
    std::cout << std::fixed << std::setprecision(2);
    for (const auto& [key, value] : stats) {
        std::cout << std::setw(25) << std::left << key << ": " << value << "\n";
    }
}

// Callback function for quality updates
void onQualityUpdate(const std::string& id, const QualityMetrics& metrics) {
    std::cout << "\nQuality Update for " << id << ":\n";
    std::cout << "Overall Score: " << metrics.overallScore << "\n";
}

// Callback function for validation warnings
void onValidationWarning(const std::string& id, const std::vector<std::string>& flags) {
    std::cout << "\nValidation Warning for " << id << ":\n";
    for (const auto& flag : flags) {
        std::cout << "- " << flag << "\n";
    }
}

int main() {
    try {
        std::cout << "Initializing Signal Quality Analyzer Test...\n";

        // Initialize signal flow system
        if (!initializeSignalFlow()) {
            std::cerr << "Failed to initialize signal flow system\n";
            return 1;
        }

        // Create quality analyzer configuration
        QualityConfig config;
        config.minSnr = 10.0;           // Minimum SNR in dB
        config.minConfidence = 0.7;      // Minimum confidence score
        config.maxPhaseNoise = -90.0;    // Maximum phase noise in dBc/Hz
        config.maxFrequencyDrift = 100.0; // Maximum frequency drift in Hz
        config.minStability = 0.8;       // Minimum stability score
        config.minQualityScore = 0.7;    // Minimum overall quality score
        config.minSampleCount = 5;       // Minimum samples for analysis
        config.enableAnomalyDetection = true;
        config.enableTrendAnalysis = true;

        // Create signal quality analyzer
        SignalQualityAnalyzer analyzer(config);
        if (!analyzer.initialize()) {
            std::cerr << "Failed to initialize SignalQualityAnalyzer\n";
            return 1;
        }

        // Set callbacks
        analyzer.setQualityUpdateCallback(onQualityUpdate);
        analyzer.setValidationCallback(onValidationWarning);

        std::cout << "\nTesting Single Signal Analysis...\n";

        // Create a test signal with good quality
        auto goodSignal = std::make_shared<Signal>();
        goodSignal->setId("test_signal_1");
        goodSignal->setMetadata("snr", "15.0");
        goodSignal->setMetadata("confidence", "0.85");
        goodSignal->setMetadata("phase_noise", "-95.0");

        // Analyze good signal
        auto goodMetrics = analyzer.analyzeSignal(goodSignal);
        printQualityMetrics("Good Signal Analysis", goodMetrics);

        // Create a test signal with poor quality
        auto poorSignal = std::make_shared<Signal>();
        poorSignal->setId("test_signal_2");
        poorSignal->setMetadata("snr", "5.0");
        poorSignal->setMetadata("confidence", "0.45");
        poorSignal->setMetadata("phase_noise", "-70.0");

        // Analyze poor signal
        auto poorMetrics = analyzer.analyzeSignal(poorSignal);
        printQualityMetrics("Poor Signal Analysis", poorMetrics);

        std::cout << "\nTesting Track Analysis...\n";

        // Create a test track with stable characteristics
        Track stableTrack;
        stableTrack.id = "test_track_1";
        stableTrack.primaryClass = SignalClass::AM;
        stableTrack.classConfidence = 0.9;
        stableTrack.active = true;

        // Add track points with stable characteristics
        auto now = std::chrono::system_clock::now();
        for (int i = 0; i < 10; ++i) {
            TrackPoint point;
            point.timestamp = now + std::chrono::milliseconds(i * 100);
            point.frequency = 1000.0 + (rand() % 10 - 5);  // Small random variations
            point.bandwidth = 200.0 + (rand() % 4 - 2);
            point.power = -50.0 + (rand() % 2 - 1);
            point.snr = 15.0 + (rand() % 2 - 1);
            point.confidence = 0.85 + (rand() % 10 - 5) * 0.01;
            point.metadata["phase_noise_score"] = "-95.0";
            stableTrack.points.push_back(point);
        }

        // Analyze stable track
        auto stableMetrics = analyzer.analyzeTrack(stableTrack);
        printQualityMetrics("Stable Track Analysis", stableMetrics);

        // Create a test track with unstable characteristics
        Track unstableTrack;
        unstableTrack.id = "test_track_2";
        unstableTrack.primaryClass = SignalClass::FM;
        unstableTrack.classConfidence = 0.6;
        unstableTrack.active = true;

        // Add track points with unstable characteristics
        for (int i = 0; i < 10; ++i) {
            TrackPoint point;
            point.timestamp = now + std::chrono::milliseconds(i * 100);
            point.frequency = 1000.0 + (rand() % 200 - 100);  // Large random variations
            point.bandwidth = 200.0 + (rand() % 40 - 20);
            point.power = -50.0 + (rand() % 20 - 10);
            point.snr = 8.0 + (rand() % 10 - 5);
            point.confidence = 0.5 + (rand() % 20 - 10) * 0.01;
            point.metadata["phase_noise_score"] = "-75.0";
            unstableTrack.points.push_back(point);
        }

        // Analyze unstable track
        auto unstableMetrics = analyzer.analyzeTrack(unstableTrack);
        printQualityMetrics("Unstable Track Analysis", unstableMetrics);

        // Print final statistics
        printStats(analyzer.getStats());

        std::cout << "\nSignal Quality Analyzer Test Completed Successfully\n";
        shutdownSignalFlow();
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error in test: " << e.what() << std::endl;
        shutdownSignalFlow();
        return 1;
    }
} 