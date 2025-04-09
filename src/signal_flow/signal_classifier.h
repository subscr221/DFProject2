#pragma once

#include "signal.h"
#include "signal_flow.h"
#include "signal_metadata.h"
#include "processing_component.h"
#include "parallel_signal_detector.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>

namespace tdoa {
namespace signal {

/**
 * @struct SignalFeatures
 * @brief Features extracted from a signal for classification
 */
struct SignalFeatures {
    double bandwidth;                    ///< Signal bandwidth (Hz)
    double centerFrequency;             ///< Center frequency (Hz)
    double peakPower;                   ///< Peak power (dBm)
    double averagePower;                ///< Average power (dBm)
    double snr;                         ///< Signal-to-noise ratio (dB)
    double modulationIndex;             ///< Modulation index
    double symbolRate;                  ///< Symbol rate (Hz)
    std::vector<double> constellation;   ///< Signal constellation points
    std::vector<double> spectrum;       ///< Power spectrum
    std::map<std::string, double> additionalFeatures; ///< Additional extracted features
};

/**
 * @enum SignalClass
 * @brief Enumeration of signal classes
 */
enum class SignalClass {
    UNKNOWN,
    AM,
    FM,
    PSK,
    QAM,
    FSK,
    OFDM,
    NOISE,
    INTERFERENCE
};

/**
 * @struct ClassificationResult
 * @brief Result of signal classification
 */
struct ClassificationResult {
    SignalClass signalClass;                    ///< Determined signal class
    std::map<SignalClass, double> probabilities; ///< Classification probabilities for each class
    SignalFeatures features;                    ///< Extracted signal features
    std::string description;                    ///< Human-readable description
    std::map<std::string, std::string> metadata; ///< Additional metadata
};

/**
 * @struct ClassifierConfig
 * @brief Configuration for signal classification
 */
struct ClassifierConfig {
    double minConfidence = 0.7;         ///< Minimum confidence for classification
    bool enableAutoThreshold = true;    ///< Enable automatic threshold adjustment
    size_t fftSize = 2048;             ///< FFT size for feature extraction
    double minSnr = 6.0;               ///< Minimum SNR for reliable classification
    bool enableDeepLearning = false;    ///< Enable deep learning based classification
    std::string modelPath = "";         ///< Path to deep learning model (if enabled)
};

/**
 * @class SignalClassifier
 * @brief Handles signal separation and classification
 */
class SignalClassifier {
public:
    using ClassificationCallback = std::function<void(const std::vector<ClassificationResult>&)>;

    /**
     * @brief Constructor
     * @param config Classifier configuration
     */
    explicit SignalClassifier(const ClassifierConfig& config);

    /**
     * @brief Initialize the classifier
     * @return True if initialization successful
     */
    bool initialize();

    /**
     * @brief Process detected signals for classification
     * @param signals Vector of detected signals
     * @return Vector of classification results
     */
    std::vector<ClassificationResult> classifySignals(
        const std::vector<DetectedSignal>& signals
    );

    /**
     * @brief Process signals asynchronously
     * @param signals Vector of detected signals
     * @param callback Callback for classification results
     * @return True if processing started successfully
     */
    bool classifySignalsAsync(
        const std::vector<DetectedSignal>& signals,
        ClassificationCallback callback
    );

    /**
     * @brief Update classifier configuration
     * @param config New configuration
     */
    void updateConfig(const ClassifierConfig& config);

    /**
     * @brief Get current configuration
     * @return Current classifier configuration
     */
    ClassifierConfig getConfig() const;

    /**
     * @brief Get classifier statistics
     * @return Map of statistic name to value
     */
    std::map<std::string, double> getStats() const;

    /**
     * @brief Convert signal class to string
     * @param signalClass Signal class to convert
     * @return String representation
     */
    static std::string signalClassToString(SignalClass signalClass);

private:
    /**
     * @brief Extract features from a signal
     * @param signal Input signal
     * @return Extracted features
     */
    SignalFeatures extractFeatures(const DetectedSignal& signal);

    /**
     * @brief Classify signal based on features
     * @param features Signal features
     * @return Classification result
     */
    ClassificationResult classifyFeatures(const SignalFeatures& features);

    /**
     * @brief Separate overlapping signals
     * @param signal Input signal containing multiple signals
     * @return Vector of separated signals
     */
    std::vector<std::shared_ptr<Signal>> separateSignals(std::shared_ptr<Signal> signal);

    /**
     * @brief Generate signal description
     * @param result Classification result
     * @return Human-readable description
     */
    std::string generateDescription(const ClassificationResult& result);

private:
    ClassifierConfig config_;
    std::unique_ptr<ProcessingChain> featureChain_;
    std::unique_ptr<ProcessingChain> separationChain_;
    mutable std::mutex mutex_;
    std::map<std::string, double> stats_;
};

} // namespace signal
} // namespace tdoa 