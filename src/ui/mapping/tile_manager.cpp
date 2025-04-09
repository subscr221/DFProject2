#include "tile_manager.h"
#include <sstream>
#include <cmath>
#include <filesystem>
#include <chrono>
#include <thread>
#include <iostream>

namespace df {
namespace ui {

struct TileManager::Impl {
    TileManagerConfig config;
    std::unique_ptr<TileServer> server;
    std::unique_ptr<mapping::TileCoverageVisualizer> coverageVisualizer;
    bool isRunning;

    explicit Impl(const TileManagerConfig& cfg)
        : config(cfg)
        , isRunning(false) {
        coverageVisualizer = std::make_unique<mapping::TileCoverageVisualizer>();
    }
};

TileManager::TileManager(const TileManagerConfig& config)
    : pImpl(std::make_unique<Impl>(config)) {
}

TileManager::~TileManager() {
    stop();
}

bool TileManager::start() {
    if (pImpl->isRunning) {
        return true;
    }

    try {
        // Create cache directory if it doesn't exist
        std::filesystem::create_directories(pImpl->config.cachePath);

        // Initialize tile server
        pImpl->server = std::make_unique<TileServer>(
            pImpl->config.cachePath,
            pImpl->config.maxCacheSize,
            pImpl->config.maxConcurrentDownloads
        );

        // Configure server
        pImpl->server->setCompression(
            pImpl->config.enableCompression,
            pImpl->config.compressionLevel
        );
        pImpl->server->setRateLimit(pImpl->config.rateLimit);
        pImpl->server->setUpdatePolicy(
            pImpl->config.updateCheckIntervalHours,
            pImpl->config.maxTileAgeHours
        );

        // Start server
        if (!pImpl->server->start(pImpl->config.serverPort)) {
            std::cerr << "Failed to start tile server" << std::endl;
            return false;
        }

        pImpl->isRunning = true;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error starting tile server: " << e.what() << std::endl;
        return false;
    }
}

void TileManager::stop() {
    if (!pImpl->isRunning) {
        return;
    }

    if (pImpl->server) {
        pImpl->server->stop();
    }
    pImpl->isRunning = false;
}

bool TileManager::downloadArea(const TileBounds& bounds,
                             int minZoom,
                             int maxZoom,
                             std::function<void(double)> progressCallback) {
    if (!pImpl->isRunning || !pImpl->server) {
        return false;
    }

    // Convert tile bounds to geographic coordinates
    double minLat, minLon, maxLat, maxLon;
    tileToLatLon(bounds.min_x, bounds.min_y, bounds.zoom, minLat, minLon);
    tileToLatLon(bounds.max_x, bounds.max_y, bounds.zoom, maxLat, maxLon);

    return pImpl->server->downloadArea(
        minLat, minLon,
        maxLat, maxLon,
        minZoom, maxZoom,
        progressCallback
    );
}

TileCoverageStats TileManager::getCoverageStats() const {
    if (!pImpl->isRunning || !pImpl->server) {
        return TileCoverageStats{};
    }

    return pImpl->coverageVisualizer->analyzeCoverage(pImpl->config.cachePath);
}

bool TileManager::generateCoverageVisualization(const std::string& outputPath,
                                              int zoomLevel) const {
    if (!pImpl->isRunning || !pImpl->server) {
        return false;
    }

    try {
        auto stats = getCoverageStats();
        pImpl->coverageVisualizer->generateHeatmap(outputPath, stats, zoomLevel);
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error generating coverage visualization: " << e.what() << std::endl;
        return false;
    }
}

size_t TileManager::clearOldTiles(int maxAgeHours) {
    if (!pImpl->isRunning || !pImpl->server) {
        return 0;
    }

    auto cutoff = std::chrono::system_clock::now() - 
                 std::chrono::hours(maxAgeHours);
    return pImpl->server->clearCache(cutoff);
}

TileStats TileManager::getServerStats() const {
    if (!pImpl->isRunning || !pImpl->server) {
        return TileStats{};
    }

    return pImpl->server->getStats();
}

bool TileManager::updateConfig(const TileManagerConfig& config) {
    if (pImpl->isRunning) {
        // Update running server configuration
        if (pImpl->server) {
            pImpl->server->setCompression(
                config.enableCompression,
                config.compressionLevel
            );
            pImpl->server->setRateLimit(config.rateLimit);
            pImpl->server->setUpdatePolicy(
                config.updateCheckIntervalHours,
                config.maxTileAgeHours
            );
        }
    }

    pImpl->config = config;
    return true;
}

namespace {
void tileToLatLon(int x, int y, int z, double& lat, double& lon) {
    int n = 1 << z;
    lon = x / static_cast<double>(n) * 360.0 - 180.0;
    double lat_rad = std::atan(std::sinh(M_PI * (1 - 2 * y / static_cast<double>(n))));
    lat = lat_rad * 180.0 / M_PI;
}
} // namespace

} // namespace ui
} // namespace df 