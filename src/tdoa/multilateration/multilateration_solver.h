/**
 * @file multilateration_solver.h
 * @brief 2D multilateration solver for TDOA positioning
 */

#pragma once

#include "../time_difference/time_difference_extractor.h"
#include <vector>
#include <memory>
#include <functional>
#include <array>

namespace tdoa {
namespace multilateration {

/**
 * @struct Position2D
 * @brief 2D position with uncertainty
 */
struct Position2D {
    double x;                   ///< X coordinate in meters
    double y;                   ///< Y coordinate in meters
    double uncertaintyX;        ///< X uncertainty in meters
    double uncertaintyY;        ///< Y uncertainty in meters
    double confidence;          ///< Position confidence (0-1)
    uint64_t timestamp;         ///< Timestamp when position was calculated
    
    /**
     * @brief Constructor with default values
     */
    Position2D()
        : x(0.0)
        , y(0.0)
        , uncertaintyX(0.0)
        , uncertaintyY(0.0)
        , confidence(0.0)
        , timestamp(0)
    {}
    
    /**
     * @brief Constructor with position and uncertainties
     */
    Position2D(double xPos, double yPos, 
              double xUncert = 0.0, double yUncert = 0.0, 
              double conf = 1.0, uint64_t time = 0)
        : x(xPos)
        , y(yPos)
        , uncertaintyX(xUncert)
        , uncertaintyY(yUncert)
        , confidence(conf)
        , timestamp(time)
    {}
};

/**
 * @struct ConfidenceEllipse
 * @brief Confidence ellipse for position uncertainty
 */
struct ConfidenceEllipse {
    double centerX;             ///< X coordinate of ellipse center in meters
    double centerY;             ///< Y coordinate of ellipse center in meters
    double semiMajorAxis;       ///< Semi-major axis in meters
    double semiMinorAxis;       ///< Semi-minor axis in meters
    double rotationAngle;       ///< Rotation angle in radians
    double confidenceLevel;     ///< Confidence level (e.g., 0.95 for 95%)
    
    /**
     * @brief Constructor with default values
     */
    ConfidenceEllipse()
        : centerX(0.0)
        , centerY(0.0)
        , semiMajorAxis(0.0)
        , semiMinorAxis(0.0)
        , rotationAngle(0.0)
        , confidenceLevel(0.95)
    {}
};

/**
 * @enum SolverMethod
 * @brief Method used for multilateration
 */
enum class SolverMethod {
    LeastSquares,           ///< Least squares solution
    TaylorSeries,           ///< Taylor series linearization
    Bayesian,               ///< Bayesian estimation
    GradientDescent         ///< Gradient descent optimization
};

/**
 * @struct MultilaterationConfig
 * @brief Configuration for multilateration solver
 */
struct MultilaterationConfig {
    SolverMethod method;                ///< Solver method
    double speedOfLight;                ///< Speed of light in m/s
    double convergenceThreshold;        ///< Convergence threshold for iterative methods
    int maxIterations;                  ///< Maximum iterations for iterative methods
    double confidenceLevel;             ///< Confidence level for uncertainty calculation
    double minRequiredSources;          ///< Minimum number of sources required for solution
    double minRequiredTimeDiffs;        ///< Minimum number of time differences required
    bool constrainToRegion;             ///< Whether to constrain solution to a region
    double regionMinX;                  ///< Region minimum X coordinate
    double regionMaxX;                  ///< Region maximum X coordinate
    double regionMinY;                  ///< Region minimum Y coordinate
    double regionMaxY;                  ///< Region maximum Y coordinate
    
