/**
 * @file signal_factory.cpp
 * @brief Implementation of the SignalFactory class
 */

#include "signal_factory.h"
#include <cmath>
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace tdoa {
namespace signal {

// Helper function to generate a UUID-like ID
std::string SignalFactory::generateSignalId() {
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    auto epoch = now_ms.time_since_epoch();
    auto value = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
    
    // Generate a random component
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 0xFFFF);
    
    // Format the ID as a string
    std::stringstream ss;
    ss << "sig_" << std::hex << std::setfill('0')
       << std::setw(16) << value << "_"
       << std::setw(4) << dis(gen) << "_"
       << std::setw(4) << dis(gen) << "_"
       << std::setw(4) << dis(gen);
    
    return ss.str();
}

// Create an empty signal
std::shared_ptr<Signal> SignalFactory::createEmptySignal(
    DataFormat format,
    size_t sampleCount,
    double sampleRate,
    double centerFreq,
    double bandwidth
) {
    // Create the signal with the specified format and sample count
    auto signal = std::make_shared<Signal>(format, sampleCount);
    
    // Set parameters
    signal->setSampleRate(sampleRate);
    signal->setCenterFrequency(centerFreq);
    signal->setBandwidth(bandwidth);
    signal->setTimestamp(std::chrono::duration<double>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    signal->setId(generateSignalId());
    
    return signal;
}

// Create a signal from existing data
std::shared_ptr<Signal> SignalFactory::createSignalFromData(
    const void* data,
    size_t dataSize,
    DataFormat format,
    size_t sampleCount,
    double sampleRate,
    double centerFreq,
    double bandwidth
) {
    // Create the signal with the specified data
    auto signal = std::make_shared<Signal>(data, dataSize, format, sampleCount);
    
    // Set parameters
    signal->setSampleRate(sampleRate);
    signal->setCenterFrequency(centerFreq);
    signal->setBandwidth(bandwidth);
    signal->setTimestamp(std::chrono::duration<double>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    signal->setId(generateSignalId());
    
    return signal;
}

// Create a signal with a sine wave
std::shared_ptr<Signal> SignalFactory::createSineWaveSignal(
    DataFormat format,
    size_t sampleCount,
    double sampleRate,
    double centerFreq,
    double bandwidth,
    double signalFreq,
    double amplitude
) {
    // Validate parameters
    if (amplitude <= 0.0 || amplitude > 1.0) {
        throw std::invalid_argument("Amplitude must be between 0 and 1");
    }
    
    // Create an empty signal
    auto signal = createEmptySignal(format, sampleCount, sampleRate, centerFreq, bandwidth);
    signal->setMetadata("signal_type", "sine_wave");
    signal->setMetadata("signal_freq", std::to_string(signalFreq));
    signal->setMetadata("amplitude", std::to_string(amplitude));
    
    // If the format is not ComplexFloat32, we need to convert
    std::shared_ptr<Signal> workSignal;
    if (format != DataFormat::ComplexFloat32) {
        // Create a temporary signal in float format
        workSignal = std::make_shared<Signal>(DataFormat::ComplexFloat32, sampleCount);
        workSignal->setSampleRate(sampleRate);
    } else {
        workSignal = signal;
    }
    
    // Generate the sine wave
    auto* samples = workSignal->complexFloat();
    const double phaseIncrement = 2.0 * M_PI * signalFreq / sampleRate;
    double phase = 0.0;
    
    for (size_t i = 0; i < sampleCount; i++) {
        samples[i] = std::complex<float>(
            amplitude * std::cos(phase),
            amplitude * std::sin(phase)
        );
        phase += phaseIncrement;
        
        // Wrap phase to keep it in a reasonable range
        if (phase > 2.0 * M_PI) {
            phase -= 2.0 * M_PI;
        }
    }
    
    // Convert to the target format if needed
    if (format != DataFormat::ComplexFloat32) {
        // Convert and copy metadata
        auto convertedSignal = workSignal->convertToFormat(format);
        signal->setMetadata("original_format", "ComplexFloat32");
        signal->setMetadata("converted_to", signal->getMetadata("format"));
        
        // Copy the converted data
        std::memcpy(signal->data(), convertedSignal->data(), signal->getBufferSize());
    }
    
    return signal;
}

// Create a signal with white noise
std::shared_ptr<Signal> SignalFactory::createNoiseSignal(
    DataFormat format,
    size_t sampleCount,
    double sampleRate,
    double centerFreq,
    double bandwidth,
    double amplitude
) {
    // Validate parameters
    if (amplitude <= 0.0 || amplitude > 1.0) {
        throw std::invalid_argument("Amplitude must be between 0 and 1");
    }
    
    // Create an empty signal
    auto signal = createEmptySignal(format, sampleCount, sampleRate, centerFreq, bandwidth);
    signal->setMetadata("signal_type", "noise");
    signal->setMetadata("amplitude", std::to_string(amplitude));
    
    // If the format is not ComplexFloat32, we need to convert
    std::shared_ptr<Signal> workSignal;
    if (format != DataFormat::ComplexFloat32) {
        // Create a temporary signal in float format
        workSignal = std::make_shared<Signal>(DataFormat::ComplexFloat32, sampleCount);
        workSignal->setSampleRate(sampleRate);
    } else {
        workSignal = signal;
    }
    
    // Generate white noise
    auto* samples = workSignal->complexFloat();
    
    // Random number generators
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dist(0.0, amplitude / std::sqrt(2.0f)); // Scale for complex values
    
    for (size_t i = 0; i < sampleCount; i++) {
        samples[i] = std::complex<float>(dist(gen), dist(gen));
    }
    
    // Convert to the target format if needed
    if (format != DataFormat::ComplexFloat32) {
        // Convert and copy metadata
        auto convertedSignal = workSignal->convertToFormat(format);
        signal->setMetadata("original_format", "ComplexFloat32");
        signal->setMetadata("converted_to", signal->getMetadata("format"));
        
        // Copy the converted data
        std::memcpy(signal->data(), convertedSignal->data(), signal->getBufferSize());
    }
    
    return signal;
}

// Create a chirp signal
std::shared_ptr<Signal> SignalFactory::createChirpSignal(
    DataFormat format,
    size_t sampleCount,
    double sampleRate,
    double centerFreq,
    double bandwidth,
    double startFreq,
    double endFreq,
    double amplitude
) {
    // Validate parameters
    if (amplitude <= 0.0 || amplitude > 1.0) {
        throw std::invalid_argument("Amplitude must be between 0 and 1");
    }
    
    // Create an empty signal
    auto signal = createEmptySignal(format, sampleCount, sampleRate, centerFreq, bandwidth);
    signal->setMetadata("signal_type", "chirp");
    signal->setMetadata("start_freq", std::to_string(startFreq));
    signal->setMetadata("end_freq", std::to_string(endFreq));
    signal->setMetadata("amplitude", std::to_string(amplitude));
    
    // If the format is not ComplexFloat32, we need to convert
    std::shared_ptr<Signal> workSignal;
    if (format != DataFormat::ComplexFloat32) {
        // Create a temporary signal in float format
        workSignal = std::make_shared<Signal>(DataFormat::ComplexFloat32, sampleCount);
        workSignal->setSampleRate(sampleRate);
    } else {
        workSignal = signal;
    }
    
    // Generate the chirp signal
    auto* samples = workSignal->complexFloat();
    
    // Time duration
    const double duration = static_cast<double>(sampleCount) / sampleRate;
    
    // Chirp rate (Hz/s)
    const double chirpRate = (endFreq - startFreq) / duration;
    
    // Generate the chirp
    double phase = 0.0;
    double instantFreq = startFreq;
    const double dt = 1.0 / sampleRate;
    
    for (size_t i = 0; i < sampleCount; i++) {
        // Calculate instantaneous frequency
        double time = static_cast<double>(i) / sampleRate;
        instantFreq = startFreq + chirpRate * time;
        
        // Calculate phase increment
        double phaseIncrement = 2.0 * M_PI * instantFreq * dt;
        
        // Update phase
        phase += phaseIncrement;
        
        // Wrap phase to keep it in a reasonable range
        if (phase > 2.0 * M_PI) {
            phase -= 2.0 * M_PI;
        }
        
        // Generate complex sample
        samples[i] = std::complex<float>(
            amplitude * std::cos(phase),
            amplitude * std::sin(phase)
        );
    }
    
    // Convert to the target format if needed
    if (format != DataFormat::ComplexFloat32) {
        // Convert and copy metadata
        auto convertedSignal = workSignal->convertToFormat(format);
        signal->setMetadata("original_format", "ComplexFloat32");
        signal->setMetadata("converted_to", signal->getMetadata("format"));
        
        // Copy the converted data
        std::memcpy(signal->data(), convertedSignal->data(), signal->getBufferSize());
    }
    
    return signal;
}

// Create a signal with multiple carriers
std::shared_ptr<Signal> SignalFactory::createMultiCarrierSignal(
    DataFormat format,
    size_t sampleCount,
    double sampleRate,
    double centerFreq,
    double bandwidth,
    const std::vector<double>& carriers,
    const std::vector<double>& amplitudes
) {
    // Validate parameters
    if (carriers.size() != amplitudes.size()) {
        throw std::invalid_argument("Carriers and amplitudes vectors must have the same size");
    }
    
    // Create an empty signal
    auto signal = createEmptySignal(format, sampleCount, sampleRate, centerFreq, bandwidth);
    signal->setMetadata("signal_type", "multi_carrier");
    signal->setMetadata("carrier_count", std::to_string(carriers.size()));
    
    // If the format is not ComplexFloat32, we need to convert
    std::shared_ptr<Signal> workSignal;
    if (format != DataFormat::ComplexFloat32) {
        // Create a temporary signal in float format
        workSignal = std::make_shared<Signal>(DataFormat::ComplexFloat32, sampleCount);
        workSignal->setSampleRate(sampleRate);
    } else {
        workSignal = signal;
    }
    
    // Get samples pointer
    auto* samples = workSignal->complexFloat();
    
    // Initialize samples to zero
    for (size_t i = 0; i < sampleCount; i++) {
        samples[i] = std::complex<float>(0.0f, 0.0f);
    }
    
    // Add each carrier
    for (size_t c = 0; c < carriers.size(); c++) {
        // Store carrier info in metadata
        signal->setMetadata("carrier_" + std::to_string(c) + "_freq", std::to_string(carriers[c]));
        signal->setMetadata("carrier_" + std::to_string(c) + "_amplitude", std::to_string(amplitudes[c]));
        
        // Validate amplitude
        if (amplitudes[c] <= 0.0 || amplitudes[c] > 1.0) {
            throw std::invalid_argument("Amplitude must be between 0 and 1");
        }
        
        // Generate the carrier
        const double phaseIncrement = 2.0 * M_PI * carriers[c] / sampleRate;
        double phase = 0.0;
        
        for (size_t i = 0; i < sampleCount; i++) {
            samples[i] += std::complex<float>(
                amplitudes[c] * std::cos(phase),
                amplitudes[c] * std::sin(phase)
            );
            phase += phaseIncrement;
            
            // Wrap phase to keep it in a reasonable range
            if (phase > 2.0 * M_PI) {
                phase -= 2.0 * M_PI;
            }
        }
    }
    
    // Convert to the target format if needed
    if (format != DataFormat::ComplexFloat32) {
        // Convert and copy metadata
        auto convertedSignal = workSignal->convertToFormat(format);
        signal->setMetadata("original_format", "ComplexFloat32");
        signal->setMetadata("converted_to", signal->getMetadata("format"));
        
        // Copy the converted data
        std::memcpy(signal->data(), convertedSignal->data(), signal->getBufferSize());
    }
    
    return signal;
}

// Set source info for a signal
void SignalFactory::setSourceInfo(
    std::shared_ptr<Signal> signal,
    const std::string& deviceType,
    const std::string& deviceId,
    const std::string& locationId,
    double latitude,
    double longitude,
    double altitude
) {
    if (!signal) {
        throw std::invalid_argument("Signal is null");
    }
    
    SourceInfo info;
    info.deviceType = deviceType;
    info.deviceId = deviceId;
    info.locationId = locationId;
    info.latitude = latitude;
    info.longitude = longitude;
    info.altitude = altitude;
    
    signal->setSourceInfo(info);
}

} // namespace signal
} // namespace tdoa 