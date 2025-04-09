/**
 * @file correlation_peak.cpp
 * @brief Implementation of correlation peak detection and interpolation
 */

#include "cross_correlation.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <limits>

namespace tdoa {
namespace correlation {

CorrelationPeak interpolatePeak(
    const std::vector<double>& correlation,
    int peakIndex,
    InterpolationType interpolationType) {
    
    const int n = static_cast<int>(correlation.size());
    
    // Ensure valid peak index
    if (peakIndex <= 0 || peakIndex >= n - 1) {
        // Cannot interpolate at the edges, return the peak as is
        CorrelationPeak peak;
        peak.delay = static_cast<double>(peakIndex);
        peak.coefficient = correlation[peakIndex];
        peak.confidence = 1.0;  // No interpolation confidence
        peak.snr = 0.0;  // Need to calculate separately
        return peak;
    }
    
    double interpolatedDelay = peakIndex;
    double interpolatedCoefficient = correlation[peakIndex];
    
    // Get neighboring points
    const double y_prev = correlation[peakIndex - 1];
    const double y_peak = correlation[peakIndex];
    const double y_next = correlation[peakIndex + 1];
    
    switch (interpolationType) {
        case InterpolationType::None:
            // No interpolation, just return the peak index
            break;
            
        case InterpolationType::Parabolic: {
            // Parabolic interpolation: fit a parabola to the three points
            // y = a*x^2 + b*x + c
            // The peak is at x = -b/(2*a)
            const double a = 0.5 * (y_prev + y_next) - y_peak;
            
            // Avoid division by zero (flat top or noise)
            if (std::abs(a) > 1e-10) {
                const double b = 0.5 * (y_next - y_prev);
                interpolatedDelay = peakIndex - b / (2.0 * a);
                
                // Calculate interpolated coefficient
                const double c = y_peak - a * std::pow(peakIndex, 2) - b * peakIndex;
                interpolatedCoefficient = a * std::pow(interpolatedDelay, 2) + 
                                         b * interpolatedDelay + c;
            }
            break;
        }
            
        case InterpolationType::Cubic: {
            // For cubic interpolation, we need four points
            // We need to handle the edge cases
            if (peakIndex <= 1 || peakIndex >= n - 2) {
                // Fall back to parabolic for edge cases
                return interpolatePeak(correlation, peakIndex, InterpolationType::Parabolic);
            }
            
            // Get additional neighboring point
            const double y_prev2 = correlation[peakIndex - 2];
            const double y_next2 = correlation[peakIndex + 2];
            
            // Use cubic interpolation formula (simplified)
            // This is an approximation of cubic interpolation
            const double a = (y_next2 - 4.0 * y_next + 6.0 * y_peak - 4.0 * y_prev + y_prev2) / 24.0;
            const double b = (y_next - 2.0 * y_peak + y_prev) / 2.0;
            const double c = (y_next - y_prev) / 2.0;
            
            // Find the extremum by taking the derivative
            if (std::abs(a) > 1e-10) {
                // Quadratic formula for the derivative's root
                const double discriminant = b * b - 3.0 * a * c;
                if (discriminant >= 0) {
                    // Take the root closest to the peak index
                    const double root1 = (-b + std::sqrt(discriminant)) / (3.0 * a);
                    const double root2 = (-b - std::sqrt(discriminant)) / (3.0 * a);
                    
                    const double offset1 = std::abs(root1);
                    const double offset2 = std::abs(root2);
                    
                    const double offset = (offset1 < offset2) ? root1 : root2;
                    
                    // Ensure we're not too far from the original peak
                    if (std::abs(offset) <= 1.5) {
                        interpolatedDelay = peakIndex + offset;
                        // Calculate interpolated coefficient
                        interpolatedCoefficient = y_peak + c * offset + b * offset * offset + 
                                                a * offset * offset * offset;
                    }
                }
            }
            break;
        }
            
        case InterpolationType::Gaussian: {
            // Gaussian interpolation using logarithms
            // log(y) = log(peak) - (x-μ)²/(2σ²)
            // The peak is at μ
            
            // Convert to logarithm, avoiding negative values
            const double log_prev = std::log(std::max(y_prev, 1e-10));
            const double log_peak = std::log(std::max(y_peak, 1e-10));
            const double log_next = std::log(std::max(y_next, 1e-10));
            
            // Solve for μ (peak location)
            const double denominator = 2.0 * log_prev - 4.0 * log_peak + 2.0 * log_next;
            
            // Avoid division by zero
            if (std::abs(denominator) > 1e-10) {
                const double delta = (log_prev - log_next) / denominator;
                interpolatedDelay = peakIndex + delta;
                
                // Calculate interpolated coefficient
                // This is approximate since we don't know σ exactly
                const double sigma2 = -1.0 / (log_prev - 2.0 * log_peak + log_next);
                interpolatedCoefficient = y_peak * std::exp(-(delta * delta) / (2.0 * sigma2));
            }
            break;
        }
            
        case InterpolationType::Sinc: {
            // Sinc interpolation is more complex and typically needs more points
            // For simplicity, we'll use a 5-point formula if possible
            
            if (peakIndex <= 2 || peakIndex >= n - 3) {
                // Fall back to parabolic for edge cases
                return interpolatePeak(correlation, peakIndex, InterpolationType::Parabolic);
            }
            
            // Use 5 points centered around the peak
            std::vector<double> y_values(5);
            for (int i = 0; i < 5; ++i) {
                y_values[i] = correlation[peakIndex - 2 + i];
            }
            
            // Iterate to find the peak using Newton's method
            double x = peakIndex;
            for (int iter = 0; iter < 5; ++iter) {
                // Calculate sinc interpolation and its derivative
                double y = 0.0;
                double dydx = 0.0;
                
                for (int i = 0; i < 5; ++i) {
                    const double xi = peakIndex - 2 + i;
                    const double dx = x - xi;
                    
                    if (std::abs(dx) < 1e-10) {
                        // Exactly at sample point
                        y += y_values[i];
                    } else {
                        const double sinc = std::sin(M_PI * dx) / (M_PI * dx);
                        y += y_values[i] * sinc;
                        
                        // Derivative of sinc
                        const double dsinc = std::cos(M_PI * dx) / dx - 
                                           std::sin(M_PI * dx) / (M_PI * dx * dx);
                        dydx += y_values[i] * dsinc;
                    }
                }
                
                // Newton's update: x = x - f(x)/f'(x)
                if (std::abs(dydx) > 1e-10) {
                    const double delta = -dydx / std::abs(dydx) * 0.1;  // Damped update
                    x += delta;
                    
                    // Check if we've converged
                    if (std::abs(delta) < 1e-5) {
                        break;
                    }
                } else {
                    break;
                }
            }
            
            // Ensure the result is within a reasonable range
            if (std::abs(x - peakIndex) <= 1.5) {
                interpolatedDelay = x;
                
                // Calculate interpolated coefficient
                double y = 0.0;
                for (int i = 0; i < 5; ++i) {
                    const double xi = peakIndex - 2 + i;
                    const double dx = x - xi;
                    
                    if (std::abs(dx) < 1e-10) {
                        y += y_values[i];
                    } else {
                        const double sinc = std::sin(M_PI * dx) / (M_PI * dx);
                        y += y_values[i] * sinc;
                    }
                }
                interpolatedCoefficient = y;
            }
            break;
        }
    }
    
    // Create peak result
    CorrelationPeak peak;
    peak.delay = interpolatedDelay;
    peak.coefficient = interpolatedCoefficient;
    
    // Calculate SNR
    peak.snr = estimatePeakSnr(correlation, peakIndex);
    
    // Calculate confidence (proportional to SNR and correlation coefficient)
    peak.confidence = calculatePeakConfidence(peak, correlation);
    
    return peak;
}

double estimatePeakSnr(const std::vector<double>& correlation, int peakIndex, int windowSize) {
    const int n = static_cast<int>(correlation.size());
    
    // Ensure valid peak index
    if (peakIndex < 0 || peakIndex >= n) {
        return 0.0;
    }
    
    // Get peak value
    const double peakValue = correlation[peakIndex];
    
    // Determine noise region (away from the peak)
    std::vector<double> noiseValues;
    
    for (int i = 0; i < n; ++i) {
        // Skip the region around the peak
        if (i < peakIndex - windowSize || i > peakIndex + windowSize) {
            noiseValues.push_back(std::abs(correlation[i]));
        }
    }
    
    // If we don't have enough noise samples, use the entire signal except peak
    if (noiseValues.size() < 10) {
        noiseValues.clear();
        for (int i = 0; i < n; ++i) {
            if (i != peakIndex) {
                noiseValues.push_back(std::abs(correlation[i]));
            }
        }
    }
    
    // Calculate mean and standard deviation of noise
    double noiseMean = 0.0;
    if (!noiseValues.empty()) {
        noiseMean = std::accumulate(noiseValues.begin(), noiseValues.end(), 0.0) / 
                   static_cast<double>(noiseValues.size());
    }
    
    double noiseStd = 0.0;
    if (noiseValues.size() > 1) {
        double sumSqDiff = 0.0;
        for (const auto& value : noiseValues) {
            sumSqDiff += std::pow(value - noiseMean, 2);
        }
        noiseStd = std::sqrt(sumSqDiff / (noiseValues.size() - 1));
    }
    
    // Avoid division by zero
    if (noiseStd < 1e-10) {
        noiseStd = 1e-10;
    }
    
    // Calculate SNR (peak to noise ratio in linear scale)
    return std::abs(peakValue) / noiseStd;
}

double calculatePeakConfidence(const CorrelationPeak& peak, const std::vector<double>& correlation) {
    // Calculate confidence based on SNR and peak sharpness
    
    // Get peak value
    const double peakValue = peak.coefficient;
    
    // Find peak index (nearest integer to delay)
    const int peakIndex = static_cast<int>(std::round(peak.delay));
    
    // Check if peak index is valid
    if (peakIndex < 0 || peakIndex >= static_cast<int>(correlation.size())) {
        return 0.0;
    }
    
    // Calculate peak sharpness (second derivative at the peak)
    double peakSharpness = 0.0;
    if (peakIndex > 0 && peakIndex < static_cast<int>(correlation.size()) - 1) {
        peakSharpness = std::abs(correlation[peakIndex - 1] - 2.0 * correlation[peakIndex] + 
                             correlation[peakIndex + 1]);
    }
    
    // Normalize peak sharpness to [0, 1]
    const double maxSharpness = 4.0;  // Theoretical maximum for normalized correlation
    peakSharpness = std::min(peakSharpness / maxSharpness, 1.0);
    
    // Normalize SNR factor (SNR of 10 gives full confidence)
    const double snrFactor = std::min(peak.snr / 10.0, 1.0);
    
    // Combine factors (weighted average)
    const double confidenceValue = 0.6 * snrFactor + 0.4 * peakSharpness;
    
    return confidenceValue;
}

std::vector<CorrelationPeak> findPeaks(
    const std::vector<double>& correlation,
    double peakThreshold,
    int maxPeaks,
    InterpolationType interpolationType) {
    
    const int n = static_cast<int>(correlation.size());
    
    if (n <= 2) {
        return {};  // Not enough points for peak detection
    }
    
    // Find maximum absolute value for threshold
    double maxAbsValue = 0.0;
    for (const auto& value : correlation) {
        maxAbsValue = std::max(maxAbsValue, std::abs(value));
    }
    
    // Set absolute threshold
    const double absThreshold = maxAbsValue * peakThreshold;
    
    // Find all potential peaks
    std::vector<std::pair<int, double>> peakIndices;
    
    for (int i = 1; i < n - 1; ++i) {
        const double val = correlation[i];
        const double prev = correlation[i - 1];
        const double next = correlation[i + 1];
        
        // Check if this is a local maximum or minimum
        const bool isLocalMax = (val > prev && val > next);
        const bool isLocalMin = (val < prev && val < next);
        
        // If this is a local extremum and above threshold, add to list
        if ((isLocalMax || isLocalMin) && std::abs(val) >= absThreshold) {
            peakIndices.emplace_back(i, std::abs(val));
        }
    }
    
    // Sort peaks by absolute coefficient (descending)
    std::sort(peakIndices.begin(), peakIndices.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Take top peaks
    const int numPeaks = std::min(maxPeaks, static_cast<int>(peakIndices.size()));
    
    // Interpolate around peaks for sub-sample precision
    std::vector<CorrelationPeak> peaks;
    for (int i = 0; i < numPeaks; ++i) {
        const int peakIndex = peakIndices[i].first;
        
        // Interpolate the peak
        CorrelationPeak peak = interpolatePeak(correlation, peakIndex, interpolationType);
        
        // Ensure correct sign
        if (correlation[peakIndex] < 0) {
            peak.coefficient = -std::abs(peak.coefficient);
        } else {
            peak.coefficient = std::abs(peak.coefficient);
        }
        
        peaks.push_back(peak);
    }
    
    return peaks;
}

} // namespace correlation
} // namespace tdoa 