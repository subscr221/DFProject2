/**
 * @file test_cross_correlation.cpp
 * @brief Test program for cross-correlation algorithm
 */

#include "../correlation/cross_correlation.h"
#include <iostream>
#include <vector>
#include <random>
#include <iomanip>
#include <chrono>

using namespace tdoa::correlation;

// Generate a test signal with a known delay
std::vector<double> generateTestSignal(int length, int delay, double snr) {
    // Create random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> noise(0.0, 1.0);
    
    // Create the original signal (a simple pulse)
    const int pulseWidth = std::min(100, length / 10);
    std::vector<double> signal(length, 0.0);
    
    // Add a pulse in the middle
    const int center = length / 2;
    for (int i = 0; i < pulseWidth; ++i) {
        const double t = static_cast<double>(i) / pulseWidth;
        // Gaussian pulse
        signal[center + i - pulseWidth/2] = std::exp(-10.0 * (t - 0.5) * (t - 0.5));
    }
    
    // Calculate signal power
    double signalPower = 0.0;
    for (const auto& s : signal) {
        signalPower += s * s;
    }
    signalPower /= signal.size();
    
    // Calculate noise power based on SNR
    // SNR = 10*log10(signalPower/noisePower)
    const double noisePower = signalPower / std::pow(10.0, snr / 10.0);
    const double noiseStd = std::sqrt(noisePower);
    
    // Add noise to the signal
    for (auto& s : signal) {
        s += noise(gen) * noiseStd;
    }
    
    // Create delayed version
    std::vector<double> delayedSignal(length, 0.0);
    for (int i = 0; i < length; ++i) {
        const int delayedIndex = i - delay;
        if (delayedIndex >= 0 && delayedIndex < length) {
            delayedSignal[i] = signal[delayedIndex];
        }
    }
    
    return delayedSignal;
}

