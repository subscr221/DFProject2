/**
 * @file temperature_compensation.h
 * @brief Temperature compensation for oscillator drift
 */

#pragma once

#include <map>
#include <vector>
#include <algorithm>
#include <cmath>

namespace tdoa {
namespace time_sync {

/**
 * @class TemperatureCompensation
 * @brief Class for temperature compensation of oscillator frequency drift
 * 
 * This class implements temperature compensation algorithms for crystal oscillators
 * by modeling the temperature vs. frequency relationship and calculating correction factors.
 */
class TemperatureCompensation {
public:
    /**
     * @brief Constructor
     * @param default_coefficient Default temperature coefficient in ppb/°C
     */
    TemperatureCompensation(double default_coefficient = -0.2)
        : enabled_(false)
        , default_coefficient_(default_coefficient)
        , reference_temperature_(25.0)
        , calibration_data_()
        , model_type_(ModelType::Linear)
        , polynomial_coefficients_({0.0, default_coefficient})
    {
    }
    
    /**
     * @enum ModelType
     * @brief Type of temperature compensation model
     */
    enum class ModelType {
        None,       ///< No compensation
        Linear,     ///< Linear model with one coefficient
        Quadratic,  ///< Quadratic model (second-order polynomial)
        Cubic,      ///< Cubic model (third-order polynomial)
        Spline      ///< Spline interpolation from calibration points
    };
    
    /**
     * @brief Enable or disable temperature compensation
     * @param enabled Whether compensation is enabled
     */
    void setEnabled(bool enabled) {
        enabled_ = enabled;
    }
    
    /**
     * @brief Check if temperature compensation is enabled
     * @return True if compensation is enabled
     */
    bool isEnabled() const {
        return enabled_;
    }
    
    /**
     * @brief Set reference temperature
     * @param temperature Reference temperature in °C
     */
    void setReferenceTemperature(double temperature) {
        reference_temperature_ = temperature;
    }
    
    /**
     * @brief Get reference temperature
     * @return Reference temperature in °C
     */
    double getReferenceTemperature() const {
        return reference_temperature_;
    }
    
    /**
     * @brief Set linear temperature coefficient
     * @param coefficient Temperature coefficient in ppb/°C
     */
    void setCoefficient(double coefficient) {
        default_coefficient_ = coefficient;
        
        if (model_type_ == ModelType::Linear) {
            polynomial_coefficients_ = {0.0, coefficient};
        }
    }
    
    /**
     * @brief Get linear temperature coefficient
     * @return Temperature coefficient in ppb/°C
     */
    double getCoefficient() const {
        return default_coefficient_;
    }
    
    /**
     * @brief Add a calibration point
     * @param temperature Temperature in °C
     * @param frequency_offset Frequency offset in ppb
     */
    void addCalibrationPoint(double temperature, double frequency_offset) {
        calibration_data_[temperature] = frequency_offset;
        
        // Update model if using spline interpolation
        if (model_type_ == ModelType::Spline) {
            computeSplineCoefficients();
        }
    }
    
    /**
     * @brief Clear all calibration points
     */
    void clearCalibrationPoints() {
        calibration_data_.clear();
    }
    
    /**
     * @brief Set the model type
     * @param type Type of compensation model
     */
    void setModelType(ModelType type) {
        model_type_ = type;
        
        switch (type) {
            case ModelType::None:
                polynomial_coefficients_ = {0.0};
                break;
                
            case ModelType::Linear:
                polynomial_coefficients_ = {0.0, default_coefficient_};
                break;
                
            case ModelType::Quadratic:
            case ModelType::Cubic:
                if (calibration_data_.size() >= 2) {
                    computePolynomialCoefficients();
                } else {
                    // Not enough data points, fall back to linear model
                    polynomial_coefficients_ = {0.0, default_coefficient_};
                }
                break;
                
            case ModelType::Spline:
                if (calibration_data_.size() >= 2) {
                    computeSplineCoefficients();
                } else {
                    // Not enough data points, fall back to linear model
                    polynomial_coefficients_ = {0.0, default_coefficient_};
                }
                break;
        }
    }
    
    /**
     * @brief Get the current model type
     * @return Type of compensation model
     */
    ModelType getModelType() const {
        return model_type_;
    }
    
    /**
     * @brief Compute compensation at a given temperature
     * @param temperature Current temperature in °C
     * @return Frequency compensation in ppb
     */
    double getCompensation(double temperature) const {
        if (!enabled_) {
            return 0.0;
        }
        
        switch (model_type_) {
            case ModelType::None:
                return 0.0;
                
            case ModelType::Linear:
            case ModelType::Quadratic:
            case ModelType::Cubic:
                return evaluatePolynomial(temperature - reference_temperature_);
                
            case ModelType::Spline:
                return interpolateSpline(temperature);
                
            default:
                return 0.0;
        }
    }
    
