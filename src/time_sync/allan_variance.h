/**
 * @file allan_variance.h
 * @brief Allan variance calculation for clock stability measurement
 */

#pragma once

#include <vector>
#include <map>
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace tdoa {
namespace time_sync {

/**
 * @class AllanVariance
 * @brief Class for calculating Allan variance to measure clock stability
 * 
 * This class implements algorithms to calculate Allan variance and Allan
 * deviation for time series data. It supports multi-tau analysis to
 * characterize clock stability across different averaging times.
 */
class AllanVariance {
public:
    /**
     * @brief Constructor
     * @param max_samples Maximum number of samples to store
     */
    AllanVariance(size_t max_samples = 1024)
        : max_samples_(max_samples)
        , time_data_()
        , value_data_()
    {
    }
    
    /**
     * @brief Add a time/value pair to the dataset
     * @param timestamp Time of measurement (ns)
     * @param value Measured value
     */
    void addSample(uint64_t timestamp, double value) {
        // Add the sample
        time_data_.push_back(timestamp);
        value_data_.push_back(value);
        
        // Limit the number of samples
        if (time_data_.size() > max_samples_) {
            time_data_.erase(time_data_.begin());
            value_data_.erase(value_data_.begin());
        }
        
        // Clear cached results
        allan_variance_.clear();
    }
    
    /**
     * @brief Reset the dataset
     */
    void reset() {
        time_data_.clear();
        value_data_.clear();
        allan_variance_.clear();
    }
    
    /**
     * @brief Get the number of samples
     * @return Number of samples in the dataset
     */
    size_t getSampleCount() const {
        return time_data_.size();
    }
    
    /**
     * @brief Calculate the Allan variance for a specific tau value
     * @param tau Averaging time in seconds
     * @return Allan variance at the specified tau
     */
    double calculateVariance(double tau) {
        if (time_data_.size() < 3) {
            return 0.0;  // Not enough data
        }
        
        // Check if we have a cached result
        auto it = allan_variance_.find(tau);
        if (it != allan_variance_.end()) {
            return it->second;
        }
        
        // Convert tau to nanoseconds
        uint64_t tau_ns = static_cast<uint64_t>(tau * 1.0e9);
        
        // Find the closest averaging factor
        size_t m = findBestAveragingFactor(tau_ns);
        if (m == 0) {
            return 0.0;  // No suitable averaging factor
        }
        
        // Calculate Allan variance
        double sum = 0.0;
        size_t n = 0;
        
        for (size_t i = 0; i + 2 * m <= value_data_.size(); i++) {
            double y1 = 0.0;
            double y2 = 0.0;
            
            // Calculate first block average
            for (size_t j = 0; j < m; j++) {
                y1 += value_data_[i + j];
            }
            y1 /= m;
            
            // Calculate second block average
            for (size_t j = 0; j < m; j++) {
                y2 += value_data_[i + m + j];
            }
            y2 /= m;
            
            // Add squared difference
            double diff = y2 - y1;
            sum += diff * diff;
            n++;
        }
        
        // Calculate variance
        double variance = (n > 0) ? (0.5 * sum / n) : 0.0;
        
        // Cache the result
        allan_variance_[tau] = variance;
        
        return variance;
    }
    
    /**
     * @brief Calculate the Allan deviation for a specific tau value
     * @param tau Averaging time in seconds
     * @return Allan deviation at the specified tau
     */
    double calculateDeviation(double tau) {
        double variance = calculateVariance(tau);
        return std::sqrt(variance);
    }
    
    /**
     * @brief Calculate Allan deviation for multiple tau values
     * @param min_tau Minimum averaging time in seconds
     * @param max_tau Maximum averaging time in seconds
     * @param points Number of points to calculate
     * @return Map of tau values to Allan deviation
     */
    std::map<double, double> calculateMultiTau(double min_tau = 1.0, double max_tau = 1000.0, size_t points = 10) {
        std::map<double, double> result;
        
        if (min_tau <= 0.0 || max_tau <= min_tau || points == 0) {
            return result;
        }
        
        // Calculate log-spaced tau values
        double log_min = std::log10(min_tau);
        double log_max = std::log10(max_tau);
        double step = (log_max - log_min) / (points - 1);
        
        for (size_t i = 0; i < points; i++) {
            double log_tau = log_min + i * step;
            double tau = std::pow(10.0, log_tau);
            
            double deviation = calculateDeviation(tau);
            result[tau] = deviation;
        }
        
        return result;
    }
    
    /**
     * @brief Get the noise type from Allan deviation slope
     * @param min_tau Minimum averaging time in seconds
     * @param max_tau Maximum averaging time in seconds
     * @return Dominant noise type and slope
     */
    std::pair<std::string, double> getNoiseType(double min_tau = 1.0, double max_tau = 100.0) {
        auto deviations = calculateMultiTau(min_tau, max_tau, 10);
        
        if (deviations.size() < 2) {
            return {"Unknown", 0.0};
        }
        
        // Calculate log-log slope
        std::vector<double> log_tau;
        std::vector<double> log_adev;
        
        for (const auto& pair : deviations) {
            if (pair.second > 0.0) {
                log_tau.push_back(std::log10(pair.first));
                log_adev.push_back(std::log10(pair.second));
            }
        }
        
        if (log_tau.size() < 2) {
            return {"Unknown", 0.0};
        }
        
        // Simple linear regression on log-log scale
        double sum_x = 0.0;
        double sum_y = 0.0;
        double sum_xy = 0.0;
        double sum_xx = 0.0;
        
        for (size_t i = 0; i < log_tau.size(); i++) {
            sum_x += log_tau[i];
            sum_y += log_adev[i];
            sum_xy += log_tau[i] * log_adev[i];
            sum_xx += log_tau[i] * log_tau[i];
        }
        
        double n = static_cast<double>(log_tau.size());
        double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x);
        
        // Determine noise type based on slope
        std::string noise_type;
        
        if (slope < -0.9) {
            noise_type = "White Phase Noise";
        } else if (slope < -0.4) {
            noise_type = "Flicker Phase Noise";
        } else if (slope < 0.1) {
            noise_type = "White Frequency Noise";
        } else if (slope < 0.6) {
            noise_type = "Flicker Frequency Noise";
        } else {
            noise_type = "Random Walk Frequency Noise";
        }
        
        return {noise_type, slope};
    }
    
private:
    /**
     * @brief Find the best averaging factor for a given tau
     * @param tau_ns Averaging time in nanoseconds
     * @return Averaging factor
     */
    size_t findBestAveragingFactor(uint64_t tau_ns) {
        if (time_data_.size() < 2) {
            return 0;
        }
        
        // Calculate average sampling interval
        uint64_t total_time = time_data_.back() - time_data_.front();
        double avg_interval = static_cast<double>(total_time) / (time_data_.size() - 1);
        
        // Calculate ideal averaging factor
        size_t m = static_cast<size_t>(std::round(tau_ns / avg_interval));
        
        // Ensure we have at least 3 blocks for variance calculation
        if (m * 3 > time_data_.size()) {
            m = time_data_.size() / 3;
        }
        
        return (m > 0) ? m : 1;
    }
    
    size_t max_samples_;                 ///< Maximum number of samples to store
    std::vector<uint64_t> time_data_;    ///< Timestamp data (ns)
    std::vector<double> value_data_;     ///< Measurement data
    std::map<double, double> allan_variance_; ///< Cached variance results
};

} // namespace time_sync
} // namespace tdoa 