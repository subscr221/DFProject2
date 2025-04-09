/**
 * @file multilateration_example.cpp
 * @brief Example demonstrating 2D multilateration using TDOA
 */

#include "tdoa/multilateration/multilateration_solver.h"
#include <iostream>
#include <iomanip>
#include <random>
#include <chrono>
#include <vector>
#include <cmath>

/**
 * @brief Simulates TDOA measurements from known receiver locations and transmitter position
 */
class TDOASimulator {
public:
    /**
     * @brief Constructor
     * @param speedOfLight Speed of light in m/s
     * @param timeUncertainty Uncertainty in time measurements in seconds
     */
    TDOASimulator(double speedOfLight = 299792458.0, double timeUncertainty = 1.0e-9)
        : speedOfLight(speedOfLight)
        , timeUncertainty(timeUncertainty)
        , rng(std::chrono::system_clock::now().time_since_epoch().count())
    {
    }
    
    /**
     * @brief Add a receiver
     * @param id Receiver ID
     * @param x X coordinate
     * @param y Y coordinate
     */
    void addReceiver(const std::string& id, double x, double y) {
        tdoa::time_difference::SignalSource source;
        source.id = id;
        source.position.x = x;
        source.position.y = y;
        source.position.z = 0.0;
        
        receivers[id] = source;
    }
    
    /**
     * @brief Simulate TDOA measurements
     * @param txX Transmitter X coordinate
     * @param txY Transmitter Y coordinate
     * @return Set of time differences
     */
    tdoa::time_difference::TimeDifferenceSet simulateMeasurement(double txX, double txY) {
        tdoa::time_difference::TimeDifferenceSet result;
        result.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // Calculate true time of arrivals
        std::map<std::string, double> toaMap;
        for (const auto& receiver : receivers) {
            double dx = txX - receiver.second.position.x;
            double dy = txY - receiver.second.position.y;
            double distance = std::sqrt(dx*dx + dy*dy);
            
            // Time of arrival = distance / speed of light
            double toa = distance / speedOfLight;
            
            // Add noise
            std::normal_distribution<double> noise(0.0, timeUncertainty);
            toa += noise(rng);
            
            toaMap[receiver.first] = toa;
        }
        
        // Create time difference measurements
        if (receivers.size() < 2) {
            return result;
        }
        
        // Use the first receiver as reference
        std::string refId = receivers.begin()->first;
        double refToa = toaMap[refId];
        
        // Calculate time differences
        for (const auto& entry : toaMap) {
            if (entry.first != refId) {
                tdoa::time_difference::TimeDifference td;
                td.sourceId = entry.first;
                td.referenceId = refId;
                td.timeDifference = entry.second - refToa;
                td.confidence = 0.95; // High confidence for simulated data
                
                result.timeDifferences.push_back(td);
            }
        }
        
        return result;
    }
    
    /**
     * @brief Get the signal sources
     * @return Map of receiver ID to signal source
     */
    const std::map<std::string, tdoa::time_difference::SignalSource>& getReceivers() const {
        return receivers;
    }
    
private:
    double speedOfLight;
    double timeUncertainty;
    std::map<std::string, tdoa::time_difference::SignalSource> receivers;
    std::mt19937 rng;
};

/**
 * @brief Prints ellipse information
 * @param ellipse Confidence ellipse
 */
void printEllipse(const tdoa::multilateration::ConfidenceEllipse& ellipse) {
    std::cout << "Confidence Ellipse:" << std::endl;
    std::cout << "  Center: (" << ellipse.centerX << ", " << ellipse.centerY << ")" << std::endl;
    std::cout << "  Semi-major axis: " << ellipse.semiMajorAxis << " m" << std::endl;
    std::cout << "  Semi-minor axis: " << ellipse.semiMinorAxis << " m" << std::endl;
    std::cout << "  Rotation angle: " << ellipse.rotationAngle * 180.0 / M_PI << " degrees" << std::endl;
    std::cout << "  Confidence level: " << ellipse.confidenceLevel * 100.0 << "%" << std::endl;
}

/**
 * @brief Main function
 */