    /**
     * @brief Get the optimal model type for the current calibration data
     * @return Optimal model type
     */
    ModelType getOptimalModelType() const {
        size_t points = calibration_data_.size();
        
        if (points < 2) {
            return ModelType::Linear;  // Fall back to linear with default coefficient
        } else if (points < 3) {
            return ModelType::Linear;
        } else if (points < 4) {
            return ModelType::Quadratic;
        } else if (points < 8) {
            return ModelType::Cubic;
        } else {
            return ModelType::Spline;
        }
    }

private:
    /**
     * @brief Compute polynomial coefficients from calibration data
     */
    void computePolynomialCoefficients() {
        if (calibration_data_.size() < 2) {
            return;
        }
        
        // Extract data points
        std::vector<double> x_values;
        std::vector<double> y_values;
        
        for (const auto& pair : calibration_data_) {
            x_values.push_back(pair.first - reference_temperature_);
            y_values.push_back(pair.second);
        }
        
        // Determine polynomial order based on model type and data points
        int order;
        switch (model_type_) {
            case ModelType::Linear:
                order = 1;
                break;
            case ModelType::Quadratic:
                order = 2;
                break;
            case ModelType::Cubic:
                order = 3;
                break;
            default:
                order = 1;
                break;
        }
        
        // Limit order based on number of data points
        if (order >= static_cast<int>(calibration_data_.size())) {
            order = static_cast<int>(calibration_data_.size()) - 1;
        }
        
        // Solve polynomial coefficients using least squares
        int n = static_cast<int>(x_values.size());
        int m = order + 1;  // Number of coefficients
        
        // Create Vandermonde matrix
        std::vector<std::vector<double>> A(n, std::vector<double>(m));
        for (int i = 0; i < n; i++) {
            double x_pow = 1.0;
            for (int j = 0; j < m; j++) {
                A[i][j] = x_pow;
                x_pow *= x_values[i];
            }
        }
        
        // Transpose of A
        std::vector<std::vector<double>> AT(m, std::vector<double>(n));
        for (int i = 0; i < m; i++) {
            for (int j = 0; j < n; j++) {
                AT[i][j] = A[j][i];
            }
        }
        
        // Compute AT*A
        std::vector<std::vector<double>> ATA(m, std::vector<double>(m, 0.0));
        for (int i = 0; i < m; i++) {
            for (int j = 0; j < m; j++) {
                for (int k = 0; k < n; k++) {
                    ATA[i][j] += AT[i][k] * A[k][j];
                }
            }
        }
        
        // Compute AT*y
        std::vector<double> ATy(m, 0.0);
        for (int i = 0; i < m; i++) {
            for (int j = 0; j < n; j++) {
                ATy[i] += AT[i][j] * y_values[j];
            }
        }
        
        // Solve ATA*x = ATy using Gaussian elimination
        for (int i = 0; i < m; i++) {
            // Find pivot
            int max_row = i;
            double max_val = std::abs(ATA[i][i]);
            
            for (int j = i + 1; j < m; j++) {
                double val = std::abs(ATA[j][i]);
                if (val > max_val) {
                    max_val = val;
                    max_row = j;
                }
            }
            
            // Swap rows if needed
            if (max_row != i) {
                std::swap(ATA[i], ATA[max_row]);
                std::swap(ATy[i], ATy[max_row]);
            }
            
            // Eliminate below
            for (int j = i + 1; j < m; j++) {
                double factor = ATA[j][i] / ATA[i][i];
                
                for (int k = i; k < m; k++) {
                    ATA[j][k] -= factor * ATA[i][k];
                }
                
                ATy[j] -= factor * ATy[i];
            }
        }
        
        // Back substitution
        std::vector<double> coeffs(m);
        for (int i = m - 1; i >= 0; i--) {
            double sum = 0.0;
            for (int j = i + 1; j < m; j++) {
                sum += ATA[i][j] * coeffs[j];
            }
            
            coeffs[i] = (ATy[i] - sum) / ATA[i][i];
        }
        
        // Store coefficients
        polynomial_coefficients_ = coeffs;
    }
    
    /**
     * @brief Compute spline coefficients from calibration data
     */
    void computeSplineCoefficients() {
        // Spline coefficients are computed on-the-fly during interpolation
        // to keep this function simpler
    }
    
    /**
     * @brief Evaluate polynomial at a given point
     * @param x Point to evaluate at
     * @return Polynomial value
     */
    double evaluatePolynomial(double x) const {
        double result = 0.0;
        double x_pow = 1.0;
        
        for (double coeff : polynomial_coefficients_) {
            result += coeff * x_pow;
            x_pow *= x;
        }
        
        return result;
    }
    
    /**
     * @brief Interpolate using cubic spline
     * @param temperature Temperature to interpolate at
     * @return Interpolated frequency offset
     */
    double interpolateSpline(double temperature) const {
        if (calibration_data_.empty()) {
            return 0.0;
        }
        
        if (calibration_data_.size() == 1) {
            return calibration_data_.begin()->second;
        }
        
        // Find bracketing points
        auto it = calibration_data_.lower_bound(temperature);
        
        if (it == calibration_data_.begin()) {
            // Temperature is below or at the lowest calibration point
            return it->second;
        }
        
        if (it == calibration_data_.end()) {
            // Temperature is above the highest calibration point
            return (--it)->second;
        }
        
        // Get the two points that bracket the requested temperature
        auto pt2 = it;
        auto pt1 = --it;
        
        double t1 = pt1->first;
        double t2 = pt2->first;
        double f1 = pt1->second;
        double f2 = pt2->second;
        
        // Linear interpolation
        double t_norm = (temperature - t1) / (t2 - t1);
        
        return f1 + t_norm * (f2 - f1);
    }
    
    bool enabled_;                      ///< Whether compensation is enabled
    double default_coefficient_;        ///< Default temperature coefficient
    double reference_temperature_;      ///< Reference temperature
    std::map<double, double> calibration_data_; ///< Temperature vs. frequency offset
    ModelType model_type_;              ///< Type of compensation model
    std::vector<double> polynomial_coefficients_; ///< Polynomial coefficients
};

} // namespace time_sync
} // namespace tdoa 