    /**
     * @brief Constructor with default values
     */
    MultilaterationConfig()
        : method(SolverMethod::TaylorSeries)
        , speedOfLight(299792458.0)  // Speed of light in m/s
        , convergenceThreshold(1e-6)
        , maxIterations(20)
        , confidenceLevel(0.95)      // 95% confidence
        , minRequiredSources(3)      // Need at least 3 receivers for 2D
        , minRequiredTimeDiffs(2)    // Need at least 2 time differences
        , constrainToRegion(false)
        , regionMinX(-1000.0)        // Default region is 2km x 2km
        , regionMaxX(1000.0)
        , regionMinY(-1000.0)
        , regionMaxY(1000.0)
    {}
};

/**
 * @struct GDOPInfo
 * @brief Geometric dilution of precision information
 */
struct GDOPInfo {
    double gdop;                ///< Geometric dilution of precision
    double pdop;                ///< Position dilution of precision
    double hdop;                ///< Horizontal dilution of precision
    double vdop;                ///< Vertical dilution of precision
    double tdop;                ///< Time dilution of precision
    
    /**
     * @brief Constructor with default values
     */
    GDOPInfo()
        : gdop(0.0)
        , pdop(0.0)
        , hdop(0.0)
        , vdop(0.0)
        , tdop(0.0)
    {}
};

/**
 * @struct MultilaterationResult
 * @brief Result of multilateration calculation
 */
struct MultilaterationResult {
    Position2D position;            ///< Estimated position
    ConfidenceEllipse confidence;   ///< Confidence ellipse
    GDOPInfo gdop;                  ///< GDOP information
    int iterations;                 ///< Number of iterations used
    double residualError;           ///< Residual error
    bool valid;                     ///< Whether the result is valid
    std::string diagnosticMessage;  ///< Diagnostic message
    
    /**
     * @brief Constructor with default values
     */
    MultilaterationResult()
        : iterations(0)
        , residualError(0.0)
        , valid(false)
        , diagnosticMessage("")
    {}
};

/**
 * @class MultilaterationSolver
 * @brief Solver for 2D multilateration
 * 
 * This class implements various algorithms for solving the multilateration
 * problem to estimate the position of a signal source based on time differences
 * of arrival (TDOA) measurements.
 */
class MultilaterationSolver {
public:
    /**
     * @brief Position calculation callback
     */
    using PositionCallback = std::function<void(const MultilaterationResult&)>;
    
    /**
     * @brief Constructor
     * @param config Configuration for multilateration
     */
    MultilaterationSolver(const MultilaterationConfig& config = MultilaterationConfig());
    
    /**
     * @brief Destructor
     */
    ~MultilaterationSolver();
    
    /**
     * @brief Calculate position from time differences
     * @param timeDiffs Set of time differences
     * @param sources Map of source IDs to signal sources
     * @return Multilateration result
     */
    MultilaterationResult calculatePosition(
        const time_difference::TimeDifferenceSet& timeDiffs,
        const std::map<std::string, time_difference::SignalSource>& sources);
    
    /**
     * @brief Set position callback
     * @param callback Function to call when new position is calculated
     */
    void setPositionCallback(PositionCallback callback);
    
    /**
     * @brief Get configuration
     * @return Current configuration
     */
    MultilaterationConfig getConfig() const;
    
    /**
     * @brief Set configuration
     * @param config New configuration
     */
    void setConfig(const MultilaterationConfig& config);
    
    /**
     * @brief Calculate geometric dilution of precision
     * @param sources Map of source IDs to signal sources
     * @param position 2D position
     * @return GDOP information
     */
    static GDOPInfo calculateGDOP(
        const std::map<std::string, time_difference::SignalSource>& sources,
        const Position2D& position);
    
    /**
     * @brief Calculate confidence ellipse for a position
     * @param position 2D position with uncertainties
     * @param confidenceLevel Confidence level (e.g., 0.95 for 95%)
     * @return Confidence ellipse
     */
    static ConfidenceEllipse calculateConfidenceEllipse(
        const Position2D& position,
        double confidenceLevel = 0.95);
    
    /**
     * @brief Convert covariance matrix to confidence ellipse
     * @param covariance 2x2 covariance matrix
     * @param position 2D position
     * @param confidenceLevel Confidence level (e.g., 0.95 for 95%)
     * @return Confidence ellipse
     */
    static ConfidenceEllipse covarianceToEllipse(
        const std::array<std::array<double, 2>, 2>& covariance,
        const Position2D& position,
        double confidenceLevel = 0.95);
    
private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace multilateration
} // namespace tdoa 