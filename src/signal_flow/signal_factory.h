/**
 * @file signal_factory.h
 * @brief Factory for creating Signal objects with utility methods
 */

#pragma once

#include "signal.h"
#include <random>
#include <ctime>
#include <string>
#include <memory>
#include <functional>
#include <vector>

namespace tdoa {
namespace signal {

/**
 * @class SignalFactory
 * @brief Factory for creating Signal objects with utility methods
 */
class SignalFactory {
public:
    /**
     * @brief Create an empty signal with specified parameters
     * @param format Data format
     * @param sampleCount Number of complex samples
     * @param sampleRate Sample rate in samples per second
     * @param centerFreq Center frequency in Hz
     * @param bandwidth Bandwidth in Hz
     * @return Shared pointer to the created Signal
     */
    static std::shared_ptr<Signal> createEmptySignal(
        DataFormat format,
        size_t sampleCount,
        double sampleRate,
        double centerFreq,
        double bandwidth
    );
    
    /**
     * @brief Create a signal from existing data
     * @param data Raw data buffer (will be copied)
     * @param dataSize Size of data buffer in bytes
     * @param format Data format
     * @param sampleCount Number of complex samples in the data
     * @param sampleRate Sample rate in samples per second
     * @param centerFreq Center frequency in Hz
     * @param bandwidth Bandwidth in Hz
     * @return Shared pointer to the created Signal
     */
    static std::shared_ptr<Signal> createSignalFromData(
        const void* data,
        size_t dataSize,
        DataFormat format,
        size_t sampleCount,
        double sampleRate,
        double centerFreq,
        double bandwidth
    );
    
    /**
     * @brief Create a signal with a sine wave at a specified frequency
     * @param format Data format
     * @param sampleCount Number of complex samples
     * @param sampleRate Sample rate in samples per second
     * @param centerFreq Center frequency in Hz
     * @param bandwidth Bandwidth in Hz
     * @param signalFreq Frequency of the sine wave in Hz (relative to center)
     * @param amplitude Amplitude of the sine wave (0.0 to 1.0)
     * @return Shared pointer to the created Signal
     */
    static std::shared_ptr<Signal> createSineWaveSignal(
        DataFormat format,
        size_t sampleCount,
        double sampleRate,
        double centerFreq,
        double bandwidth,
        double signalFreq,
        double amplitude = 1.0
    );
    
    /**
     * @brief Create a signal with white noise
     * @param format Data format
     * @param sampleCount Number of complex samples
     * @param sampleRate Sample rate in samples per second
     * @param centerFreq Center frequency in Hz
     * @param bandwidth Bandwidth in Hz
     * @param amplitude Amplitude of the noise (0.0 to 1.0)
     * @return Shared pointer to the created Signal
     */
    static std::shared_ptr<Signal> createNoiseSignal(
        DataFormat format,
        size_t sampleCount,
        double sampleRate,
        double centerFreq,
        double bandwidth,
        double amplitude = 1.0
    );
    
    /**
     * @brief Create a chirp signal with linearly increasing frequency
     * @param format Data format
     * @param sampleCount Number of complex samples
     * @param sampleRate Sample rate in samples per second
     * @param centerFreq Center frequency in Hz
     * @param bandwidth Bandwidth in Hz
     * @param startFreq Start frequency of the chirp in Hz (relative to center)
     * @param endFreq End frequency of the chirp in Hz (relative to center)
     * @param amplitude Amplitude of the chirp (0.0 to 1.0)
     * @return Shared pointer to the created Signal
     */
    static std::shared_ptr<Signal> createChirpSignal(
        DataFormat format,
        size_t sampleCount,
        double sampleRate,
        double centerFreq,
        double bandwidth,
        double startFreq,
        double endFreq,
        double amplitude = 1.0
    );
    
    /**
     * @brief Create a signal with multiple carriers
     * @param format Data format
     * @param sampleCount Number of complex samples
     * @param sampleRate Sample rate in samples per second
     * @param centerFreq Center frequency in Hz
     * @param bandwidth Bandwidth in Hz
     * @param carriers Vector of carrier frequencies in Hz (relative to center)
     * @param amplitudes Vector of amplitudes for each carrier (0.0 to 1.0)
     * @return Shared pointer to the created Signal
     */
    static std::shared_ptr<Signal> createMultiCarrierSignal(
        DataFormat format,
        size_t sampleCount,
        double sampleRate,
        double centerFreq,
        double bandwidth,
        const std::vector<double>& carriers,
        const std::vector<double>& amplitudes
    );
    
    /**
     * @brief Generate a unique signal ID
     * @return Unique ID string
     */
    static std::string generateSignalId();
    
    /**
     * @brief Set source info for a signal
     * @param signal Signal to set source info for
     * @param deviceType Device type string
     * @param deviceId Device ID string
     * @param locationId Location ID string
     * @param latitude Latitude in degrees
     * @param longitude Longitude in degrees
     * @param altitude Altitude in meters
     */
    static void setSourceInfo(
        std::shared_ptr<Signal> signal,
        const std::string& deviceType,
        const std::string& deviceId,
        const std::string& locationId,
        double latitude,
        double longitude,
        double altitude
    );
};

} // namespace signal
} // namespace tdoa 