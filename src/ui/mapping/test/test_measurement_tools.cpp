#include "map_display.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <cmath>

using namespace df::ui;

void onDistanceMeasured(double distance) {
    std::cout << "Measured distance: " << distance << " meters" << std::endl;
}

void onAreaMeasured(double area) {
    std::cout << "Measured area: " << area << " square meters" << std::endl;
}

void onBearingMeasured(double bearing) {
    std::cout << "Measured bearing: " << bearing << " degrees" << std::endl;
}

int main() {
    // Create map configuration
    MapConfig config;
    config.initialLat = 37.7749;  // San Francisco
    config.initialLon = -122.4194;
    config.initialZoom = 12;
    config.width = 800;
    config.height = 600;
    
    // Configure local tile source
    config.tileConfig.source = TileSource::OSM_LOCAL;
    config.tileConfig.localTilePath = "tiles";
    
    // Create map display
    MapDisplay map;
    if (!map.initialize(config)) {
        std::cerr << "Failed to initialize map display" << std::endl;
        return 1;
    }
    
    // Test measurement tools
    std::cout << "Testing distance measurement..." << std::endl;
    map.startDistanceMeasurement(onDistanceMeasured);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    std::cout << "Testing area measurement..." << std::endl;
    map.startAreaMeasurement(onAreaMeasured);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    std::cout << "Testing bearing measurement..." << std::endl;
    map.startBearingMeasurement(onBearingMeasured);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // Test track visualization
    std::vector<std::tuple<double, double, int64_t>> trackPoints;
    
    // Create a circular track
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    for (int i = 0; i < 360; i += 10) {
        double angle = i * M_PI / 180.0;
        double radius = 0.01;  // About 1km at this latitude
        double lat = config.initialLat + radius * std::cos(angle);
        double lon = config.initialLon + radius * std::sin(angle);
        trackPoints.emplace_back(lat, lon, timestamp + i);
    }
    
    int trackId = map.addTrack(trackPoints, "#FF0000", 3.0);
    if (trackId < 0) {
        std::cerr << "Failed to add track" << std::endl;
        return 1;
    }
    
    // Test track animation
    map.setTrackAnimation(true, 10.0);  // 10x speed
    std::this_thread::sleep_for(std::chrono::seconds(10));
    map.setTrackAnimation(false);
    
    // Test track time range
    auto startTime = timestamp;
    auto endTime = timestamp + 180;  // Show first half of track
    map.setTrackTimeRange(startTime, endTime);
    
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();
    
    return 0;
} 