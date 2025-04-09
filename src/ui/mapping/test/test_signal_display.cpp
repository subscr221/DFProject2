#include "map_display.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <cmath>

using namespace df::ui;

// Helper function to generate random signals
SignalInfo generateRandomSignal(double centerLat, double centerLon,
                              double radius, std::mt19937& gen) {
    std::uniform_real_distribution<> angleDist(0, 2 * M_PI);
    std::uniform_real_distribution<> radiusDist(0, radius);
    std::uniform_real_distribution<> freqDist(100e6, 6e9);  // 100 MHz to 6 GHz
    std::uniform_real_distribution<> powerDist(-120, -20);  // -120 to -20 dBm
    std::uniform_real_distribution<> confidenceDist(0.1, 1.0);
    
    double angle = angleDist(gen);
    double r = radiusDist(gen);
    
    // Convert radius from meters to degrees (approximate)
    double rDeg = r / 111319.9;
    
    SignalInfo signal;
    signal.latitude = centerLat + rDeg * std::cos(angle);
    signal.longitude = centerLon + rDeg * std::sin(angle);
    signal.frequency = freqDist(gen);
    signal.power = powerDist(gen);
    signal.confidenceLevel = confidenceDist(gen);
    
    // Add confidence ellipse for some signals
    if (signal.confidenceLevel > 0.5) {
        signal.semiMajorAxis = r * 0.5;
        signal.semiMinorAxis = r * 0.3;
        signal.orientation = angleDist(gen);
    }
    
    // Format label
    std::stringstream ss;
    ss << "Signal " << std::fixed << std::setprecision(1)
       << (signal.frequency / 1e6) << " MHz";
    signal.label = ss.str();
    
    return signal;
}

int main() {
    // Create map configuration
    MapConfig config;
    config.initialLat = 37.7749;  // San Francisco
    config.initialLon = -122.4194;
    config.initialZoom = 12;
    config.mapboxToken = "YOUR_MAPBOX_TOKEN";  // Replace with actual token
    config.mapStyle = "streets";
    config.width = 1024;
    config.height = 768;
    config.enableClustering = true;
    config.clusterRadius = 50;
    
    // Create map display
    MapDisplay map;
    if (!map.initialize(config)) {
        std::cerr << "Failed to initialize map display" << std::endl;
        return 1;
    }
    
    // Set click callback
    map.setClickCallback([](double lat, double lon) {
        std::cout << "Map clicked at: " << lat << ", " << lon << std::endl;
    });
    
    // Generate random signals
    std::random_device rd;
    std::mt19937 gen(rd());
    std::vector<int> signalIds;
    
    // Generate 100 random signals within 5km of San Francisco
    for (int i = 0; i < 100; ++i) {
        auto signal = generateRandomSignal(37.7749, -122.4194, 5000.0, gen);
        int id = map.addSignal(signal);
        if (id >= 0) {
            signalIds.push_back(id);
        }
    }
    
    // Demonstrate filtering
    std::cout << "\nPress Enter to apply frequency filter (>1 GHz)..." << std::endl;
    std::cin.get();
    map.setSignalFilter(1e9, 1e12, -200.0, 0.0);
    
    std::cout << "\nPress Enter to apply power filter (>-60 dBm)..." << std::endl;
    std::cin.get();
    map.setSignalFilter(0.0, 1e12, -60.0, 0.0);
    
    std::cout << "\nPress Enter to apply confidence filter (>0.8)..." << std::endl;
    std::cin.get();
    map.setSignalFilter(0.0, 1e12, -200.0, 0.8);
    
    std::cout << "\nPress Enter to disable clustering..." << std::endl;
    std::cin.get();
    map.setClusteringEnabled(false);
    
    std::cout << "\nPress Enter to exit..." << std::endl;
    std::cin.get();
    
    // Clean up
    for (int id : signalIds) {
        map.removeSignal(id);
    }
    
    return 0;
} 