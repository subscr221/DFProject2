#include "map_display.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace df::ui;

int main() {
    // Create map configuration
    MapConfig config;
    config.initialLat = 37.7749;  // San Francisco
    config.initialLon = -122.4194;
    config.initialZoom = 12;
    config.mapboxToken = "YOUR_MAPBOX_TOKEN";  // Replace with actual token
    config.mapStyle = "streets";
    config.width = 800;
    config.height = 600;
    
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
    
    // Add some test markers
    int marker1 = map.addMarker(37.7749, -122.4194, "San Francisco", "#FF0000");
    int marker2 = map.addMarker(37.7858, -122.4008, "Financial District", "#00FF00");
    
    // Add a test confidence ellipse
    int ellipse = map.addConfidenceEllipse(37.7749, -122.4194,
                                         500.0,  // 500m semi-major axis
                                         300.0,  // 300m semi-minor axis
                                         45.0 * M_PI / 180.0,  // 45 degrees rotation
                                         "#0000FF",
                                         0.3);
    
    // Wait for user input
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();
    
    // Clean up
    map.removeMarker(marker1);
    map.removeMarker(marker2);
    map.removeConfidenceEllipse(ellipse);
    
    return 0;
} 