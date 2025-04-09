#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <map>
#include <optional>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include "signal_marker.h"

namespace df {
namespace ui {

/**
 * @enum TileSource
 * @brief Defines the source of map tiles
 */
enum class TileSource {
    OSM_ONLINE, ///< Online OpenStreetMap tiles
    OSM_LOCAL   ///< Local OpenStreetMap tiles
};

/**
 * @struct TileConfig
 * @brief Configuration for tile source
 */
struct TileConfig {
    TileSource source = TileSource::OSM_ONLINE;
    std::string localTilePath;                  ///< Path to local tile directory
    bool useCache = true;                      ///< Whether to cache tiles locally
};

/**
 * @struct MapConfig
 * @brief Configuration for the map display
 */
struct MapConfig {
    double initialLat = 0.0;           ///< Initial latitude
    double initialLon = 0.0;           ///< Initial longitude
    int initialZoom = 13;              ///< Initial zoom level
    int width = 800;                   ///< Display width in pixels
    int height = 600;                  ///< Display height in pixels
    bool enableClustering = true;      ///< Enable signal marker clustering
    int clusterRadius = 50;            ///< Radius in pixels for clustering markers
    TileConfig tileConfig;             ///< Tile source configuration
};

/**
 * @class MapDisplay
 * @brief WebGL-based map display using Leaflet.js/Mapbox GL JS
 */
class MapDisplay {
public:
    /**
     * @brief Constructor
     */
    MapDisplay();

    /**
     * @brief Destructor
     */
    ~MapDisplay();

    /**
     * @brief Initialize the map display
     * 
     * @param config Map configuration
     * @return bool True if initialization was successful
     */
    bool initialize(const MapConfig& config);

    /**
     * @brief Set the map center and zoom level
     * 
     * @param lat Latitude
     * @param lon Longitude
     * @param zoom Zoom level (optional)
     */
    void setView(double lat, double lon, int zoom = -1);

    /**
     * @brief Set the map style
     * 
     * @param style Map style name (streets, satellite, terrain)
     */
    void setStyle(const std::string& style);

    /**
     * @brief Handle window resize
     * 
     * @param width New width
     * @param height New height
     */
    void resize(int width, int height);

    /**
     * @brief Add a marker to the map
     * 
     * @param lat Latitude
     * @param lon Longitude
     * @param label Label text
     * @param color Marker color (hex format: #RRGGBB)
     * @return int Marker ID
     */
    int addMarker(double lat, double lon, const std::string& label = "", const std::string& color = "#FF0000");

    /**
     * @brief Remove a marker from the map
     * 
     * @param markerId ID of the marker to remove
     * @return bool True if marker was removed
     */
    bool removeMarker(int markerId);

    /**
     * @brief Add a confidence ellipse to the map
     * 
     * @param centerLat Center latitude
     * @param centerLon Center longitude
     * @param semiMajorAxis Semi-major axis in meters
     * @param semiMinorAxis Semi-minor axis in meters
     * @param rotationAngle Rotation angle in radians
     * @param color Ellipse color (hex format: #RRGGBB)
     * @param fillOpacity Fill opacity (0.0-1.0)
     * @return int Ellipse ID
     */
    int addConfidenceEllipse(double centerLat, double centerLon,
                            double semiMajorAxis, double semiMinorAxis,
                            double rotationAngle, const std::string& color = "#FF0000",
                            double fillOpacity = 0.2);

    /**
     * @brief Remove a confidence ellipse from the map
     * 
     * @param ellipseId ID of the ellipse to remove
     * @return bool True if ellipse was removed
     */
    bool removeConfidenceEllipse(int ellipseId);

    /**
     * @brief Set click callback function
     * 
     * @param callback Function to call when map is clicked
     */
    void setClickCallback(std::function<void(double lat, double lon)> callback);

    /**
     * @brief Get current map center
     * 
     * @return std::pair<double, double> Pair of latitude and longitude
     */
    std::pair<double, double> getCenter() const;

    /**
     * @brief Get current zoom level
     * 
     * @return int Zoom level
     */
    int getZoom() const;

    /**
     * @brief Add a signal marker to the map
     * 
     * @param signal Signal information
     * @return int Signal marker ID
     */
    int addSignal(const SignalInfo& signal);

    /**
     * @brief Update an existing signal marker
     * 
     * @param signalId Signal marker ID
     * @param signal Updated signal information
     * @return bool True if signal was updated
     */
    bool updateSignal(int signalId, const SignalInfo& signal);

    /**
     * @brief Remove a signal marker from the map
     * 
     * @param signalId Signal marker ID
     * @return bool True if signal was removed
     */
    bool removeSignal(int signalId);

    /**
     * @brief Set signal filter criteria
     * 
     * @param minFreq Minimum frequency (Hz)
     * @param maxFreq Maximum frequency (Hz)
     * @param minPower Minimum power (dBm)
     * @param minConfidence Minimum confidence level (0.0-1.0)
     */
    void setSignalFilter(double minFreq = 0.0, double maxFreq = 1e12,
                        double minPower = -200.0, double minConfidence = 0.0);

    /**
     * @brief Enable or disable marker clustering
     * 
     * @param enable True to enable clustering
     * @param radius Cluster radius in pixels
     */
    void setClusteringEnabled(bool enable, int radius = 50);

    /**
     * @brief Start distance measurement mode
     * @param callback Function to receive distance in meters
     */
    void startDistanceMeasurement(std::function<void(double)> callback);

    /**
     * @brief Start area measurement mode
     * @param callback Function to receive area in square meters
     */
    void startAreaMeasurement(std::function<void(double)> callback);

    /**
     * @brief Start bearing measurement mode
     * @param callback Function to receive bearing in degrees
     */
    void startBearingMeasurement(std::function<void(double)> callback);

    /**
     * @brief Cancel any active measurement
     */
    void cancelMeasurement();

    /**
     * @brief Add a historical track to the map
     * @param points Vector of {lat, lon, timestamp} points
     * @param color Track color in hex format (#RRGGBB)
     * @param width Track line width in pixels
     * @return int Track ID
     */
    int addTrack(const std::vector<std::tuple<double, double, int64_t>>& points,
                 const std::string& color = "#0000FF",
                 double width = 2.0);

    /**
     * @brief Remove a track from the map
     * @param trackId ID of track to remove
     * @return bool True if track was removed
     */
    bool removeTrack(int trackId);

    /**
     * @brief Set the visible time range for tracks
     * @param startTime Start timestamp (Unix epoch)
     * @param endTime End timestamp (Unix epoch)
     */
    void setTrackTimeRange(int64_t startTime, int64_t endTime);

    /**
     * @brief Enable/disable track animation
     * @param enabled Whether animation is enabled
     * @param speed Animation speed multiplier (1.0 = realtime)
     */
    void setTrackAnimation(bool enabled, double speed = 1.0);

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;

    static void handleDistanceMeasurement(double distance);
    static void handleAreaMeasurement(double area);
    static void handleBearingMeasurement(double bearing);
};

} // namespace ui
} // namespace df 