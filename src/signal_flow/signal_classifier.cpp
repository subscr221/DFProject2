#include "signal_classifier.h"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace tdoa {
namespace signal {

SignalClassifier::SignalClassifier(const ClassifierConfig& config)
    : config_(config) {
}

bool SignalClassifier::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        // Create processing chains
        SignalFlow& flow = SignalFlow::getInstance();
        featureChain_ = flow.createChain("FeatureExtraction");
        separationChain_ = flow.createChain("SignalSeparation");

        // Add feature extraction components
        // These would be your actual feature extraction components
        // For example: FFT, power spectrum, constellation mapper, etc.

        // Add signal separation components
        // These would be your actual signal separation components
        // For example: blind source separation, ICA, etc.

        // Initialize statistics
        stats_["total_processed"] = 0;
        stats_["total_classified"] = 0;
        stats_["classification_rate"] = 0;
        stats_["average_confidence"] = 0;
        stats_["processing_time"] = 0;

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error initializing SignalClassifier: " << e.what() << std::endl;
        return false;
    }
}

std::vector<ClassificationResult> SignalClassifier::classifySignals(
    const std::vector<DetectedSignal>& signals) {
    
    if (signals.empty()) {
        return {};
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        std::vector<ClassificationResult> results;
        results.reserve(signals.size());

        // Process each detected signal
        for (const auto& signal : signals) {
            // Extract features
            auto features = extractFeatures(signal);

            // Classify based on features
            auto result = classifyFeatures(features);

            // Add metadata from original signal
            result.metadata["signal_id"] = signal.id;
            result.metadata["detection_confidence"] = std::to_string(signal.confidence);

            // Generate human-readable description
            result.description = generateDescription(result);

            results.push_back(result);
        }

        // Update statistics
        auto endTime = std::chrono::high_resolution_clock::now();
        double processingTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();

        stats_["total_processed"]++;
        stats_["total_classified"] += results.size();
        stats_["classification_rate"] = stats_["total_classified"] / stats_["total_processed"];
        stats_["processing_time"] = processingTime;

        if (!results.empty()) {
            double totalConfidence = 0;
            for (const auto& result : results) {
                totalConfidence += result.probabilities.at(result.signalClass);
            }
            stats_["average_confidence"] = totalConfidence / results.size();
        }

        return results;
    }
    catch (const std::exception& e) {
        std::cerr << "Error in classifySignals: " << e.what() << std::endl;
        return {};
    }
}

bool SignalClassifier::classifySignalsAsync(
    const std::vector<DetectedSignal>& signals,
    ClassificationCallback callback) {
    
    if (signals.empty() || !callback) {
        return false;
    }

    try {
        // Submit async classification task
        SignalFlow::getInstance().getParallelEngine().submitTask(
            nullptr,  // No input signal needed since we have DetectedSignals
            [this, signals, callback]() {
                auto results = this->classifySignals(signals);
                callback(results);
                return nullptr;
            },
            TaskPriority::NORMAL
        );

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error in classifySignalsAsync: " << e.what() << std::endl;
        return false;
    }
}

