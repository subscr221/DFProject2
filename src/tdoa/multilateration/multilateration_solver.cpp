/**
 * @file multilateration_solver.cpp
 * @brief Implementation of 2D multilateration solver for TDOA positioning
 */

#include "multilateration_solver.h"
#include <cmath>
#include <iostream>
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>

namespace tdoa {
namespace multilateration {

/**
 * @struct MultilaterationSolver::Impl
 * @brief Private implementation of MultilaterationSolver
 */
struct MultilaterationSolver::Impl {
    MultilaterationConfig config;
    PositionCallback positionCallback;
    
    // Least squares solution
    Position2D solveLeastSquares(
        const time_difference::TimeDifferenceSet& timeDiffs,
        const std::map<std::string, time_difference::SignalSource>& sources);
    
    // Taylor series solution
    Position2D solveTaylorSeries(
        const time_difference::TimeDifferenceSet& timeDiffs,
        const std::map<std::string, time_difference::SignalSource>& sources);
    
    // Bayesian solution
    Position2D solveBayesian(
        const time_difference::TimeDifferenceSet& timeDiffs,
        const std::map<std::string, time_difference::SignalSource>& sources);
    
    // Gradient descent solution
    Position2D solveGradientDescent(
        const time_difference::TimeDifferenceSet& timeDiffs,
        const std::map<std::string, time_difference::SignalSource>& sources);
    
    // Calculate distance between two points
    double calculateDistance(double x1, double y1, double x2, double y2) {
        return std::sqrt(std::pow(x2 - x1, 2) + std::pow(y2 - y1, 2));
    }
    
    // Check if enough sources for valid calculation
    bool hasEnoughSources(const std::map<std::string, time_difference::SignalSource>& sources,
                         const time_difference::TimeDifferenceSet& timeDiffs) {
        if (sources.size() < config.minRequiredSources) {
            return false;
        }
        if (timeDiffs.timeDifferences.size() < config.minRequiredTimeDiffs) {
            return false;
        }
        return true;
    }
    
