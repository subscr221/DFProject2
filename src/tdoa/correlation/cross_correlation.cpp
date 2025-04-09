/**
 * @file cross_correlation.cpp
 * @brief Implementation of cross-correlation functions
 */

#include "cross_correlation.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <complex>
#include <vector>

namespace tdoa {
namespace correlation {

// Helper function for direct cross-correlation calculation
std::vector<double> directCrossCorrelation(
    const std::vector<double>& signal1,
    const std::vector<double>& signal2) {
    
    const int n1 = static_cast<int>(signal1.size());
    const int n2 = static_cast<int>(signal2.size());
    const int resultSize = n1 + n2 - 1;
    
    std::vector<double> result(resultSize, 0.0);
    
    // Cross-correlation: r[k] = sum(x[n] * y[n+k]) for all valid n
    for (int k = 0; k < resultSize; ++k) {
        for (int n = 0; n < n1; ++n) {
            const int index = k - n + n2 - 1;
            if (index >= 0 && index < n2) {
                result[k] += signal1[n] * signal2[index];
            }
        }
    }
    
    return result;
}

// Helper function for direct cross-correlation calculation with complex signals
std::vector<double> directCrossCorrelation(
    const std::vector<std::complex<double>>& signal1,
    const std::vector<std::complex<double>>& signal2) {
    
    const int n1 = static_cast<int>(signal1.size());
    const int n2 = static_cast<int>(signal2.size());
    const int resultSize = n1 + n2 - 1;
    
    std::vector<double> result(resultSize, 0.0);
    
    // Cross-correlation: r[k] = sum(x[n] * conj(y[n+k])) for all valid n
    for (int k = 0; k < resultSize; ++k) {
        for (int n = 0; n < n1; ++n) {
            const int index = k - n + n2 - 1;
            if (index >= 0 && index < n2) {
                // Complex conjugate of signal2
                std::complex<double> conj_s2 = std::conj(signal2[index]);
                
                // Complex multiplication
                std::complex<double> product = signal1[n] * conj_s2;
                
                // Take real part (for complex correlation we typically want magnitude)
                result[k] += product.real();
            }
        }
    }
    
    return result;
}

// Helper function to calculate magnitude of complex correlation
std::vector<double> complexMagnitude(const std::vector<std::complex<double>>& correlation) {
    std::vector<double> result(correlation.size());
    for (size_t i = 0; i < correlation.size(); ++i) {
        result[i] = std::abs(correlation[i]);
    }
    return result;
}

CorrelationResult crossCorrelate(
    const std::vector<double>& signal1,
    const std::vector<double>& signal2,
    const CorrelationConfig& config) {
    
    // Check for empty signals
    if (signal1.empty() || signal2.empty()) {
        throw std::invalid_argument("Input signals cannot be empty");
    }
    
    // Apply window function if needed
    std::vector<double> windowedSignal1 = applyWindow(signal1, config.windowType);
    std::vector<double> windowedSignal2 = applyWindow(signal2, config.windowType);
    
    // Compute cross-correlation
    std::vector<double> correlation = directCrossCorrelation(windowedSignal1, windowedSignal2);
    
    // Normalize if requested
    if (config.normalizeOutput) {
        correlation = normalizeCorrelation(correlation);
    }
    
    // Find peaks
    std::vector<CorrelationPeak> peaks = findPeaks(
        correlation, 
        config.peakThreshold, 
        config.maxPeaks, 
        config.interpolationType);
    
    // Prepare result
    CorrelationResult result;
    result.correlation = correlation;
    result.peaks = peaks;
    result.sampleRate = config.sampleRate;
    
    // Find maximum peak confidence
    result.maxPeakConfidence = 0.0;
    for (const auto& peak : peaks) {
        result.maxPeakConfidence = std::max(result.maxPeakConfidence, peak.confidence);
    }
    
    return result;
}

CorrelationResult crossCorrelate(
    const std::vector<std::complex<double>>& signal1,
    const std::vector<std::complex<double>>& signal2,
    const CorrelationConfig& config) {
    
    // Check for empty signals
    if (signal1.empty() || signal2.empty()) {
        throw std::invalid_argument("Input signals cannot be empty");
    }
    
    // Apply window function if needed
    std::vector<std::complex<double>> windowedSignal1 = applyWindow(signal1, config.windowType);
    std::vector<std::complex<double>> windowedSignal2 = applyWindow(signal2, config.windowType);
    
    // Compute cross-correlation
    std::vector<double> correlation = directCrossCorrelation(windowedSignal1, windowedSignal2);
    
    // Normalize if requested
    if (config.normalizeOutput) {
        correlation = normalizeCorrelation(correlation);
    }
    
    // Find peaks
    std::vector<CorrelationPeak> peaks = findPeaks(
        correlation, 
        config.peakThreshold, 
        config.maxPeaks, 
        config.interpolationType);
    
    // Prepare result
    CorrelationResult result;
    result.correlation = correlation;
    result.peaks = peaks;
    result.sampleRate = config.sampleRate;
    
    // Find maximum peak confidence
    result.maxPeakConfidence = 0.0;
    for (const auto& peak : peaks) {
        result.maxPeakConfidence = std::max(result.maxPeakConfidence, peak.confidence);
    }
    
    return result;
}

// SegmentedCorrelator implementation

SegmentedCorrelator::SegmentedCorrelator(
    const CorrelationConfig& config,
    int segmentSize,
    double overlapFactor)
    : config_(config)
    , segmentSize_(segmentSize)
    , overlapFactor_(overlapFactor)
    , usingComplex_(false)
{
    // Validate inputs
    if (segmentSize <= 0) {
        throw std::invalid_argument("Segment size must be positive");
    }
    
    if (overlapFactor < 0.0 || overlapFactor >= 1.0) {
        throw std::invalid_argument("Overlap factor must be in range [0, 1)");
    }
}

CorrelationResult SegmentedCorrelator::processSegment(
    const std::vector<double>& segment1,
    const std::vector<double>& segment2) {
    
    // Mark that we're using real signals
    usingComplex_ = false;
    
    // Check if we need to use previous segments
    if (!prevSegment1_.empty() && !prevSegment2_.empty()) {
        // Calculate overlap size
        const int overlapSize = static_cast<int>(segmentSize_ * overlapFactor_);
        
        // Create combined segments with overlap
        std::vector<double> combinedSegment1(segmentSize_ + segment1.size() - overlapSize);
        std::vector<double> combinedSegment2(segmentSize_ + segment2.size() - overlapSize);
        
        // Copy previous segments
        std::copy(prevSegment1_.begin(), prevSegment1_.end(), combinedSegment1.begin());
        std::copy(prevSegment2_.begin(), prevSegment2_.end(), combinedSegment2.begin());
        
        // Copy new segments (skipping overlap)
        std::copy(segment1.begin() + overlapSize, segment1.end(), 
                 combinedSegment1.begin() + segmentSize_);
        std::copy(segment2.begin() + overlapSize, segment2.end(), 
                 combinedSegment2.begin() + segmentSize_);
        
        // Compute correlation with combined segments
        CorrelationResult result = crossCorrelate(combinedSegment1, combinedSegment2, config_);
        
        // Store segments for next call
        prevSegment1_ = segment1;
        prevSegment2_ = segment2;
        
        // Call result callback if registered
        if (resultCallback_) {
            resultCallback_(result);
        }
        
        return result;
    } else {
        // First call, no overlap
        prevSegment1_ = segment1;
        prevSegment2_ = segment2;
        
        // Compute correlation directly
        CorrelationResult result = crossCorrelate(segment1, segment2, config_);
        
        // Call result callback if registered
        if (resultCallback_) {
            resultCallback_(result);
        }
        
        return result;
    }
}

CorrelationResult SegmentedCorrelator::processSegment(
    const std::vector<std::complex<double>>& segment1,
    const std::vector<std::complex<double>>& segment2) {
    
    // Mark that we're using complex signals
    usingComplex_ = true;
    
    // Check if we need to use previous segments
    if (!prevComplexSegment1_.empty() && !prevComplexSegment2_.empty()) {
        // Calculate overlap size
        const int overlapSize = static_cast<int>(segmentSize_ * overlapFactor_);
        
        // Create combined segments with overlap
        std::vector<std::complex<double>> combinedSegment1(segmentSize_ + segment1.size() - overlapSize);
        std::vector<std::complex<double>> combinedSegment2(segmentSize_ + segment2.size() - overlapSize);
        
        // Copy previous segments
        std::copy(prevComplexSegment1_.begin(), prevComplexSegment1_.end(), combinedSegment1.begin());
        std::copy(prevComplexSegment2_.begin(), prevComplexSegment2_.end(), combinedSegment2.begin());
        
        // Copy new segments (skipping overlap)
        std::copy(segment1.begin() + overlapSize, segment1.end(), 
                 combinedSegment1.begin() + segmentSize_);
        std::copy(segment2.begin() + overlapSize, segment2.end(), 
                 combinedSegment2.begin() + segmentSize_);
        
        // Compute correlation with combined segments
        CorrelationResult result = crossCorrelate(combinedSegment1, combinedSegment2, config_);
        
        // Store segments for next call
        prevComplexSegment1_ = segment1;
        prevComplexSegment2_ = segment2;
        
        // Call result callback if registered
        if (resultCallback_) {
            resultCallback_(result);
        }
        
        return result;
    } else {
        // First call, no overlap
        prevComplexSegment1_ = segment1;
        prevComplexSegment2_ = segment2;
        
        // Compute correlation directly
        CorrelationResult result = crossCorrelate(segment1, segment2, config_);
        
        // Call result callback if registered
        if (resultCallback_) {
            resultCallback_(result);
        }
        
        return result;
    }
}

void SegmentedCorrelator::reset() {
    prevSegment1_.clear();
    prevSegment2_.clear();
    prevComplexSegment1_.clear();
    prevComplexSegment2_.clear();
}

void SegmentedCorrelator::setResultCallback(std::function<void(const CorrelationResult&)> callback) {
    resultCallback_ = callback;
}

CorrelationConfig SegmentedCorrelator::getConfig() const {
    return config_;
}

void SegmentedCorrelator::setConfig(const CorrelationConfig& config) {
    config_ = config;
}

} // namespace correlation
} // namespace tdoa 