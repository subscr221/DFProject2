/**
 * @file kalman_filter.h
 * @brief Kalman filter implementation for time synchronization
 */

#pragma once

#include <vector>
#include <cmath>
#include <stdexcept>

namespace tdoa {
namespace time_sync {

/**
 * @class KalmanFilter
 * @brief A Kalman filter implementation for time synchronization
 * 
 * This class implements a Kalman filter optimized for disciplining local clocks
 * using reference time sources like GPS. It tracks clock offset, drift, and aging
 * as state variables.
 */
class KalmanFilter {
public:
    /**
     * @brief Constructor
     * @param process_noise Process noise covariance
     * @param measurement_noise Measurement noise covariance
     * @param initial_estimate Initial state estimate
     * @param initial_error_covariance Initial error covariance
     */
    KalmanFilter(
        double process_noise = 1.0e-12,
        double measurement_noise = 1.0e-6,
        double initial_estimate = 0.0,
        double initial_error_covariance = 1.0
    )
        : x_(3, 0.0)  // State vector: [offset, drift, aging]
        , P_(3, 3, 0.0)  // Error covariance matrix
        , Q_(3, 3, 0.0)  // Process noise covariance matrix
        , R_(measurement_noise)  // Measurement noise covariance
        , last_update_time_(0)
        , initialized_(false)
    {
        // Initialize state vector
        x_[0] = initial_estimate;  // Offset (seconds)
        x_[1] = 0.0;               // Drift (seconds/second)
        x_[2] = 0.0;               // Aging (seconds/second^2)
        
        // Initialize error covariance matrix
        P_(0, 0) = initial_error_covariance;
        P_(1, 1) = 1.0e-8;  // Initial drift uncertainty
        P_(2, 2) = 1.0e-12; // Initial aging uncertainty
        
        // Initialize process noise covariance matrix
        Q_(0, 0) = process_noise;
        Q_(1, 1) = process_noise * 1.0e-4;
        Q_(2, 2) = process_noise * 1.0e-8;
    }
    
    /**
     * @brief Set measurement noise
     * @param noise Measurement noise covariance
     */
    void setMeasurementNoise(double noise) {
        R_ = noise;
    }
    
    /**
     * @brief Set process noise
     * @param noise Process noise covariance for offset
     * @param drift_noise Process noise covariance for drift
     * @param aging_noise Process noise covariance for aging
     */
    void setProcessNoise(double noise, double drift_noise = 0.0, double aging_noise = 0.0) {
        Q_(0, 0) = noise;
        Q_(1, 1) = drift_noise > 0.0 ? drift_noise : noise * 1.0e-4;
        Q_(2, 2) = aging_noise > 0.0 ? aging_noise : noise * 1.0e-8;
    }
    
    /**
     * @brief Reset the filter state
     * @param initial_offset Initial offset estimate
     * @param initial_error Initial error covariance
     */
    void reset(double initial_offset = 0.0, double initial_error = 1.0) {
        x_[0] = initial_offset;
        x_[1] = 0.0;
        x_[2] = 0.0;
        
        P_(0, 0) = initial_error;
        P_(1, 1) = 1.0e-8;
        P_(2, 2) = 1.0e-12;
        
        last_update_time_ = 0;
        initialized_ = false;
    }
    
    /**
     * @brief Update the filter with a new measurement
     * @param timestamp Time of measurement (ns)
     * @param measurement Measured offset (ns)
     * @param uncertainty Measurement uncertainty (ns)
     */
    void update(uint64_t timestamp, double measurement, double uncertainty = 0.0) {
        // Convert to seconds for filter
        double measurement_sec = measurement * 1.0e-9;
        
        // Use provided uncertainty if available
        if (uncertainty > 0.0) {
            // Convert ns^2 to s^2
            R_ = uncertainty * uncertainty * 1.0e-18;
        }
        
        // Initialize with first measurement
        if (!initialized_) {
            x_[0] = measurement_sec;
            last_update_time_ = timestamp;
            initialized_ = true;
            return;
        }
        
        // Compute time delta in seconds
        double dt = static_cast<double>(timestamp - last_update_time_) * 1.0e-9;
        if (dt <= 0.0) {
            // Invalid time delta
            return;
        }
        
        // Predict step: project state ahead
        // State transition matrix
        Matrix F(3, 3, 0.0);
        F(0, 0) = 1.0;
        F(0, 1) = dt;
        F(0, 2) = 0.5 * dt * dt;
        F(1, 1) = 1.0;
        F(1, 2) = dt;
        F(2, 2) = 1.0;
        
        // Project state ahead
        Vector x_pred = F * x_;
        
        // Project error covariance ahead
        Matrix P_pred = F * P_ * F.transpose() + Q_;
        
        // Update step: correct with measurement
        // Measurement matrix (we only measure offset)
        Vector H(3, 0.0);
        H[0] = 1.0;
        
        // Compute Kalman gain
        double S = (H.transpose() * P_pred * H)[0] + R_;
        Vector K = P_pred * H * (1.0 / S);
        
        // Update state with measurement
        x_ = x_pred + K * (measurement_sec - (H.transpose() * x_pred)[0]);
        
        // Update error covariance
        P_ = (Matrix::identity(3) - K * H.transpose()) * P_pred;
        
        // Store timestamp for next update
        last_update_time_ = timestamp;
    }
    
