#include "signal_classifier.h"
#include "parallel_signal_detector.h"
#include "signal_factory.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>

using namespace tdoa::signal;

void printClassificationResults(const std::vector<ClassificationResult>& results) {
    std::cout << "\nClassification Results:" << std::endl;
    std::cout << std::setw(10) << "Signal ID"
              << std::setw(15) << "Class"
              << std::setw(12) << "Confidence"
              << std::setw(15) << "Frequency"
              << std::setw(12) << "Bandwidth"
              << std::setw(8) << "SNR" << std::endl;
    std::cout << std::string(75, '-') << std::endl;

    for (const auto& result : results) {
        std::cout << std::setw(10) << result.metadata.at("signal_id").substr(0, 8)
                  << std::setw(15) << SignalClassifier::signalClassToString(result.signalClass)
                  << std::setw(12) << std::fixed << std::setprecision(2)
                  << result.probabilities.at(result.signalClass) * 100.0
                  << std::setw(15) << std::fixed << std::setprecision(3)
                  << result.features.centerFrequency / 1e6
                  << std::setw(12) << result.features.bandwidth / 1e3
                  << std::setw(8) << std::fixed << std::setprecision(1)
                  << result.features.snr << std::endl;

        // Print detailed description
        std::cout << "\nDescription: " << result.description << std::endl;

        // Print all class probabilities
        std::cout << "Class probabilities:" << std::endl;
        for (const auto& [signalClass, probability] : result.probabilities) {
            if (probability > 0.01) {  // Only show significant probabilities
                std::cout << "  " << std::setw(12) << SignalClassifier::signalClassToString(signalClass)
                          << ": " << std::fixed << std::setprecision(1)
                          << probability * 100.0 << "%" << std::endl;
            }
        }
        std::cout << std::string(75, '-') << std::endl;
    }
}

void printStats(const std::map<std::string, double>& stats) {
    std::cout << "\nClassifier Statistics:" << std::endl;
    std::cout << std::string(40, '-') << std::endl;
    
    for (const auto& [key, value] : stats) {
        std::cout << std::setw(20) << key << ": " 
                  << std::fixed << std::setprecision(3) << value << std::endl;
    }
}

int main() {
    try {
        // Initialize signal flow
        SignalFlow::getInstance().initialize();

        // Create detector configuration
        DetectionConfig detConfig;
        detConfig.minFrequency = 100e6;    // 100 MHz
        detConfig.maxFrequency = 6e9;      // 6 GHz
        detConfig.minBandwidth = 10e3;     // 10 kHz
        detConfig.minSnr = 6.0;            // 6 dB
        detConfig.detectionThreshold = 0.7;
        detConfig.maxSignals = 10;
        detConfig.enableSignalTracking = true;

        // Create classifier configuration
        ClassifierConfig classConfig;
        classConfig.minConfidence = 0.7;
        classConfig.enableAutoThreshold = true;
        classConfig.fftSize = 2048;
        classConfig.minSnr = 6.0;

        // Create detector and classifier
        ParallelSignalDetector detector(detConfig);
        SignalClassifier classifier(classConfig);

        if (!detector.initialize() || !classifier.initialize()) {
            std::cerr << "Failed to initialize detector or classifier" << std::endl;
            return 1;
        }

        // Create test signals with different modulation types
        std::vector<std::tuple<double, double, double, std::string>> testSignals = {
            {500e6, 50e3, 15.0, "AM"},     // AM signal
            {1.2e9, 100e3, 12.0, "FM"},    // FM signal
            {2.4e9, 20e3, 9.0, "PSK"},     // PSK signal
            {5.8e9, 200e3, 6.0, "QAM"}     // QAM signal
        };

        // Create signal segments
        auto signal = SignalFactory::createMultiCarrierSignal(
            DataFormat::ComplexFloat32,
            8192,           // samples
            10e6,          // 10 MHz sample rate
            3e9,           // 3 GHz center frequency
            6e9,           // 6 GHz bandwidth
            {500e6, 1.2e9, 2.4e9, 5.8e9},  // carrier frequencies
            {0.8, 0.6, 0.4, 0.2}           // relative amplitudes
        );

        // Test synchronous processing
        std::cout << "Testing synchronous processing..." << std::endl;
        
        // First detect signals
        auto detectedSignals = detector.processSegment(signal);
        std::cout << "\nDetected " << detectedSignals.size() << " signals" << std::endl;

        // Then classify them
        auto classificationResults = classifier.classifySignals(detectedSignals);
        printClassificationResults(classificationResults);
        printStats(classifier.getStats());

        // Test asynchronous processing
        std::cout << "\nTesting asynchronous processing..." << std::endl;
        bool asyncStarted = classifier.classifySignalsAsync(
            detectedSignals,
            [](const std::vector<ClassificationResult>& results) {
                printClassificationResults(results);
            });

        if (!asyncStarted) {
            std::cerr << "Failed to start async processing" << std::endl;
            return 1;
        }

        // Wait for async processing to complete
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Test continuous classification
        std::cout << "\nTesting continuous classification..." << std::endl;
        for (int i = 0; i < 3; ++i) {
            auto detectedSignals = detector.processSegment(signal);
            auto results = classifier.classifySignals(detectedSignals);
            std::cout << "\nIteration " << i + 1 << ":" << std::endl;
            printClassificationResults(results);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Print final statistics
        printStats(classifier.getStats());

        // Shutdown signal flow
        SignalFlow::getInstance().shutdown();

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 