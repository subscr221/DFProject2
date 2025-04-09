#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <optional>
#include <chrono>
#include <filesystem>

namespace df {
namespace ui {

/**
 * @struct TileStats
 * @brief Statistics for tile server monitoring
 */
struct TileStats {
    size_t totalTiles = 0;
    size_t cachedTiles = 0;
    size_t totalSizeBytes = 0;
    size_t requestsServed = 0;
    double averageLatencyMs = 0.0;
    size_t cacheHits = 0;
    size_t cacheMisses = 0;
};

/**
 * @struct TileRequest
 * @brief Represents a request for a map tile
 */
struct TileRequest {
    int z;                  ///< Zoom level
    int x;                  ///< X coordinate
    int y;                  ///< Y coordinate
    bool priority = false;  ///< Whether this is a priority request
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @struct TileResponse
 * @brief Response containing tile data
 */
struct TileResponse {
    std::vector<uint8_t> data;
    std::string contentType;
    std::string etag;
    bool fromCache = false;
};

/**
 * @class TileServer
 * @brief Local tile server for efficient map tile delivery
 */
class TileServer {
public:
    /**
     * @brief Constructor
     * @param cachePath Path to tile cache directory
     * @param maxCacheSize Maximum cache size in bytes
     * @param maxConcurrentDownloads Maximum concurrent downloads
     */
    TileServer(const std::string& cachePath,
              size_t maxCacheSize = 1024 * 1024 * 1024,  // 1GB default
              size_t maxConcurrentDownloads = 4);
    
    /**
     * @brief Destructor
     */
    ~TileServer();
    
    /**
     * @brief Start the tile server
     * @param port Port to listen on
     * @return bool True if server started successfully
     */
    bool start(uint16_t port = 8080);
    
    /**
     * @brief Stop the tile server
     */
    void stop();
    
    /**
     * @brief Get tile server statistics
     * @return TileStats Current statistics
     */
    TileStats getStats() const;
    
    /**
     * @brief Download tiles for an area
     * @param minLat Minimum latitude
     * @param minLon Minimum longitude
     * @param maxLat Maximum latitude
     * @param maxLon Maximum longitude
     * @param minZoom Minimum zoom level
     * @param maxZoom Maximum zoom level
     * @param progressCallback Callback for download progress (0.0-1.0)
     * @return bool True if download completed successfully
     */
    bool downloadArea(double minLat, double minLon,
                     double maxLat, double maxLon,
                     int minZoom, int maxZoom,
                     std::function<void(double)> progressCallback = nullptr);
    
    /**
     * @brief Clear tile cache
     * @param olderThan Clear tiles older than this (optional)
     * @return size_t Number of tiles cleared
     */
    size_t clearCache(std::optional<std::chrono::system_clock::time_point> olderThan = std::nullopt);
    
    /**
     * @brief Get tile coverage information
     * @return Vector of {zoom, count} pairs
     */
    std::vector<std::pair<int, size_t>> getCoverage() const;
    
    /**
     * @brief Set rate limit
     * @param requestsPerMinute Maximum requests per minute
     */
    void setRateLimit(size_t requestsPerMinute);
    
    /**
     * @brief Enable/disable tile compression
     * @param enable Whether to enable compression
     * @param level Compression level (1-9, default 6)
     */
    void setCompression(bool enable, int level = 6);
    
    /**
     * @brief Set tile update policy
     * @param checkIntervalHours Hours between update checks
     * @param maxAgeHours Maximum age of tiles before update
     */
    void setUpdatePolicy(int checkIntervalHours, int maxAgeHours);
    
    /**
     * @brief Synchronize with another tile server
     * @param serverUrl URL of server to sync with
     * @param progressCallback Callback for sync progress (0.0-1.0)
     * @return bool True if sync completed successfully
     */
    bool synchronize(const std::string& serverUrl,
                    std::function<void(double)> progressCallback = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
}; 