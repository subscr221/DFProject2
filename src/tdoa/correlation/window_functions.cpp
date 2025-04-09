/**
 * @file window_functions.cpp
 * @brief Implementation of window functions and related utilities
 */

#include "cross_correlation.h"
#include <cmath>
#include <numeric>
#include <algorithm>
#include <stdexcept>

namespace tdoa {
namespace correlation {

std::vector<double> generateWindow(int length, WindowType windowType) {
    if (length <= 0) {
        throw std::invalid_argument("Window length must be positive");
    }
    
    std::vector<double> window(length);
    
    switch (windowType) {
        case WindowType::None:
            // Rectangular window (all ones)
            std::fill(window.begin(), window.end(), 1.0);
            break;
            
        case WindowType::Hamming:
            // Hamming window: w(n) = 0.54 - 0.46 * cos(2π*n/(N-1))
            for (int i = 0; i < length; ++i) {
                window[i] = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (length - 1));
            }
            break;
            
        case WindowType::Hanning:
            // Hanning window: w(n) = 0.5 * (1 - cos(2π*n/(N-1)))
            for (int i = 0; i < length; ++i) {
                window[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (length - 1)));
            }
            break;
            
        case WindowType::Blackman:
            // Blackman window: w(n) = 0.42 - 0.5*cos(2π*n/(N-1)) + 0.08*cos(4π*n/(N-1))
            for (int i = 0; i < length; ++i) {
                const double x = 2.0 * M_PI * i / (length - 1);
                window[i] = 0.42 - 0.5 * std::cos(x) + 0.08 * std::cos(2.0 * x);
            }
            break;
            
        case WindowType::BlackmanHarris:
            // Blackman-Harris window: w(n) = a0 - a1*cos(2π*n/(N-1)) + a2*cos(4π*n/(N-1)) - a3*cos(6π*n/(N-1))
            for (int i = 0; i < length; ++i) {
                const double x = 2.0 * M_PI * i / (length - 1);
                window[i] = 0.35875 - 0.48829 * std::cos(x) + 
                           0.14128 * std::cos(2.0 * x) - 0.01168 * std::cos(3.0 * x);
            }
            break;
            
        case WindowType::FlatTop:
            // Flat-top window: w(n) = a0 - a1*cos(2π*n/(N-1)) + a2*cos(4π*n/(N-1)) - a3*cos(6π*n/(N-1)) + a4*cos(8π*n/(N-1))
            for (int i = 0; i < length; ++i) {
                const double x = 2.0 * M_PI * i / (length - 1);
                window[i] = 0.21557895 - 0.41663158 * std::cos(x) + 
                           0.277263158 * std::cos(2.0 * x) - 0.083578947 * std::cos(3.0 * x) +
                           0.006947368 * std::cos(4.0 * x);
            }
            break;
    }
    
    return window;
}

std::vector<double> applyWindow(const std::vector<double>& signal, WindowType windowType) {
    if (windowType == WindowType::None) {
        return signal; // No windowing needed
    }
    
    const int length = static_cast<int>(signal.size());
    const auto window = generateWindow(length, windowType);
    
    std::vector<double> windowed(length);
    for (int i = 0; i < length; ++i) {
        windowed[i] = signal[i] * window[i];
    }
    
    return windowed;
}

std::vector<std::complex<double>> applyWindow(
    const std::vector<std::complex<double>>& signal, 
    WindowType windowType) {
    
    if (windowType == WindowType::None) {
        return signal; // No windowing needed
    }
    
    const int length = static_cast<int>(signal.size());
    const auto window = generateWindow(length, windowType);
    
    std::vector<std::complex<double>> windowed(length);
    for (int i = 0; i < length; ++i) {
        windowed[i] = signal[i] * window[i];
    }
    
    return windowed;
}

std::vector<double> normalizeCorrelation(const std::vector<double>& correlation) {
    if (correlation.empty()) {
        return {};
    }
    
    // Find max absolute value
    double maxAbs = 0.0;
    for (const auto& value : correlation) {
        maxAbs = std::max(maxAbs, std::abs(value));
    }
    
    // Avoid division by zero
    if (maxAbs < 1e-10) {
        return correlation;
    }
    
    // Normalize
    std::vector<double> normalized(correlation.size());
    for (size_t i = 0; i < correlation.size(); ++i) {
        normalized[i] = correlation[i] / maxAbs;
    }
    
    return normalized;
}

double samplesToTime(double delaySamples, double sampleRate) {
    return delaySamples / sampleRate;
}

double timeToSamples(double delaySeconds, double sampleRate) {
    return delaySeconds * sampleRate;
}

} // namespace correlation
} // namespace tdoa 