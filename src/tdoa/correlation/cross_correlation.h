/**
 * @file cross_correlation.h
 * @brief Cross-correlation algorithm with sub-sample interpolation
 */

#pragma once

#include <vector>
#include <complex>
#include <memory>
#include <string>
#include <functional>

namespace tdoa {
namespace correlation {

/**
 * @enum WindowType
 * @brief Window functions for signal processing
 */
enum class WindowType {
    None,           ///< No windowing
    Hamming,        ///< Hamming window
    Hanning,        ///< Hanning window
    Blackman,       ///< Blackman window
    BlackmanHarris, ///< Blackman-Harris window
    FlatTop         ///< Flat-top window
};

/**
 * @enum InterpolationType
 * @brief Interpolation methods for sub-sample precision
 */
enum class InterpolationType {
    None,           ///< No interpolation
    Parabolic,      ///< Parabolic interpolation
    Cubic,          ///< Cubic interpolation
    Gaussian,       ///< Gaussian interpolation
    Sinc            ///< Sinc interpolation
};

/**
 * @struct CorrelationPeak
 * @brief Structure to hold correlation peak information
 */
struct CorrelationPeak {
    double delay;           ///< Time delay in samples (can be fractional)
    double coefficient;     ///< Correlation coefficient
    double confidence;      ///< Confidence value (0-1)
    double snr;             ///< Signal-to-noise ratio estimate
};

/**
 * @struct CorrelationResult
 * @brief Structure to hold correlation results
 */
struct CorrelationResult {
    std::vector<double> correlation;        ///< Full correlation result
    std::vector<CorrelationPeak> peaks;     ///< Detected peaks
    double sampleRate;                      ///< Sample rate in Hz
    double maxPeakConfidence;               ///< Maximum peak confidence
};

/**
 * @struct CorrelationConfig
 * @brief Configuration for correlation algorithm
 */
struct CorrelationConfig {
    WindowType windowType;                   ///< Window function type
    InterpolationType interpolationType;     ///< Interpolation method
    double peakThreshold;                    ///< Threshold for peak detection (0-1)
    int maxPeaks;                            ///< Maximum number of peaks to detect
    bool normalizeOutput;                    ///< Whether to normalize correlation output
    double sampleRate;                       ///< Sample rate in Hz
    double minSnr;                           ///< Minimum SNR for valid peaks
    