    // Calculate time of arrival based on position and source location
    double calculateTOA(double sourceX, double sourceY, double targetX, double targetY) {
        return calculateDistance(sourceX, sourceY, targetX, targetY) / config.speedOfLight;
    }
};

MultilaterationSolver::MultilaterationSolver(const MultilaterationConfig& config)
    : pImpl(std::make_unique<Impl>())
{
    pImpl->config = config;
}

MultilaterationSolver::~MultilaterationSolver() = default;

MultilaterationResult MultilaterationSolver::calculatePosition(
    const time_difference::TimeDifferenceSet& timeDiffs,
    const std::map<std::string, time_difference::SignalSource>& sources)
{
    MultilaterationResult result;
    
    // Check if we have enough sources for calculation
    if (!pImpl->hasEnoughSources(sources, timeDiffs)) {
        result.valid = false;
        result.diagnosticMessage = "Not enough sources or time differences for calculation";
        return result;
    }
    
    // Calculate position based on selected method
    Position2D position;
    switch (pImpl->config.method) {
        case SolverMethod::LeastSquares:
            position = pImpl->solveLeastSquares(timeDiffs, sources);
            break;
        case SolverMethod::TaylorSeries:
            position = pImpl->solveTaylorSeries(timeDiffs, sources);
            break;
        case SolverMethod::Bayesian:
            position = pImpl->solveBayesian(timeDiffs, sources);
            break;
        case SolverMethod::GradientDescent:
            position = pImpl->solveGradientDescent(timeDiffs, sources);
            break;
        default:
            position = pImpl->solveTaylorSeries(timeDiffs, sources);
            break;
    }
    
    // Calculate uncertainty metrics
    result.position = position;
    result.gdop = calculateGDOP(sources, position);
    result.confidence = calculateConfidenceEllipse(position, pImpl->config.confidenceLevel);
    result.valid = true;
    
    // Call position callback if set
    if (pImpl->positionCallback) {
        pImpl->positionCallback(result);
    }
    
    return result;
}

void MultilaterationSolver::setPositionCallback(PositionCallback callback)
{
    pImpl->positionCallback = callback;
}

MultilaterationConfig MultilaterationSolver::getConfig() const
{
    return pImpl->config;
}

void MultilaterationSolver::setConfig(const MultilaterationConfig& config)
{
    pImpl->config = config;
}

GDOPInfo MultilaterationSolver::calculateGDOP(
    const std::map<std::string, time_difference::SignalSource>& sources,
    const Position2D& position)
{
    GDOPInfo gdopInfo;
    
    // Need at least 3 sources for GDOP calculation in 2D
    if (sources.size() < 3) {
        return gdopInfo;
    }
    
    // Build the geometry matrix
    Eigen::MatrixXd G(sources.size(), 3); // 3 columns: x, y, time bias
    int i = 0;
    
    for (const auto& source : sources) {
        const auto& pos = source.second.position;
        double distance = std::sqrt(std::pow(pos.x - position.x, 2) + 
                                   std::pow(pos.y - position.y, 2));
        
        // Unit vector from source to estimated position
        if (distance > 1e-10) {
            G(i, 0) = (position.x - pos.x) / distance;
            G(i, 1) = (position.y - pos.y) / distance;
            G(i, 2) = 1.0; // For clock bias
        } else {
            // Source and position are practically at the same location
            G(i, 0) = 0.0;
            G(i, 1) = 0.0;
            G(i, 2) = 1.0;
        }
        i++;
    }
    
    // Calculate GDOP from the geometry matrix
    Eigen::MatrixXd GTG = G.transpose() * G;
    Eigen::MatrixXd cov;
    
    // Check if matrix is invertible
    double det = GTG.determinant();
    if (std::abs(det) > 1e-10) {
        cov = GTG.inverse();
        
        // GDOP is the square root of the trace of the covariance matrix
        gdopInfo.gdop = std::sqrt(cov.trace());
        
        // PDOP (Position DOP) is the square root of the sum of the first two diagonal elements
        gdopInfo.pdop = std::sqrt(cov(0,0) + cov(1,1));
        
        // HDOP (Horizontal DOP) is the square root of the first diagonal element
        gdopInfo.hdop = std::sqrt(cov(0,0) + cov(1,1));
        
        // VDOP (Vertical DOP) is 0 in 2D
        gdopInfo.vdop = 0.0;
        
        // TDOP (Time DOP) is the square root of the third diagonal element
        gdopInfo.tdop = std::sqrt(cov(2,2));
    }
    
    return gdopInfo;
}

ConfidenceEllipse MultilaterationSolver::calculateConfidenceEllipse(
    const Position2D& position,
    double confidenceLevel)
{
    // Create a 2x2 covariance matrix from position uncertainty
    std::array<std::array<double, 2>, 2> covariance = {{
        {std::pow(position.uncertaintyX, 2), 0.0},
        {0.0, std::pow(position.uncertaintyY, 2)}
    }};
    
    return covarianceToEllipse(covariance, position, confidenceLevel);
}

ConfidenceEllipse MultilaterationSolver::covarianceToEllipse(
    const std::array<std::array<double, 2>, 2>& covariance,
    const Position2D& position,
    double confidenceLevel)
{
    ConfidenceEllipse ellipse;
    ellipse.centerX = position.x;
    ellipse.centerY = position.y;
    ellipse.confidenceLevel = confidenceLevel;
    
    // Convert to Eigen matrices for eigenvalue computation
    Eigen::Matrix2d covMatrix;
    covMatrix(0,0) = covariance[0][0];
    covMatrix(0,1) = covariance[0][1];
    covMatrix(1,0) = covariance[1][0];
    covMatrix(1,1) = covariance[1][1];
    
    // Compute eigenvalues and eigenvectors
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix2d> eigensolver(covMatrix);
    Eigen::Vector2d eigenvalues = eigensolver.eigenvalues();
    Eigen::Matrix2d eigenvectors = eigensolver.eigenvectors();
    
    // Convert eigenvalues to semi-axes
    // Chi-square value for 2 degrees of freedom at the given confidence level
    double chiSquare = 0.0;
    if (confidenceLevel >= 0.99) {
        chiSquare = 9.21; // 99% confidence
    } else if (confidenceLevel >= 0.95) {
        chiSquare = 5.99; // 95% confidence
    } else if (confidenceLevel >= 0.90) {
        chiSquare = 4.61; // 90% confidence
    } else if (confidenceLevel >= 0.70) {
        chiSquare = 2.41; // 70% confidence
    } else if (confidenceLevel >= 0.50) {
        chiSquare = 1.39; // 50% confidence
    } else {
        chiSquare = 1.0;  // Default
    }
    
    // Calculate semi-axes lengths
    ellipse.semiMajorAxis = std::sqrt(chiSquare * eigenvalues(1));
    ellipse.semiMinorAxis = std::sqrt(chiSquare * eigenvalues(0));
    
    // Calculate rotation angle from eigenvectors
    // The angle is the arctangent of the eigenvector components for the largest eigenvalue
    ellipse.rotationAngle = std::atan2(eigenvectors(1, 1), eigenvectors(0, 1));
    
    return ellipse;
}

Position2D MultilaterationSolver::Impl::solveLeastSquares(
    const time_difference::TimeDifferenceSet& timeDiffs,
    const std::map<std::string, time_difference::SignalSource>& sources)
{
    Position2D position;
    
    // Need at least 3 sources for a 2D solution
    if (sources.size() < 3) {
        position.uncertaintyX = 1000.0;
        position.uncertaintyY = 1000.0;
        position.confidence = 0.0;
        return position;
    }
    
    // Reference source is the first one in the sorted map
    std::string refSourceId = sources.begin()->first;
    const auto& refSource = sources.begin()->second;
    
    // Setup the matrices for least squares computation
    int numEquations = timeDiffs.timeDifferences.size();
    Eigen::MatrixXd A(numEquations, 2); // 2 unknowns: x and y
    Eigen::VectorXd b(numEquations);
    
    // Fill the matrices
    int row = 0;
    for (const auto& td : timeDiffs.timeDifferences) {
        // Skip if source IDs not found
        auto sourceIt = sources.find(td.sourceId);
        auto refIt = sources.find(td.referenceId);
        if (sourceIt == sources.end() || refIt == sources.end()) {
            continue;
        }
        
        const auto& source = sourceIt->second;
        const auto& reference = refIt->second;
        
        // Distance difference from TDOA
        double distDiff = td.timeDifference * config.speedOfLight;
        
        // Coordinates
        double x1 = source.position.x;
        double y1 = source.position.y;
        double x2 = reference.position.x;
        double y2 = reference.position.y;
        
        // Distance from each source to an arbitrary point
        double r1 = std::sqrt(std::pow(x1, 2) + std::pow(y1, 2));
        double r2 = std::sqrt(std::pow(x2, 2) + std::pow(y2, 2));
        
        // Fill matrix row
        A(row, 0) = 2 * (x2 - x1);
        A(row, 1) = 2 * (y2 - y1);
        b(row) = std::pow(distDiff, 2) + std::pow(r1, 2) - std::pow(r2, 2) - 2 * distDiff * r1;
        
        row++;
    }
    
    // Check if we have enough valid equations
    if (row < 2) {
        position.uncertaintyX = 1000.0;
        position.uncertaintyY = 1000.0;
        position.confidence = 0.0;
        return position;
    }
    
    // Resize matrices to actual number of equations
    if (row < numEquations) {
        A.conservativeResize(row, 2);
        b.conservativeResize(row);
    }
    
    // Solve using least squares (A^T A)^-1 A^T b
    Eigen::MatrixXd ATA = A.transpose() * A;
    
    // Check if matrix is invertible
    if (ATA.determinant() < 1e-10) {
        // Poor geometry, use pseudo-inverse instead
        Eigen::VectorXd x = A.bdcSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(b);
        position.x = x(0);
        position.y = x(1);
    } else {
        // Solve using normal equation
        Eigen::VectorXd x = ATA.inverse() * A.transpose() * b;
        position.x = x(0);
        position.y = x(1);
    }
    
    // Calculate residuals to estimate uncertainty
    Eigen::VectorXd residuals = A * Eigen::Vector2d(position.x, position.y) - b;
    double residualSumSquares = residuals.squaredNorm();
    double variance = residualSumSquares / (row - 2); // 2 degrees of freedom (x,y)
    
    // Estimate uncertainties from covariance matrix
    Eigen::MatrixXd covariance = variance * ATA.inverse();
    position.uncertaintyX = std::sqrt(covariance(0, 0));
    position.uncertaintyY = std::sqrt(covariance(1, 1));
    
    // Calculate confidence from residuals
    double normalizedResidual = std::sqrt(residualSumSquares / row) / config.speedOfLight;
    position.confidence = std::exp(-normalizedResidual / 1.0e-6); // Scale factor based on typical timing error
    position.confidence = std::max(0.0, std::min(1.0, position.confidence)); // Clamp to [0,1]
    
    // Apply constraints if needed
    if (config.constrainToRegion) {
        position.x = std::max(config.regionMinX, std::min(config.regionMaxX, position.x));
        position.y = std::max(config.regionMinY, std::min(config.regionMaxY, position.y));
    }
    
    return position;
}

Position2D MultilaterationSolver::Impl::solveTaylorSeries(
    const time_difference::TimeDifferenceSet& timeDiffs,
    const std::map<std::string, time_difference::SignalSource>& sources)
{
    Position2D position;
    
    // Need at least 3 sources for a 2D solution
    if (sources.size() < 3) {
        position.uncertaintyX = 1000.0;
        position.uncertaintyY = 1000.0;
        position.confidence = 0.0;
        return position;
    }
    
    // Initialize solution with centroid of receiver positions as starting point
    double sumX = 0.0, sumY = 0.0;
    for (const auto& sourceEntry : sources) {
        sumX += sourceEntry.second.position.x;
        sumY += sourceEntry.second.position.y;
    }
    position.x = sumX / sources.size();
    position.y = sumY / sources.size();
    
    // Setup for iteration
    int iterations = 0;
    double prevX = position.x + 1000.0; // Ensure first iteration occurs
    double prevY = position.y + 1000.0;
    double delta = 1000.0;
    
    // Iterative solution using Taylor series linearization
    while (delta > config.convergenceThreshold && iterations < config.maxIterations) {
        // Setup matrices for this iteration
        int numEquations = timeDiffs.timeDifferences.size();
        Eigen::MatrixXd H(numEquations, 2); // Jacobian matrix
        Eigen::VectorXd deltaY(numEquations); // Measurement-prediction difference
        
        // Fill the matrices
        int row = 0;
        for (const auto& td : timeDiffs.timeDifferences) {
            // Skip if source IDs not found
            auto sourceIt = sources.find(td.sourceId);
            auto refIt = sources.find(td.referenceId);
            if (sourceIt == sources.end() || refIt == sources.end()) {
                continue;
            }
            
            const auto& source = sourceIt->second;
            const auto& reference = refIt->second;
            
            // Calculate theoretical time difference for current position estimate
            double d1 = calculateDistance(position.x, position.y, source.position.x, source.position.y);
            double d2 = calculateDistance(position.x, position.y, reference.position.x, reference.position.y);
            double predictedTimeDiff = (d1 - d2) / config.speedOfLight;
            
            // Calculate the Jacobian (partial derivatives)
            // Partial derivatives for source
            double dx1 = (position.x - source.position.x) / (d1 * config.speedOfLight);
            double dy1 = (position.y - source.position.y) / (d1 * config.speedOfLight);
            
            // Partial derivatives for reference
            double dx2 = (position.x - reference.position.x) / (d2 * config.speedOfLight);
            double dy2 = (position.y - reference.position.y) / (d2 * config.speedOfLight);
            
            // Combine partial derivatives
            H(row, 0) = dx1 - dx2;
            H(row, 1) = dy1 - dy2;
            
            // Measurement difference (observed - predicted)
            deltaY(row) = td.timeDifference - predictedTimeDiff;
            
            row++;
        }
        
        // Check if we have enough valid equations
        if (row < 2) {
            position.uncertaintyX = 1000.0;
            position.uncertaintyY = 1000.0;
            position.confidence = 0.0;
            return position;
        }
        
        // Resize matrices if needed
        if (row < numEquations) {
            H.conservativeResize(row, 2);
            deltaY.conservativeResize(row);
        }
        
        // Compute position update using least squares: dX = (H^T H)^-1 H^T deltaY
        Eigen::MatrixXd HTH = H.transpose() * H;
        Eigen::Vector2d deltaPos;
        
        if (HTH.determinant() < 1e-10) {
            // Poor geometry, use SVD
            deltaPos = H.bdcSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(deltaY);
        } else {
            // Use normal equations
            deltaPos = HTH.inverse() * H.transpose() * deltaY;
        }
        
        // Update position estimate
        prevX = position.x;
        prevY = position.y;
        position.x += deltaPos(0);
        position.y += deltaPos(1);
        
        // Enforce constraints if enabled
        if (config.constrainToRegion) {
            position.x = std::max(config.regionMinX, std::min(config.regionMaxX, position.x));
            position.y = std::max(config.regionMinY, std::min(config.regionMaxY, position.y));
        }
        
        // Calculate convergence metric
        delta = std::sqrt(std::pow(position.x - prevX, 2) + std::pow(position.y - prevY, 2));
        iterations++;
    }
    
    // Calculate position uncertainty from the Jacobian at the solution point
    int numEquations = timeDiffs.timeDifferences.size();
    Eigen::MatrixXd H(numEquations, 2);
    Eigen::VectorXd residuals(numEquations);
    
    // Recalculate Jacobian at the final position
    int row = 0;
    for (const auto& td : timeDiffs.timeDifferences) {
        auto sourceIt = sources.find(td.sourceId);
        auto refIt = sources.find(td.referenceId);
        if (sourceIt == sources.end() || refIt == sources.end()) {
            continue;
        }
        
        const auto& source = sourceIt->second;
        const auto& reference = refIt->second;
        
        // Calculate distances and time differences
        double d1 = calculateDistance(position.x, position.y, source.position.x, source.position.y);
        double d2 = calculateDistance(position.x, position.y, reference.position.x, reference.position.y);
        double predictedTimeDiff = (d1 - d2) / config.speedOfLight;
        
        // Jacobian components
        double dx1 = (position.x - source.position.x) / (d1 * config.speedOfLight);
        double dy1 = (position.y - source.position.y) / (d1 * config.speedOfLight);
        double dx2 = (position.x - reference.position.x) / (d2 * config.speedOfLight);
        double dy2 = (position.y - reference.position.y) / (d2 * config.speedOfLight);
        
        H(row, 0) = dx1 - dx2;
        H(row, 1) = dy1 - dy2;
        
        // Residuals
        residuals(row) = td.timeDifference - predictedTimeDiff;
        row++;
    }
    
    // Resize matrices if needed
    if (row < numEquations) {
        H.conservativeResize(row, 2);
        residuals.conservativeResize(row);
    }
    
    // Calculate covariance matrix
    Eigen::MatrixXd HTH = H.transpose() * H;
    double residualSumSquares = residuals.squaredNorm();
    double variance = residualSumSquares / (row - 2); // 2 degrees of freedom (x, y)
    
    Eigen::MatrixXd covariance;
    if (HTH.determinant() > 1e-10) {
        covariance = variance * HTH.inverse();
        position.uncertaintyX = std::sqrt(covariance(0, 0));
        position.uncertaintyY = std::sqrt(covariance(1, 1));
    } else {
        // Poor geometry, set large uncertainties
        position.uncertaintyX = 1000.0;
        position.uncertaintyY = 1000.0;
    }
    
    // Calculate confidence from residuals and convergence
    double normalizedResidual = std::sqrt(residualSumSquares / row) / config.speedOfLight;
    double iterationPenalty = static_cast<double>(iterations) / config.maxIterations;
    position.confidence = std::exp(-normalizedResidual / 1.0e-6) * (1.0 - 0.5 * iterationPenalty);
    position.confidence = std::max(0.0, std::min(1.0, position.confidence)); // Clamp to [0,1]
    
    return position;
}

Position2D MultilaterationSolver::Impl::solveBayesian(
    const time_difference::TimeDifferenceSet& timeDiffs,
    const std::map<std::string, time_difference::SignalSource>& sources)
{
    Position2D position;
    
    // Implementation of Bayesian solution will be added later
    // For now, return a position with high uncertainty
    position.uncertaintyX = 100.0;
    position.uncertaintyY = 100.0;
    position.confidence = 0.1;
    
    return position;
}

Position2D MultilaterationSolver::Impl::solveGradientDescent(
    const time_difference::TimeDifferenceSet& timeDiffs,
    const std::map<std::string, time_difference::SignalSource>& sources)
{
    Position2D position;
    
    // Implementation of gradient descent solution will be added later
    // For now, return a position with high uncertainty
    position.uncertaintyX = 100.0;
    position.uncertaintyY = 100.0;
    position.confidence = 0.1;
    
    return position;
}

} // namespace multilateration
} // namespace tdoa 