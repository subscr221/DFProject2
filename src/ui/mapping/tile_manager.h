#pragma once

#include "tile_server.h"
#include "tile_coverage.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace df {
namespace ui {

/**
 * @struct TileManagerConfig
 * @brief Configuration for tile manager
 */
struct TileManagerConfig {
    std::string cachePath;                  ///< Path to tile cache directory
    size_t maxCacheSize = 1024 * 1024 * 1024;  ///< Maximum cache size (1GB default)
    size_t maxConcurrentDownloads = 4;      ///< Maximum concurrent downloads
    uint16_t serverPort = 8080;             ///< Local tile server port
    bool enableCompression = true;          ///< Whether to enable tile compression
    int compressionLevel = 6;               ///< Compression level (1-9)
    int updateCheckIntervalHours = 24;      ///< Hours between update checks
    int maxTileAgeHours = 168;             ///< Maximum tile age (1 week)
    size_t rateLimit = 250;                ///< Maximum requests per minute
};

/**
 * @class TileManager
 * @brief Manages tile server and provides high-level tile management functions
 */
class TileManager {
public:
    /**
     * @brief Constructor
     * @param config Tile manager configuration
     */
    explicit TileManager(const TileManagerConfig& config);

    /**
     * @brief Destructor
     */
    ~TileManager();

    /**
     * @brief Start the tile server
     * @return bool True if server started successfully
     */
    bool start();

    /**
     * @brief Stop the tile server
     */
    void stop();

    /**
     * @brief Download tiles for an area
     * @param bounds Geographic bounds to download
     * @param minZoom Minimum zoom level
     * @param maxZoom Maximum zoom level
     * @param progressCallback Callback for download progress
     * @return bool True if download completed successfully
     */
    bool downloadArea(const TileBounds& bounds,
                     int minZoom,
                     int maxZoom,
                     std::function<void(double)> progressCallback = nullptr);

    /**
     * @brief Get tile coverage statistics
     * @return TileCoverageStats Coverage statistics
     */
    TileCoverageStats getCoverageStats() const;

    /**
     * @brief Generate coverage visualization
     * @param outputPath Path to save visualization
     * @param zoomLevel Zoom level to visualize
     * @return bool True if visualization was generated
     */
    bool generateCoverageVisualization(const std::string& outputPath,
                                     int zoomLevel) const;

    /**
     * @brief Clear old tiles from cache
     * @param maxAgeHours Maximum tile age in hours
     * @return size_t Number of tiles cleared
     */
    size_t clearOldTiles(int maxAgeHours);

    /**
     * @brief Get server status
     * @return TileStats Current server statistics
     */
    TileStats getServerStats() const;

    /**
     * @brief Update tile server configuration
     * @param config New configuration
     * @return bool True if configuration was updated
     */
    bool updateConfig(const TileManagerConfig& config);

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace ui
} // namespace df 