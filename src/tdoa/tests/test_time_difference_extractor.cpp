/**
 * @file test_time_difference_extractor.cpp
 * @brief Test program for time difference extraction
 */

#include "../time_difference/time_difference_extractor.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <random>
#include <chrono>
#include <thread>
#include <cmath>
#include <algorithm>

using namespace tdoa::time_difference;
using namespace tdoa::correlation;

// Generate a test signal with known time offset
std::vector<double> generateTestSignalWithOffset(
    int length, double offsetSeconds, double sampleRate, double snr) {
    
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
    
    // Calculate offset in samples
    const int offsetSamples = static_cast<int>(std::round(offsetSeconds * sampleRate));
    
    // Create offset version with noise
    std::vector<double> offsetSignal(length, 0.0);
    for (int i = 0; i < length; ++i) {
        const int originalIndex = i - offsetSamples;
        if (originalIndex >= 0 && originalIndex < length) {
            offsetSignal[i] = signal[originalIndex];
        }
        
        // Add noise
        offsetSignal[i] += noise(gen) * noiseStd;
    }
    
    return offsetSignal;
}

int main() {
    // Test parameters
    const int signalLength = 1000;
    const double sampleRate = 1000.0;  // 1 kHz
    const double snr = 20.0;           // 20 dB
    
    // Create time difference config
    TimeDifferenceConfig config;
    config.correlationConfig.sampleRate = sampleRate;
    config.correlationConfig.windowType = WindowType::Hamming;
    config.correlationConfig.interpolationType = InterpolationType::Parabolic;
    config.correlationConfig.peakThreshold = 0.5;
    config.calibrationMode = CalibrationMode::None;
    config.clockCorrectionMethod = ClockCorrectionMethod::None;
    
    // Create time difference extractor
    TimeDifferenceExtractor extractor(config);
    
    // Define sources
    SignalSource source1("ref", 0.0, 0.0, 0.0);     // Reference source at origin
    SignalSource source2("r1", 100.0, 0.0, 0.0);    // 100m east
    SignalSource source3("r2", 0.0, 100.0, 0.0);    // 100m north
    SignalSource source4("r3", -100.0, -100.0, 0.0); // 141.4m southwest
    
    // Add sources
    extractor.addSource(source1);
    extractor.addSource(source2);
    extractor.addSource(source3);
    extractor.addSource(source4);
    
    // Set reference source
    extractor.setReferenceSource("ref");
    
    std::cout << "Time Difference Extraction Test" << std::endl;
    std::cout << "=============================" << std::endl;
    std::cout << "Sample rate: " << sampleRate << " Hz" << std::endl;
    std::cout << "Signal length: " << signalLength << " samples" << std::endl;
    std::cout << "SNR: " << snr << " dB" << std::endl;
    std::cout << std::endl;
    
    // Define test offsets (in seconds)
    std::map<std::string, double> trueOffsets = {
        {"r1", 0.0001},    // 100 microseconds
        {"r2", -0.0002},   // -200 microseconds
        {"r3", 0.0003}     // 300 microseconds
    };
    
    std::cout << "True time offsets:" << std::endl;
    std::cout << "------------------" << std::endl;
    for (const auto& pair : trueOffsets) {
        std::cout << "Source " << pair.first << ": " 
                  << std::fixed << std::setprecision(6) << pair.second * 1e6 
                  << " μs" << std::endl;
    }
    std::cout << std::endl;
    
    // Generate reference signal
    std::vector<double> refSignal(signalLength, 0.0);
    
    // Add a pulse to refSignal
    const int pulseWidth = std::min(100, signalLength / 10);
    const int center = signalLength / 2;
    for (int i = 0; i < pulseWidth; ++i) {
        const double t = static_cast<double>(i) / pulseWidth;
        refSignal[center + i - pulseWidth/2] = std::exp(-10.0 * (t - 0.5) * (t - 0.5));
    }
    
    // Generate test signals with offsets
    std::map<std::string, std::vector<double>> signals;
    signals["ref"] = refSignal;
    
    for (const auto& pair : trueOffsets) {
        const std::string& sourceId = pair.first;
        const double offset = pair.second;
        
        signals[sourceId] = generateTestSignalWithOffset(signalLength, offset, sampleRate, snr);
    }
    
    // Register callback for time differences
    extractor.setTimeDifferenceCallback([](const TimeDifferenceSet& tdSet) {
        std::cout << "Callback received " << tdSet.differences.size() 
                  << " time differences" << std::endl;
    });
    
    // Process signals
    auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    auto startTime = std::chrono::high_resolution_clock::now();
    TimeDifferenceSet result = extractor.processSignals(signals, timestamp);
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    // Print results
    std::cout << "Processing time: " << duration << " ms" << std::endl;
    std::cout << "Number of time differences: " << result.differences.size() << std::endl;
    std::cout << std::endl;
    
    // Print extracted time differences
    std::cout << "Extracted time differences:" << std::endl;
    std::cout << "-------------------------" << std::endl;
    std::cout << std::setw(10) << "Source"
              << std::setw(15) << "Measured (μs)"
              << std::setw(15) << "True (μs)"
              << std::setw(15) << "Error (μs)"
              << std::setw(15) << "Confidence"
              << std::endl;
    
    for (const auto& diff : result.differences) {
        const std::string& sourceId = diff.sourceId2;
        const double measuredOffset = diff.timeDiff;
        const double trueOffset = trueOffsets[sourceId];
        const double error = measuredOffset - trueOffset;
        
        std::cout << std::fixed << std::setprecision(3);
        std::cout << std::setw(10) << sourceId
                  << std::setw(15) << measuredOffset * 1e6
                  << std::setw(15) << trueOffset * 1e6
                  << std::setw(15) << error * 1e6
                  << std::setw(15) << diff.confidence
                  << std::endl;
    }
    std::cout << std::endl;
    
    // Test with clock offsets
    std::cout << "Testing with clock offsets:" << std::endl;
    std::cout << "------------------------" << std::endl;
    
    // Add clock offset to sources
    extractor.setClockOffset("r1", 0.00005);  // 50 μs
    extractor.setClockOffset("r2", -0.00005); // -50 μs
    extractor.setClockOffset("r3", 0.0001);   // 100 μs
    
    // Configure clock correction
    config.clockCorrectionMethod = ClockCorrectionMethod::Offset;
    extractor.setConfig(config);
    
    // Process signals again
    result = extractor.processSignals(signals, timestamp);
    
    // Print results with clock correction
    std::cout << "Time differences with clock correction:" << std::endl;
    std::cout << std::setw(10) << "Source"
              << std::setw(15) << "Measured (μs)"
              << std::setw(15) << "True (μs)"
              << std::setw(15) << "Error (μs)"
              << std::setw(15) << "Confidence"
              << std::endl;
    
    for (const auto& diff : result.differences) {
        const std::string& sourceId = diff.sourceId2;
        const double measuredOffset = diff.timeDiff;
        const double trueOffset = trueOffsets[sourceId];
        const double error = measuredOffset - trueOffset;
        
        std::cout << std::fixed << std::setprecision(3);
        std::cout << std::setw(10) << sourceId
                  << std::setw(15) << measuredOffset * 1e6
                  << std::setw(15) << trueOffset * 1e6
                  << std::setw(15) << error * 1e6
                  << std::setw(15) << diff.confidence
                  << std::endl;
    }
    std::cout << std::endl;
    
    // Test with different SNR levels
    std::cout << "Testing different SNR levels:" << std::endl;
    std::cout << "---------------------------" << std::endl;
    
    // Reset extractor
    extractor.reset();
    
    std::cout << std::setw(10) << "SNR (dB)"
              << std::setw(15) << "r1 Error (μs)"
              << std::setw(15) << "r2 Error (μs)"
              << std::setw(15) << "r3 Error (μs)"
              << std::setw(15) << "Avg Conf"
              << std::endl;
    
    for (const double testSnr : {30.0, 20.0, 10.0, 5.0, 0.0}) {
        // Generate signals with this SNR
        std::map<std::string, std::vector<double>> snrSignals;
        snrSignals["ref"] = refSignal;
        
        for (const auto& pair : trueOffsets) {
            const std::string& sourceId = pair.first;
            const double offset = pair.second;
            
            snrSignals[sourceId] = generateTestSignalWithOffset(signalLength, offset, sampleRate, testSnr);
        }
        
        // Process signals
        result = extractor.processSignals(snrSignals, timestamp);
        
        // Calculate errors and average confidence
        double r1Error = 0.0, r2Error = 0.0, r3Error = 0.0;
        double avgConf = 0.0;
        
        for (const auto& diff : result.differences) {
            const std::string& sourceId = diff.sourceId2;
            const double measuredOffset = diff.timeDiff;
            const double trueOffset = trueOffsets[sourceId];
            const double error = measuredOffset - trueOffset;
            
            if (sourceId == "r1") r1Error = error;
            else if (sourceId == "r2") r2Error = error;
            else if (sourceId == "r3") r3Error = error;
            
            avgConf += diff.confidence;
        }
        
        avgConf /= result.differences.size();
        
        std::cout << std::fixed << std::setprecision(3);
        std::cout << std::setw(10) << testSnr
                  << std::setw(15) << r1Error * 1e6
                  << std::setw(15) << r2Error * 1e6
                  << std::setw(15) << r3Error * 1e6
                  << std::setw(15) << avgConf
                  << std::endl;
    }
    
    return 0;
} 