int main() {
    // Create a simulator
    TDOASimulator simulator;
    
    // Add receivers in a good geometric configuration
    simulator.addReceiver("R1", -1000.0, -1000.0);
    simulator.addReceiver("R2", 1000.0, -1000.0);
    simulator.addReceiver("R3", 0.0, 1000.0);
    simulator.addReceiver("R4", -500.0, 500.0);
    
    // Create multilateration solver
    tdoa::multilateration::MultilaterationConfig config;
    config.method = tdoa::multilateration::SolverMethod::TaylorSeries;
    config.confidenceLevel = 0.95;
    
    tdoa::multilateration::MultilaterationSolver solver(config);
    
    // True transmitter position
    double trueX = 250.0;
    double trueY = 300.0;
    
    std::cout << "True Transmitter Position: (" << trueX << ", " << trueY << ")" << std::endl;
    std::cout << std::endl;
    
    // Simulate TDOA measurement
    tdoa::time_difference::TimeDifferenceSet tdoa = simulator.simulateMeasurement(trueX, trueY);
    
    // Print time differences
    std::cout << "Simulated Time Differences:" << std::endl;
    for (const auto& td : tdoa.timeDifferences) {
        std::cout << "  " << td.sourceId << " - " << td.referenceId 
                  << ": " << td.timeDifference * 1.0e9 << " ns" << std::endl;
    }
    std::cout << std::endl;
    
    // Calculate position using different methods
    std::cout << "Least Squares Solution:" << std::endl;
    config.method = tdoa::multilateration::SolverMethod::LeastSquares;
    solver.setConfig(config);
    
    tdoa::multilateration::MultilaterationResult lsResult = 
        solver.calculatePosition(tdoa, simulator.getReceivers());
    
    std::cout << "  Estimated Position: (" 
              << lsResult.position.x << ", " << lsResult.position.y << ")" << std::endl;
    std::cout << "  Error: " 
              << std::sqrt(std::pow(lsResult.position.x - trueX, 2) + 
                         std::pow(lsResult.position.y - trueY, 2)) << " m" << std::endl;
    std::cout << "  Confidence: " << lsResult.position.confidence * 100.0 << "%" << std::endl;
    std::cout << "  X Uncertainty: " << lsResult.position.uncertaintyX << " m" << std::endl;
    std::cout << "  Y Uncertainty: " << lsResult.position.uncertaintyY << " m" << std::endl;
    std::cout << "  GDOP: " << lsResult.gdop.gdop << std::endl;
    printEllipse(lsResult.confidence);
    std::cout << std::endl;
    
    std::cout << "Taylor Series Solution:" << std::endl;
    config.method = tdoa::multilateration::SolverMethod::TaylorSeries;
    solver.setConfig(config);
    
    tdoa::multilateration::MultilaterationResult tsResult = 
        solver.calculatePosition(tdoa, simulator.getReceivers());
    
    std::cout << "  Estimated Position: (" 
              << tsResult.position.x << ", " << tsResult.position.y << ")" << std::endl;
    std::cout << "  Error: " 
              << std::sqrt(std::pow(tsResult.position.x - trueX, 2) + 
                         std::pow(tsResult.position.y - trueY, 2)) << " m" << std::endl;
    std::cout << "  Confidence: " << tsResult.position.confidence * 100.0 << "%" << std::endl;
    std::cout << "  X Uncertainty: " << tsResult.position.uncertaintyX << " m" << std::endl;
    std::cout << "  Y Uncertainty: " << tsResult.position.uncertaintyY << " m" << std::endl;
    std::cout << "  GDOP: " << tsResult.gdop.gdop << std::endl;
    printEllipse(tsResult.confidence);
    std::cout << std::endl;
    
    // Try with a poor geometry
    std::cout << "Testing with Poor Geometry:" << std::endl;
    TDOASimulator poorSimulator;
    poorSimulator.addReceiver("P1", -1000.0, 0.0);
    poorSimulator.addReceiver("P2", -800.0, 0.0);
    poorSimulator.addReceiver("P3", -600.0, 0.0);
    
    tdoa::time_difference::TimeDifferenceSet poorTdoa = 
        poorSimulator.simulateMeasurement(trueX, trueY);
    
    tdoa::multilateration::MultilaterationResult poorResult = 
        solver.calculatePosition(poorTdoa, poorSimulator.getReceivers());
    
    std::cout << "  Estimated Position: (" 
              << poorResult.position.x << ", " << poorResult.position.y << ")" << std::endl;
    std::cout << "  Error: " 
              << std::sqrt(std::pow(poorResult.position.x - trueX, 2) + 
                         std::pow(poorResult.position.y - trueY, 2)) << " m" << std::endl;
    std::cout << "  Confidence: " << poorResult.position.confidence * 100.0 << "%" << std::endl;
    std::cout << "  X Uncertainty: " << poorResult.position.uncertaintyX << " m" << std::endl;
    std::cout << "  Y Uncertainty: " << poorResult.position.uncertaintyY << " m" << std::endl;
    std::cout << "  GDOP: " << poorResult.gdop.gdop << std::endl;
    printEllipse(poorResult.confidence);
    
    return 0;
} 