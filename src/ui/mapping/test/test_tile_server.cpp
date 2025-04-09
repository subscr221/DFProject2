#include "tile_server.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>

using namespace df::ui;

void printStats(const TileStats& stats) {
    std::cout << "Tile Server Statistics:" << std::endl;
    std::cout << "  Total Tiles: " << stats.totalTiles << std::endl;
    std::cout << "  Cached Tiles: " << stats.cachedTiles << std::endl;
    std::cout << "  Total Size: " << (stats.totalSizeBytes / 1024.0 / 1024.0) << " MB" << std::endl;
    std::cout << "  Requests Served: " << stats.requestsServed << std::endl;
    std::cout << "  Average Latency: " << stats.averageLatencyMs << " ms" << std::endl;
    std::cout << "  Cache Hits: " << stats.cacheHits << std::endl;
    std::cout << "  Cache Misses: " << stats.cacheMisses << std::endl;
}

void printCoverage(const std::vector<std::pair<int, size_t>>& coverage) {
    std::cout << "Tile Coverage:" << std::endl;
    for (const auto& [zoom, count] : coverage) {
        std::cout << "  Zoom " << zoom << ": " << count << " tiles" << std::endl;
    }
}

int main() {
    // Create cache directory
    std::string cachePath = "tile_cache";
    std::filesystem::create_directories(cachePath);
    
    // Create tile server
    TileServer server(cachePath);
    
    // Start server
    if (!server.start(8080)) {
        std::cerr << "Failed to start tile server" << std::endl;
        return 1;
    }
    
    std::cout << "Tile server started on port 8080" << std::endl;
    
    // Configure server
    server.setRateLimit(15000);  // 15k requests per minute
    server.setCompression(true, 6);  // Enable compression with level 6
    server.setUpdatePolicy(24, 168);  // Check daily, max age 1 week
    
    // Download San Francisco area
    std::cout << "\nDownloading San Francisco area tiles..." << std::endl;
    server.downloadArea(
        37.7549, -122.4494,  // Southwest corner
        37.7949, -122.3894,  // Northeast corner
        12, 16,  // Zoom levels
        [](double progress) {
            std::cout << "\rProgress: " << std::fixed << std::setprecision(1)
                     << (progress * 100.0) << "%" << std::flush;
        }
    );
    std::cout << std::endl;
    
    // Print initial stats
    std::cout << "\nInitial statistics:" << std::endl;
    printStats(server.getStats());
    
    // Print coverage
    std::cout << "\nInitial coverage:" << std::endl;
    printCoverage(server.getCoverage());
    
    // Test tile updates
    std::cout << "\nWaiting for tile updates (10 seconds)..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    // Print updated stats
    std::cout << "\nUpdated statistics:" << std::endl;
    printStats(server.getStats());
    
    // Test cache clearing
    auto now = std::chrono::system_clock::now();
    auto clearedTiles = server.clearCache(now - std::chrono::hours(24));
    std::cout << "\nCleared " << clearedTiles << " tiles older than 24 hours" << std::endl;
    
    // Print final coverage
    std::cout << "\nFinal coverage:" << std::endl;
    printCoverage(server.getCoverage());
    
    // Stop server
    server.stop();
    std::cout << "\nTile server stopped" << std::endl;
    
    return 0;
} 