int main() {
    // Test parameters
    const int signalLength = 1000;
    const int trueDelay = 42;  // Samples
    const double snr = 10.0;   // dB
    
    std::cout << "Cross-Correlation Test" << std::endl;
    std::cout << "======================" << std::endl;
    std::cout << "Signal length: " << signalLength << std::endl;
    std::cout << "True delay: " << trueDelay << " samples" << std::endl;
    std::cout << "SNR: " << snr << " dB" << std::endl;
    std::cout << std::endl;
    
    // Generate test signals
    std::vector<double> signal1(signalLength, 0.0);
    std::vector<double> signal2 = generateTestSignal(signalLength, trueDelay, snr);
    
    // Add a pulse to signal1
    const int pulseWidth = std::min(100, signalLength / 10);
    const int center = signalLength / 2;
    for (int i = 0; i < pulseWidth; ++i) {
        const double t = static_cast<double>(i) / pulseWidth;
        signal1[center + i - pulseWidth/2] = std::exp(-10.0 * (t - 0.5) * (t - 0.5));
    }
    
    // Configure correlation
    CorrelationConfig config;
    config.windowType = WindowType::Hamming;
    config.interpolationType = InterpolationType::Parabolic;
    config.peakThreshold = 0.5;
    config.maxPeaks = 3;
    config.normalizeOutput = true;
    config.sampleRate = 1000.0;  // 1 kHz sample rate
    
    // Measure execution time
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Perform cross-correlation
    CorrelationResult result = crossCorrelate(signal1, signal2, config);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    // Print results
    std::cout << "Correlation execution time: " << duration << " ms" << std::endl;
    std::cout << "Number of detected peaks: " << result.peaks.size() << std::endl;
    std::cout << std::endl;
    
    // Print detected peaks
    std::cout << "Detected peaks:" << std::endl;
    std::cout << "----------------" << std::endl;
    std::cout << std::setw(10) << "Delay" << std::setw(15) << "Coefficient" 
              << std::setw(15) << "Confidence" << std::setw(10) << "SNR" << std::endl;
    
    for (const auto& peak : result.peaks) {
        // Convert delay to correct reference frame
        const double adjustedDelay = peak.delay - (signal1.size() + signal2.size() - 1) / 2.0;
        
        std::cout << std::fixed << std::setprecision(2);
        std::cout << std::setw(10) << adjustedDelay
                  << std::setw(15) << peak.coefficient
                  << std::setw(15) << peak.confidence
                  << std::setw(10) << peak.snr
                  << std::endl;
    }
    std::cout << std::endl;
    
    // Calculate error
    double estimatedDelay = 0.0;
    if (!result.peaks.empty()) {
        // Find the peak with highest confidence
        const auto& bestPeak = *std::max_element(
            result.peaks.begin(), result.peaks.end(),
            [](const auto& a, const auto& b) { return a.confidence < b.confidence; });
        
        // Convert to the correct reference frame
        estimatedDelay = bestPeak.delay - (signal1.size() + signal2.size() - 1) / 2.0;
    }
    
    const double error = estimatedDelay - trueDelay;
    std::cout << "True delay: " << trueDelay << " samples" << std::endl;
    std::cout << "Estimated delay: " << estimatedDelay << " samples" << std::endl;
    std::cout << "Error: " << error << " samples" << std::endl;
    
    // Test with different interpolation methods
    std::cout << std::endl;
    std::cout << "Testing different interpolation methods:" << std::endl;
    std::cout << "--------------------------------------" << std::endl;
    std::cout << std::setw(15) << "Method" << std::setw(15) << "Delay" 
              << std::setw(15) << "Error" << std::setw(15) << "Execution (ms)" << std::endl;
    
    for (const auto& method : {
        InterpolationType::None,
        InterpolationType::Parabolic,
        InterpolationType::Cubic,
        InterpolationType::Gaussian,
        InterpolationType::Sinc
    }) {
        // Update config
        config.interpolationType = method;
        
        // Measure execution time
        startTime = std::chrono::high_resolution_clock::now();
        
        // Perform cross-correlation
        result = crossCorrelate(signal1, signal2, config);
        
        endTime = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        
        // Find best peak
        double methodEstimatedDelay = 0.0;
        if (!result.peaks.empty()) {
            const auto& bestPeak = *std::max_element(
                result.peaks.begin(), result.peaks.end(),
                [](const auto& a, const auto& b) { return a.confidence < b.confidence; });
            
            methodEstimatedDelay = bestPeak.delay - (signal1.size() + signal2.size() - 1) / 2.0;
        }
        
        const double methodError = methodEstimatedDelay - trueDelay;
        
        // Print method name
        std::string methodName;
        switch (method) {
            case InterpolationType::None: methodName = "None"; break;
            case InterpolationType::Parabolic: methodName = "Parabolic"; break;
            case InterpolationType::Cubic: methodName = "Cubic"; break;
            case InterpolationType::Gaussian: methodName = "Gaussian"; break;
            case InterpolationType::Sinc: methodName = "Sinc"; break;
        }
        
        std::cout << std::fixed << std::setprecision(4);
        std::cout << std::setw(15) << methodName
                  << std::setw(15) << methodEstimatedDelay
                  << std::setw(15) << methodError
                  << std::setw(15) << duration
                  << std::endl;
    }
    
    // Test with different SNR levels
    std::cout << std::endl;
    std::cout << "Testing different SNR levels:" << std::endl;
    std::cout << "---------------------------" << std::endl;
    std::cout << std::setw(10) << "SNR (dB)" << std::setw(15) << "Delay" 
              << std::setw(15) << "Error" << std::setw(15) << "Confidence" << std::endl;
    
    config.interpolationType = InterpolationType::Parabolic;  // Reset to default
    
    for (const double testSnr : {30.0, 20.0, 10.0, 5.0, 0.0, -5.0}) {
        // Generate test signal with this SNR
        std::vector<double> testSignal2 = generateTestSignal(signalLength, trueDelay, testSnr);
        
        // Perform cross-correlation
        result = crossCorrelate(signal1, testSignal2, config);
        
        // Find best peak
        double snrEstimatedDelay = 0.0;
        double peakConfidence = 0.0;
        if (!result.peaks.empty()) {
            const auto& bestPeak = *std::max_element(
                result.peaks.begin(), result.peaks.end(),
                [](const auto& a, const auto& b) { return a.confidence < b.confidence; });
            
            snrEstimatedDelay = bestPeak.delay - (signal1.size() + signal2.size() - 1) / 2.0;
            peakConfidence = bestPeak.confidence;
        }
        
        const double snrError = snrEstimatedDelay - trueDelay;
        
        std::cout << std::fixed << std::setprecision(2);
        std::cout << std::setw(10) << testSnr
                  << std::setw(15) << snrEstimatedDelay
                  << std::setw(15) << snrError
                  << std::setw(15) << peakConfidence
                  << std::endl;
    }
    
    // Test segmented correlator
    std::cout << std::endl;
    std::cout << "Testing segmented correlation:" << std::endl;
    std::cout << "-----------------------------" << std::endl;
    
    // Create longer signals
    const int longSignalLength = 10000;
    std::vector<double> longSignal1(longSignalLength, 0.0);
    std::vector<double> longSignal2 = generateTestSignal(longSignalLength, trueDelay, snr);
    
    // Add multiple pulses to signal1
    for (int i = 0; i < 10; ++i) {
        const int pulseCenter = longSignalLength / 10 * i + longSignalLength / 20;
        for (int j = 0; j < pulseWidth; ++j) {
            const double t = static_cast<double>(j) / pulseWidth;
            const int index = pulseCenter + j - pulseWidth/2;
            if (index >= 0 && index < longSignalLength) {
                longSignal1[index] = std::exp(-10.0 * (t - 0.5) * (t - 0.5));
            }
        }
    }
    
    // Create segmented correlator
    SegmentedCorrelator segCorrelator(config, 1000, 0.5);
    
    // Process segments
    std::cout << "Processing signal in segments:" << std::endl;
    
    for (int segment = 0; segment < 10; ++segment) {
        const int start = segment * 1000;
        const int end = std::min(start + 1000, longSignalLength);
        
        // Extract segment
        std::vector<double> segmentSignal1(longSignal1.begin() + start, longSignal1.begin() + end);
        std::vector<double> segmentSignal2(longSignal2.begin() + start, longSignal2.begin() + end);
        
        // Process segment
        CorrelationResult segResult = segCorrelator.processSegment(segmentSignal1, segmentSignal2);
        
        // Find best peak
        double segEstimatedDelay = 0.0;
        if (!segResult.peaks.empty()) {
            const auto& bestPeak = *std::max_element(
                segResult.peaks.begin(), segResult.peaks.end(),
                [](const auto& a, const auto& b) { return a.confidence < b.confidence; });
            
            segEstimatedDelay = bestPeak.delay - (segmentSignal1.size() + segmentSignal2.size() - 1) / 2.0;
        }
        
        std::cout << "Segment " << segment << ": "
                  << "Delay = " << segEstimatedDelay
                  << ", Peaks = " << segResult.peaks.size()
                  << ", Confidence = " << segResult.maxPeakConfidence
                  << std::endl;
    }
    
    return 0;
} 