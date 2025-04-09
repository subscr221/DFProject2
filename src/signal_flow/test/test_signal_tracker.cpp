#include "signal_tracker.h"
#include "signal_factory.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

using namespace tdoa::signal;

void printTrackPoint(const TrackPoint& point) {
    std::cout << std::fixed << std::setprecision(3)
              << "Time: " << std::chrono::duration<double>(
                    point.timestamp.time_since_epoch()).count()
              << " s, Freq: " << point.frequency / 1e6 << " MHz"
              << ", BW: " << point.bandwidth / 1e3 << " kHz"
              << ", Power: " << point.power << " dBm"
              << ", SNR: " << point.snr << " dB"
              << ", Conf: " << point.confidence
              << ", Class: " << SignalClassifier::signalClassToString(point.signalClass)
              << std::endl;
}

void printTrack(const Track& track) {
    std::cout << "\nTrack ID: " << track.id
              << "\nActive: " << (track.active ? "Yes" : "No")
              << "\nPrimary Class: " << SignalClassifier::signalClassToString(track.primaryClass)
              << "\nPoints: " << track.points.size()
              << "\nLast Update: " << std::chrono::duration<double>(
                    track.lastUpdate.time_since_epoch()).count() << " s"
              << std::endl;

    std::cout << "\nTrack Points:" << std::endl;
    for (const auto& point : track.points) {
        std::cout << "  ";
        printTrackPoint(point);
    }
    std::cout << std::string(80, '-') << std::endl;
}

void printStats(const std::map<std::string, double>& stats) {
    std::cout << "\nTracker Statistics:" << std::endl;
    std::cout << std::string(40, '-') << std::endl;
    
    for (const auto& [key, value] : stats) {
        std::cout << std::setw(20) << key << ": " 
                  << std::fixed << std::setprecision(3) << value << std::endl;
    }
    std::cout << std::endl;
}

int main() {
    try {
        // Initialize signal flow
        SignalFlow::getInstance().initialize();

        // Create tracking configuration
        TrackingConfig config;
        config.timeWindow = 5.0;
        config.frequencyTolerance = 1e3;
        config.bandwidthTolerance = 0.2;
        config.powerTolerance = 10.0;
        config.maxTracks = 100;
        config.enablePrediction = true;
        config.enableMerging = true;
        config.mergingThreshold = 0.8;

        // Create tracker
        SignalTracker tracker(config);
        if (!tracker.initialize()) {
            std::cerr << "Failed to initialize tracker" << std::endl;
            return 1;
        }

        // Set callbacks
        tracker.setTrackUpdateCallback([](const Track& track) {
            std::cout << "\nTrack Updated:" << std::endl;
            printTrack(track);
        });

        tracker.setTrackEndCallback([](const Track& track) {
            std::cout << "\nTrack Ended:" << std::endl;
            printTrack(track);
        });

        // Create test signals with different characteristics
        std::vector<std::tuple<double, double, double, double>> testSignals = {
            {500e6, 50e3, 15.0, -50.0},    // 500 MHz, 50 kHz BW, 15 dB SNR, -50 dBm
            {1.2e9, 100e3, 12.0, -60.0},   // 1.2 GHz, 100 kHz BW, 12 dB SNR, -60 dBm
            {2.4e9, 20e3, 9.0, -70.0},     // 2.4 GHz, 20 kHz BW, 9 dB SNR, -70 dBm
            {5.8e9, 200e3, 6.0, -80.0}     // 5.8 GHz, 200 kHz BW, 6 dB SNR, -80 dBm
        };

        // Test track creation and updating
        std::cout << "Testing track creation and updating..." << std::endl;
        
        for (int iteration = 0; iteration < 3; ++iteration) {
            std::cout << "\nIteration " << iteration + 1 << ":" << std::endl;
            
            std::vector<DetectedSignal> signals;
            for (const auto& [freq, bw, snr, power] : testSignals) {
                DetectedSignal signal;
                signal.centerFrequency = freq + (iteration * 1e3);  // Slight frequency drift
                signal.bandwidth = bw;
                signal.snr = snr;
                signal.power = power - (iteration * 2.0);  // Power degradation
                signal.confidence = 0.8 - (iteration * 0.1);  // Confidence degradation
                signals.push_back(signal);
            }

            // Update tracks
            auto updatedTracks = tracker.updateTracks(signals);
            std::cout << "Updated " << updatedTracks.size() << " tracks" << std::endl;

            // Print active tracks
            auto activeTracks = tracker.getActiveTracks();
            std::cout << "\nActive Tracks: " << activeTracks.size() << std::endl;
            for (const auto& track : activeTracks) {
                printTrack(*track);
            }

            // Print statistics
            printStats(tracker.getStats());

            // Wait before next iteration
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Test track prediction
        std::cout << "\nTesting track prediction..." << std::endl;
        std::vector<DetectedSignal> emptySignals;
        tracker.updateTracks(emptySignals);  // This will trigger predictions

        // Print active tracks after prediction
        auto activeTracks = tracker.getActiveTracks();
        std::cout << "\nActive Tracks after Prediction: " << activeTracks.size() << std::endl;
        for (const auto& track : activeTracks) {
            printTrack(*track);
        }

        // Test track merging
        std::cout << "\nTesting track merging..." << std::endl;
        std::vector<DetectedSignal> mergingSignals;

        // Create two similar signals that should be merged
        DetectedSignal signal1;
        signal1.centerFrequency = 1.0e9;
        signal1.bandwidth = 100e3;
        signal1.snr = 15.0;
        signal1.power = -50.0;
        signal1.confidence = 0.9;

        DetectedSignal signal2;
        signal2.centerFrequency = 1.001e9;  // Very close to signal1
        signal2.bandwidth = 95e3;
        signal2.snr = 14.0;
        signal2.power = -52.0;
        signal2.confidence = 0.85;

        mergingSignals.push_back(signal1);
        mergingSignals.push_back(signal2);

        // Update tracks with similar signals
        tracker.updateTracks(mergingSignals);

        // Print active tracks after merging
        activeTracks = tracker.getActiveTracks();
        std::cout << "\nActive Tracks after Merging: " << activeTracks.size() << std::endl;
        for (const auto& track : activeTracks) {
            printTrack(*track);
        }

        // Print final statistics
        printStats(tracker.getStats());

        // Shutdown signal flow
        SignalFlow::getInstance().shutdown();

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 