    /**
     * @brief Get the current offset estimate
     * @return Offset estimate in nanoseconds
     */
    double getOffset() const {
        return x_[0] * 1.0e9;  // Convert to nanoseconds
    }
    
    /**
     * @brief Get the current drift estimate
     * @return Drift estimate in ppb (parts per billion)
     */
    double getDrift() const {
        return x_[1] * 1.0e9;  // Convert to ppb
    }
    
    /**
     * @brief Get the current aging estimate
     * @return Aging estimate in ppb/day
     */
    double getAging() const {
        return x_[2] * 86400.0 * 1.0e9;  // Convert to ppb/day
    }
    
    /**
     * @brief Get uncertainty of the offset estimate
     * @return Uncertainty in nanoseconds
     */
    double getUncertainty() const {
        return std::sqrt(P_(0, 0)) * 1.0e9;  // Convert to nanoseconds
    }
    
    /**
     * @brief Predict the offset at a future time
     * @param timestamp Future timestamp in nanoseconds
     * @return Predicted offset in nanoseconds
     */
    double predict(uint64_t timestamp) const {
        if (!initialized_) {
            return 0.0;
        }
        
        double dt = static_cast<double>(timestamp - last_update_time_) * 1.0e-9;
        double predicted_offset = x_[0] + x_[1] * dt + 0.5 * x_[2] * dt * dt;
        
        return predicted_offset * 1.0e9;  // Convert to nanoseconds
    }

private:
    // Simple matrix/vector classes for Kalman filter implementation
    class Vector {
    public:
        Vector(size_t size, double initial_value = 0.0)
            : data_(size, initial_value)
            , size_(size)
        {}
        
        double& operator[](size_t index) {
            return data_[index];
        }
        
        const double& operator[](size_t index) const {
            return data_[index];
        }
        
        Vector operator+(const Vector& other) const {
            if (size_ != other.size_) {
                throw std::invalid_argument("Vector dimensions must match");
            }
            
            Vector result(size_);
            for (size_t i = 0; i < size_; ++i) {
                result[i] = data_[i] + other[i];
            }
            
            return result;
        }
        
        Vector operator*(double scalar) const {
            Vector result(size_);
            for (size_t i = 0; i < size_; ++i) {
                result[i] = data_[i] * scalar;
            }
            
            return result;
        }
        
        size_t size() const {
            return size_;
        }
        
    private:
        std::vector<double> data_;
        size_t size_;
    };
    
    class Matrix {
    public:
        Matrix(size_t rows, size_t cols, double initial_value = 0.0)
            : data_(rows * cols, initial_value)
            , rows_(rows)
            , cols_(cols)
        {}
        
        double& operator()(size_t row, size_t col) {
            return data_[row * cols_ + col];
        }
        
        const double& operator()(size_t row, size_t col) const {
            return data_[row * cols_ + col];
        }
        
        Vector operator*(const Vector& vec) const {
            if (cols_ != vec.size()) {
                throw std::invalid_argument("Matrix and vector dimensions must match");
            }
            
            Vector result(rows_);
            for (size_t i = 0; i < rows_; ++i) {
                double sum = 0.0;
                for (size_t j = 0; j < cols_; ++j) {
                    sum += (*this)(i, j) * vec[j];
                }
                result[i] = sum;
            }
            
            return result;
        }
        
        Matrix operator*(const Matrix& other) const {
            if (cols_ != other.rows_) {
                throw std::invalid_argument("Matrix dimensions must match");
            }
            
            Matrix result(rows_, other.cols_);
            for (size_t i = 0; i < rows_; ++i) {
                for (size_t j = 0; j < other.cols_; ++j) {
                    double sum = 0.0;
                    for (size_t k = 0; k < cols_; ++k) {
                        sum += (*this)(i, k) * other(k, j);
                    }
                    result(i, j) = sum;
                }
            }
            
            return result;
        }
        
        Matrix operator+(const Matrix& other) const {
            if (rows_ != other.rows_ || cols_ != other.cols_) {
                throw std::invalid_argument("Matrix dimensions must match");
            }
            
            Matrix result(rows_, cols_);
            for (size_t i = 0; i < rows_; ++i) {
                for (size_t j = 0; j < cols_; ++j) {
                    result(i, j) = (*this)(i, j) + other(i, j);
                }
            }
            
            return result;
        }
        
        Matrix transpose() const {
            Matrix result(cols_, rows_);
            for (size_t i = 0; i < rows_; ++i) {
                for (size_t j = 0; j < cols_; ++j) {
                    result(j, i) = (*this)(i, j);
                }
            }
            
            return result;
        }
        
        static Matrix identity(size_t size) {
            Matrix result(size, size);
            for (size_t i = 0; i < size; ++i) {
                result(i, i) = 1.0;
            }
            
            return result;
        }
        
    private:
        std::vector<double> data_;
        size_t rows_;
        size_t cols_;
    };
    
    Vector x_;                   ///< State vector: [offset, drift, aging]
    Matrix P_;                   ///< Error covariance matrix
    Matrix Q_;                   ///< Process noise covariance matrix
    double R_;                   ///< Measurement noise covariance
    uint64_t last_update_time_;  ///< Last update timestamp
    bool initialized_;           ///< Whether the filter has been initialized
};

} // namespace time_sync
} // namespace tdoa 