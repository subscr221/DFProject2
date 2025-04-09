#include "parallel_signal_detector.h"
#include "signal_factory.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <iomanip>

using namespace tdoa::signal;

void printDetectedSignals(const std::vector<DetectedSignal>& signals) {
    std::cout << "\nDetected Signals:" << std::endl;
    std::cout << std::setw(10) << "ID" 
              << std::setw(15) << "Frequency" 
              << std::setw(12) << "Bandwidth"
              << std::setw(8) << "SNR"
              << std::setw(12) << "Confidence" << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    for (const auto& signal : signals) {
        std::cout << std::setw(10) << signal.id.substr(0, 8)
                  << std::setw(15) << std::fixed << std::setprecision(3) << signal.centerFrequency / 1e6
                  << std::setw(12) << signal.bandwidth / 1e3
                  << std::setw(8) << std::fixed << std::setprecision(1) << signal.snr
                  << std::setw(12) << std::fixed << std::setprecision(3) << signal.confidence
                  << std::endl;
    }
}

void printStats(const std::map<std::string, double>& stats) {
    std::cout << "\nDetector Statistics:" << std::endl;
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
        DetectionConfig config;
        config.minFrequency = 100e6;    // 100 MHz
        config.maxFrequency = 6e9;      // 6 GHz
        config.minBandwidth = 10e3;     // 10 kHz
        config.minSnr = 6.0;            // 6 dB
        config.detectionThreshold = 0.7;
        config.maxSignals = 10;
        config.enableSignalTracking = true;
        config.trackingTimeWindow = 1.0;
        config.frequencyTolerance = 1e3;
        config.bandwidthTolerance = 0.2;

        // Create detector
        ParallelSignalDetector detector(config);
        if (!detector.initialize()) {
            std::cerr << "Failed to initialize detector" << std::endl;
            return 1;
        }

        // Create test signals
        std::vector<std::tuple<double, double, double>> testSignals = {
            {500e6, 50e3, 15.0},    // 500 MHz carrier, 50 kHz bandwidth, 15 dB SNR
            {1.2e9, 100e3, 12.0},   // 1.2 GHz carrier, 100 kHz bandwidth, 12 dB SNR
            {2.4e9, 20e3, 9.0},     // 2.4 GHz carrier, 20 kHz bandwidth, 9 dB SNR
            {5.8e9, 200e3, 6.0}     // 5.8 GHz carrier, 200 kHz bandwidth, 6 dB SNR
        };

        // Create signal segment with multiple carriers
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
        auto detectedSignals = detector.processSegment(signal);
        printDetectedSignals(detectedSignals);
        printStats(detector.getStats());

        // Test asynchronous processing
        std::cout << "\nTesting asynchronous processing..." << std::endl;
        bool asyncStarted = detector.processSegmentAsync(signal, 
            [](const std::vector<DetectedSignal>& signals) {
                printDetectedSignals(signals);
            });

        if (!asyncStarted) {
            std::cerr << "Failed to start async processing" << std::endl;
            return 1;
        }

        // Wait for async processing to complete
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Test signal tracking
        std::cout << "\nTesting signal tracking..." << std::endl;
        for (int i = 0; i < 3; ++i) {
            auto detectedSignals = detector.processSegment(signal);
            std::cout << "\nIteration " << i + 1 << ":" << std::endl;
            printDetectedSignals(detectedSignals);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Print final statistics
        printStats(detector.getStats());

        // Shutdown signal flow
        SignalFlow::getInstance().shutdown();

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 