    /**
     * @brief Constructor with default values
     */
    CorrelationConfig()
        : windowType(WindowType::Hamming)
        , interpolationType(InterpolationType::Parabolic)
        , peakThreshold(0.5)
        , maxPeaks(3)
        , normalizeOutput(true)
        , sampleRate(1.0)
        , minSnr(3.0)
    {}
};

/**
 * @brief Cross-correlate two real signals
 * 
 * @param signal1 First signal
 * @param signal2 Second signal
 * @param config Correlation configuration
 * @return CorrelationResult with correlation and detected peaks
 */
CorrelationResult crossCorrelate(
    const std::vector<double>& signal1,
    const std::vector<double>& signal2,
    const CorrelationConfig& config = CorrelationConfig());

/**
 * @brief Cross-correlate two complex signals
 * 
 * @param signal1 First signal
 * @param signal2 Second signal
 * @param config Correlation configuration
 * @return CorrelationResult with correlation and detected peaks
 */
CorrelationResult crossCorrelate(
    const std::vector<std::complex<double>>& signal1,
    const std::vector<std::complex<double>>& signal2,
    const CorrelationConfig& config = CorrelationConfig());

/**
 * @brief Apply window function to a signal
 * 
 * @param signal Signal to apply window to
 * @param windowType Window function type
 * @return Windowed signal
 */
std::vector<double> applyWindow(
    const std::vector<double>& signal,
    WindowType windowType);

/**
 * @brief Apply window function to a complex signal
 * 
 * @param signal Signal to apply window to
 * @param windowType Window function type
 * @return Windowed signal
 */
std::vector<std::complex<double>> applyWindow(
    const std::vector<std::complex<double>>& signal,
    WindowType windowType);

/**
 * @brief Generate window coefficients
 * 
 * @param length Window length
 * @param windowType Window function type
 * @return Window coefficients
 */
std::vector<double> generateWindow(int length, WindowType windowType);

/**
 * @brief Interpolate around correlation peak for sub-sample precision
 * 
 * @param correlation Correlation array
 * @param peakIndex Index of the peak
 * @param interpolationType Interpolation method
 * @return Interpolated peak information
 */
CorrelationPeak interpolatePeak(
    const std::vector<double>& correlation,
    int peakIndex,
    InterpolationType interpolationType);

/**
 * @brief Find peaks in correlation result
 * 
 * @param correlation Correlation array
 * @param peakThreshold Threshold for peak detection (0-1)
 * @param maxPeaks Maximum number of peaks to detect
 * @param interpolationType Interpolation method
 * @return Vector of detected peaks
 */
std::vector<CorrelationPeak> findPeaks(
    const std::vector<double>& correlation,
    double peakThreshold,
    int maxPeaks,
    InterpolationType interpolationType);

/**
 * @brief Estimate SNR of a correlation peak
 * 
 * @param correlation Correlation array
 * @param peakIndex Index of the peak
 * @param windowSize Window size for noise estimation
 * @return Estimated SNR in linear scale
 */
double estimatePeakSnr(
    const std::vector<double>& correlation,
    int peakIndex,
    int windowSize = 20);

/**
 * @brief Calculate confidence metric for a correlation peak
 * 
 * @param peak Peak information
 * @param correlation Full correlation result
 * @return Confidence value (0-1)
 */
double calculatePeakConfidence(
    const CorrelationPeak& peak,
    const std::vector<double>& correlation);

/**
 * @brief Normalize correlation result to range [-1, 1]
 * 
 * @param correlation Correlation array to normalize
 * @return Normalized correlation
 */
std::vector<double> normalizeCorrelation(const std::vector<double>& correlation);

/**
 * @brief Convert time delay from samples to seconds
 * 
 * @param delaySamples Delay in samples
 * @param sampleRate Sample rate in Hz
 * @return Delay in seconds
 */
double samplesToTime(double delaySamples, double sampleRate);

/**
 * @brief Convert time delay from seconds to samples
 * 
 * @param delaySeconds Delay in seconds
 * @param sampleRate Sample rate in Hz
 * @return Delay in samples
 */
double timeToSamples(double delaySeconds, double sampleRate);

/**
 * @class SegmentedCorrelator
 * @brief Class for processing continuous signals in segments
 */
class SegmentedCorrelator {
public:
    /**
     * @brief Constructor
     * @param config Correlation configuration
     * @param segmentSize Size of each segment
     * @param overlapFactor Overlap factor between segments (0-1)
     */
    SegmentedCorrelator(
        const CorrelationConfig& config = CorrelationConfig(),
        int segmentSize = 1024,
        double overlapFactor = 0.5);
    
    /**
     * @brief Process a new segment of data
     * @param segment1 Segment from first signal
     * @param segment2 Segment from second signal
     * @return Correlation result for this segment
     */
    CorrelationResult processSegment(
        const std::vector<double>& segment1,
        const std::vector<double>& segment2);
    
    /**
     * @brief Process a new segment of data (complex version)
     * @param segment1 Segment from first signal
     * @param segment2 Segment from second signal
     * @return Correlation result for this segment
     */
    CorrelationResult processSegment(
        const std::vector<std::complex<double>>& segment1,
        const std::vector<std::complex<double>>& segment2);
    
    /**
     * @brief Reset the correlator state
     */
    void reset();
    
    /**
     * @brief Set callback for new correlation results
     * @param callback Function to call with new results
     */
    void setResultCallback(std::function<void(const CorrelationResult&)> callback);
    
    /**
     * @brief Get configuration
     * @return Current configuration
     */
    CorrelationConfig getConfig() const;
    
    /**
     * @brief Set configuration
     * @param config New configuration
     */
    void setConfig(const CorrelationConfig& config);
    
private:
    CorrelationConfig config_;
    int segmentSize_;
    double overlapFactor_;
    std::function<void(const CorrelationResult&)> resultCallback_;
    
    // Previous segments for overlap
    std::vector<double> prevSegment1_;
    std::vector<double> prevSegment2_;
    std::vector<std::complex<double>> prevComplexSegment1_;
    std::vector<std::complex<double>> prevComplexSegment2_;
    bool usingComplex_;
};

} // namespace correlation
} // namespace tdoa 