SignalFeatures SignalClassifier::extractFeatures(const DetectedSignal& signal) {
    SignalFeatures features;

    try {
        // Copy basic features from detected signal
        features.bandwidth = signal.bandwidth;
        features.centerFrequency = signal.centerFrequency;
        features.snr = signal.snr;

        // Extract additional features using the feature chain
        if (featureChain_) {
            // This would be your actual feature extraction implementation
            // For example:
            // 1. Calculate power spectrum using FFT
            // 2. Estimate modulation parameters
            // 3. Extract constellation points
            // 4. Measure symbol rate
            // 5. Calculate modulation index

            // For now, this is a placeholder implementation
            features.peakPower = -30.0;  // dBm
            features.averagePower = -40.0;  // dBm
            features.modulationIndex = 0.8;
            features.symbolRate = 1e6;  // 1 MHz
            
            // Generate placeholder constellation
            features.constellation = {1.0, 0.0, -1.0, 0.0};  // QPSK-like

            // Generate placeholder spectrum
            features.spectrum.resize(config_.fftSize);
            for (size_t i = 0; i < config_.fftSize; ++i) {
                features.spectrum[i] = -50.0;  // dBm
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error in extractFeatures: " << e.what() << std::endl;
    }

    return features;
}

ClassificationResult SignalClassifier::classifyFeatures(const SignalFeatures& features) {
    ClassificationResult result;
    result.features = features;

    try {
        // Initialize probabilities for all classes
        for (int i = 0; i < static_cast<int>(SignalClass::INTERFERENCE) + 1; ++i) {
            result.probabilities[static_cast<SignalClass>(i)] = 0.0;
        }

        // This would be your actual classification logic
        // For example:
        // 1. Apply decision tree based on features
        // 2. Use machine learning model if enabled
        // 3. Calculate confidence scores
        // 4. Select best matching class

        // For now, this is a placeholder implementation
        // Classify based on simple feature thresholds
        if (features.snr < config_.minSnr) {
            result.signalClass = SignalClass::NOISE;
            result.probabilities[SignalClass::NOISE] = 0.9;
        }
        else if (features.modulationIndex > 0.7) {
            result.signalClass = SignalClass::FM;
            result.probabilities[SignalClass::FM] = 0.8;
        }
        else if (features.constellation.size() == 4) {
            result.signalClass = SignalClass::PSK;
            result.probabilities[SignalClass::PSK] = 0.85;
        }
        else {
            result.signalClass = SignalClass::UNKNOWN;
            result.probabilities[SignalClass::UNKNOWN] = 0.6;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error in classifyFeatures: " << e.what() << std::endl;
        result.signalClass = SignalClass::UNKNOWN;
        result.probabilities[SignalClass::UNKNOWN] = 1.0;
    }

    return result;
}

std::vector<std::shared_ptr<Signal>> SignalClassifier::separateSignals(
    std::shared_ptr<Signal> signal) {
    
    std::vector<std::shared_ptr<Signal>> separatedSignals;

    try {
        if (separationChain_ && signal) {
            // This would be your actual signal separation implementation
            // For example:
            // 1. Apply blind source separation
            // 2. Use ICA or similar algorithms
            // 3. Validate separated signals
            // 4. Filter out noise components

            // For now, this is a placeholder that just returns the input signal
            separatedSignals.push_back(signal);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error in separateSignals: " << e.what() << std::endl;
    }

    return separatedSignals;
}

std::string SignalClassifier::generateDescription(const ClassificationResult& result) {
    std::stringstream ss;

    try {
        // Generate human-readable description
        ss << "Signal classified as " << signalClassToString(result.signalClass)
           << " with " << (result.probabilities.at(result.signalClass) * 100.0) << "% confidence. "
           << "Center frequency: " << (result.features.centerFrequency / 1e6) << " MHz, "
           << "Bandwidth: " << (result.features.bandwidth / 1e3) << " kHz, "
           << "SNR: " << result.features.snr << " dB";

        if (result.signalClass != SignalClass::UNKNOWN && 
            result.signalClass != SignalClass::NOISE) {
            ss << ", Symbol rate: " << (result.features.symbolRate / 1e3) << " ksps";
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error in generateDescription: " << e.what() << std::endl;
        ss << "Error generating description";
    }

    return ss.str();
}

void SignalClassifier::updateConfig(const ClassifierConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

ClassifierConfig SignalClassifier::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

std::map<std::string, double> SignalClassifier::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

std::string SignalClassifier::signalClassToString(SignalClass signalClass) {
    switch (signalClass) {
        case SignalClass::AM: return "AM";
        case SignalClass::FM: return "FM";
        case SignalClass::PSK: return "PSK";
        case SignalClass::QAM: return "QAM";
        case SignalClass::FSK: return "FSK";
        case SignalClass::OFDM: return "OFDM";
        case SignalClass::NOISE: return "Noise";
        case SignalClass::INTERFERENCE: return "Interference";
        case SignalClass::UNKNOWN:
        default: return "Unknown";
    }
}

} // namespace signal
} // namespace tdoa 