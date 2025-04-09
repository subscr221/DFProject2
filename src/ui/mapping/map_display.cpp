#include "map_display.h"
#include "tile_manager.h"
#include "tile_config.h"
#include <iostream>
#include <cmath>
#include <emscripten.h>
#include <nlohmann/json.hpp>

namespace df {
namespace ui {

// JavaScript functions to be called from C++
EM_JS(void, js_initializeMap, (const char* containerId, double lat, double lon, int zoom, const char* token, const char* style), {
    // Initialize Leaflet map
    window.dfMap = L.map(UTF8ToString(containerId)).setView([lat, lon], zoom);
    
    // Add Mapbox tile layer
    L.tileLayer('https://api.mapbox.com/styles/v1/mapbox/' + UTF8ToString(style) + '-v11/tiles/{z}/{x}/{y}?access_token=' + UTF8ToString(token), {
        attribution: '© <a href="https://www.mapbox.com/about/maps/">Mapbox</a>',
        maxZoom: 18,
        tileSize: 512,
        zoomOffset: -1
    }).addTo(window.dfMap);
    
    // Initialize marker and ellipse layers
    window.dfMarkers = {};
    window.dfEllipses = {};
    window.nextMarkerId = 1;
    window.nextEllipseId = 1;
});

EM_JS(void, js_setMapView, (double lat, double lon, int zoom), {
    if (!window.dfMap) return;
    if (zoom >= 0) {
        window.dfMap.setView([lat, lon], zoom);
    } else {
        window.dfMap.panTo([lat, lon]);
    }
});

EM_JS(void, js_setMapStyle, (const char* token, const char* style), {
    if (!window.dfMap) return;
    
    // Remove existing tile layer
    window.dfMap.eachLayer((layer) => {
        if (layer instanceof L.TileLayer) {
            window.dfMap.removeLayer(layer);
        }
    });
    
    // Add new tile layer
    L.tileLayer('https://api.mapbox.com/styles/v1/mapbox/' + UTF8ToString(style) + '-v11/tiles/{z}/{x}/{y}?access_token=' + UTF8ToString(token), {
        attribution: '© <a href="https://www.mapbox.com/about/maps/">Mapbox</a>',
        maxZoom: 18,
        tileSize: 512,
        zoomOffset: -1
    }).addTo(window.dfMap);
});

EM_JS(int, js_addMarker, (double lat, double lon, const char* label, const char* color), {
    if (!window.dfMap) return -1;
    
    const markerId = window.nextMarkerId++;
    const marker = L.marker([lat, lon], {
        icon: L.divIcon({
            className: 'custom-marker',
            html: `<div style="background-color: ${UTF8ToString(color)}; width: 12px; height: 12px; border-radius: 50%; border: 2px solid white;"></div>`
        })
    }).addTo(window.dfMap);
    
    if (UTF8ToString(label)) {
        marker.bindTooltip(UTF8ToString(label));
    }
    
    window.dfMarkers[markerId] = marker;
    return markerId;
});

EM_JS(bool, js_removeMarker, (int markerId), {
    if (!window.dfMap || !window.dfMarkers[markerId]) return false;
    
    window.dfMap.removeLayer(window.dfMarkers[markerId]);
    delete window.dfMarkers[markerId];
    return true;
});

EM_JS(int, js_addConfidenceEllipse, (double centerLat, double centerLon, double semiMajorAxis,
                                    double semiMinorAxis, double rotationAngle, const char* color,
                                    double fillOpacity), {
    if (!window.dfMap) return -1;
    
    // Convert semi-axes from meters to degrees (approximate)
    const metersPerDegree = 111319.9;
    const semiMajorDeg = semiMajorAxis / metersPerDegree;
    const semiMinorDeg = semiMinorAxis / metersPerDegree;
    
    // Generate ellipse points
    const points = [];
    const steps = 64;
    for (let i = 0; i <= steps; i++) {
        const angle = (i / steps) * 2 * Math.PI;
        const x = semiMajorDeg * Math.cos(angle);
        const y = semiMinorDeg * Math.sin(angle);
        
        // Apply rotation
        const rotatedX = x * Math.cos(rotationAngle) - y * Math.sin(rotationAngle);
        const rotatedY = x * Math.sin(rotationAngle) + y * Math.cos(rotationAngle);
        
        points.push([centerLat + rotatedY, centerLon + rotatedX]);
    }
    
    const ellipseId = window.nextEllipseId++;
    const ellipse = L.polygon(points, {
        color: UTF8ToString(color),
        fillColor: UTF8ToString(color),
        fillOpacity: fillOpacity,
        weight: 1
    }).addTo(window.dfMap);
    
    window.dfEllipses[ellipseId] = ellipse;
    return ellipseId;
});

EM_JS(bool, js_removeConfidenceEllipse, (int ellipseId), {
    if (!window.dfMap || !window.dfEllipses[ellipseId]) return false;
    
    window.dfMap.removeLayer(window.dfEllipses[ellipseId]);
    delete window.dfEllipses[ellipseId];
    return true;
});

EM_JS(void, js_setClickCallback, (void (*callback)(double lat, double lon)), {
    if (!window.dfMap) return;
    
    window.dfMap.on('click', (e) => {
        const lat = e.latlng.lat;
        const lng = e.latlng.lng;
        ccall('handleMapClick', 'void', ['number', 'number'], [lat, lng]);
    });
    
    window.mapClickCallback = callback;
});

// C++ callback function for map clicks
extern "C" {
    EMSCRIPTEN_KEEPALIVE void handleMapClick(double lat, double lon) {
        if (MapDisplay::Impl::clickCallback) {
            MapDisplay::Impl::clickCallback(lat, lon);
        }
    }
}

EM_JS(void, js_initializeMarkerClustering, (int radius), {
    if (!window.dfMap) return;
    
    // Initialize marker cluster group if not exists
    if (!window.dfClusterGroup) {
        window.dfClusterGroup = L.markerClusterGroup({
            maxClusterRadius: radius,
            iconCreateFunction: function(cluster) {
                var childCount = cluster.getChildCount();
                var confidenceSum = 0;
                cluster.getAllChildMarkers().forEach(function(marker) {
                    confidenceSum += marker.options.confidence || 0;
                });
                var avgConfidence = confidenceSum / childCount;
                
                // Color interpolation similar to SignalMarker::interpolateColor
                var r = avgConfidence < 0.5 ? 255 : Math.round(510 * (1 - avgConfidence));
                var g = avgConfidence < 0.5 ? Math.round(510 * avgConfidence) : 255;
                
                return L.divIcon({
                    html: '<div style="background-color: rgb(' + r + ',' + g + ',0); width: 30px; height: 30px; border-radius: 15px; display: flex; align-items: center; justify-content: center; color: white; font-weight: bold;">' + childCount + '</div>',
                    className: 'marker-cluster',
                    iconSize: L.point(30, 30)
                });
            }
        });
        window.dfMap.addLayer(window.dfClusterGroup);
    }
});

EM_JS(int, js_addSignalMarker, (double lat, double lon, const char* color, double opacity,
                               const char* tooltip, double confidence), {
    if (!window.dfMap) return -1;
    
    const markerId = window.nextMarkerId++;
    const marker = L.marker([lat, lon], {
        icon: L.divIcon({
            className: 'signal-marker',
            html: `<div style="background-color: ${UTF8ToString(color)}; opacity: ${opacity}; width: 12px; height: 12px; border-radius: 50%; border: 2px solid white;"></div>`
        }),
        confidence: confidence  // Store confidence for cluster coloring
    });
    
    marker.bindTooltip(UTF8ToString(tooltip), {
        permanent: false,
        direction: 'top'
    });
    
    if (window.dfClusterGroup) {
        window.dfClusterGroup.addLayer(marker);
    } else {
        marker.addTo(window.dfMap);
    }
    
    window.dfMarkers[markerId] = marker;
    return markerId;
});

EM_JS(void, js_initializeMapWithLocalTiles, (const char* containerId, double lat, double lon, int zoom,
                                           const char* tilePath, int minZoom, int maxZoom,
                                           const char* attribution), {
    if (window.dfMap) return;
    
    // Initialize Leaflet map
    window.dfMap = L.map(UTF8ToString(containerId)).setView([lat, lon], zoom);
    
    // Add local tile layer
    L.tileLayer(UTF8ToString(tilePath) + '/{z}/{x}/{y}.png', {
        attribution: UTF8ToString(attribution),
        minZoom: minZoom,
        maxZoom: maxZoom,
        tileSize: 256,
        detectRetina: true
    }).addTo(window.dfMap);
    
    // Initialize marker storage
    window.dfMarkers = {};
    window.nextMarkerId = 0;
});

EM_JS(void, js_initializeMapWithOSM, (const char* containerId, double lat, double lon, int zoom), {
    if (window.dfMap) return;
    
    // Initialize Leaflet map
    window.dfMap = L.map(UTF8ToString(containerId)).setView([lat, lon], zoom);
    
    // Add OpenStreetMap tile layer
    L.tileLayer(UTF8ToString(OSM_TILE_SERVER), {
        attribution: UTF8ToString(OSM_ATTRIBUTION),
        maxZoom: OSM_MAX_ZOOM,
        minZoom: OSM_MIN_ZOOM,
        tileSize: 256,
        detectRetina: true
    }).addTo(window.dfMap);
    
    // Initialize marker storage
    window.dfMarkers = {};
    window.nextMarkerId = 0;
});

// JavaScript functions for measurement tools
EM_JS(void, js_startDistanceMeasurement, (void (*callback)(double)), {
    if (!window.dfMap) return;
    
    // Initialize measurement control if not exists
    if (!window.dfMeasureControl) {
        window.dfMeasureControl = L.control.measure({
            primaryLengthUnit: 'meters',
            secondaryLengthUnit: 'kilometers',
            primaryAreaUnit: 'sqmeters',
            secondaryAreaUnit: 'hectares',
            activeColor: '#ABE67E',
            completedColor: '#C8F2BE'
        }).addTo(window.dfMap);
    }
    
    // Store callback
    window.distanceMeasurementCallback = callback;
    
    // Start measurement
    window.dfMeasureControl.startMeasure();
    
    // Listen for measurement complete
    window.dfMap.once('measurefinish', function(e) {
        if (window.distanceMeasurementCallback) {
            ccall('handleDistanceMeasurement', 'void', ['number'], [e.distance]);
        }
    });
});

EM_JS(void, js_startAreaMeasurement, (void (*callback)(double)), {
    if (!window.dfMap) return;
    
    if (!window.dfMeasureControl) {
        window.dfMeasureControl = L.control.measure({
            primaryLengthUnit: 'meters',
            secondaryLengthUnit: 'kilometers',
            primaryAreaUnit: 'sqmeters',
            secondaryAreaUnit: 'hectares',
            activeColor: '#ABE67E',
            completedColor: '#C8F2BE'
        }).addTo(window.dfMap);
    }
    
    window.areaMeasurementCallback = callback;
    window.dfMeasureControl.startArea();
    
    window.dfMap.once('measurefinish', function(e) {
        if (window.areaMeasurementCallback) {
            ccall('handleAreaMeasurement', 'void', ['number'], [e.area]);
        }
    });
});

EM_JS(void, js_startBearingMeasurement, (void (*callback)(double)), {
    if (!window.dfMap) return;
    
    if (!window.dfBearingControl) {
        window.dfBearingControl = L.control.bearing({
            position: 'topleft',
            primaryLengthUnit: 'meters',
            secondaryLengthUnit: 'kilometers',
            bearingTextIn: 'In',
            bearingTextOut: 'Out',
            tooltipTextFinish: 'Click to finish bearing measurement',
            tooltipTextDelete: 'Press SHIFT-key and click to delete point',
            tooltipTextMove: 'Click and drag to move point',
            tooltipTextResume: 'Click to resume bearing measurement',
            tooltipTextAdd: 'Press CTRL-key and click to add point'
        }).addTo(window.dfMap);
    }
    
    window.bearingMeasurementCallback = callback;
    window.dfBearingControl.startBearing();
    
    window.dfMap.once('bearingfinish', function(e) {
        if (window.bearingMeasurementCallback) {
            ccall('handleBearingMeasurement', 'void', ['number'], [e.bearing]);
        }
    });
});

// JavaScript functions for track visualization
EM_JS(int, js_addTrack, (const char* pointsJson, const char* color, double width), {
    if (!window.dfMap) return -1;
    
    const points = JSON.parse(UTF8ToString(pointsJson));
    const trackId = window.nextTrackId++;
    
    // Create track polyline
    const trackLine = L.polyline(points.map(p => [p[0], p[1]]), {
        color: UTF8ToString(color),
        weight: width,
        opacity: 0.8
    }).addTo(window.dfMap);
    
    // Store track data
    window.dfTracks[trackId] = {
        line: trackLine,
        points: points,
        visible: true
    };
    
    return trackId;
});

EM_JS(bool, js_removeTrack, (int trackId), {
    if (!window.dfMap || !window.dfTracks[trackId]) return false;
    
    window.dfMap.removeLayer(window.dfTracks[trackId].line);
    delete window.dfTracks[trackId];
    return true;
});

EM_JS(void, js_setTrackTimeRange, (int64_t startTime, int64_t endTime), {
    if (!window.dfMap) return;
    
    // Update track visibility based on time range
    for (const [trackId, track] of Object.entries(window.dfTracks)) {
        const visiblePoints = track.points.filter(p => p[2] >= startTime && p[2] <= endTime);
        if (visiblePoints.length > 0) {
            track.line.setLatLngs(visiblePoints.map(p => [p[0], p[1]]));
            if (!track.visible) {
                window.dfMap.addLayer(track.line);
                track.visible = true;
            }
        } else if (track.visible) {
            window.dfMap.removeLayer(track.line);
            track.visible = false;
        }
    }
});

EM_JS(void, js_setTrackAnimation, (bool enabled, double speed), {
    if (!window.dfMap) return;
    
    if (enabled) {
        if (!window.trackAnimationInterval) {
            const animationStep = 1000 / speed; // milliseconds per step
            let currentTime = window.trackAnimationStartTime || Date.now();
            
            window.trackAnimationInterval = setInterval(() => {
                currentTime += animationStep;
                js_setTrackTimeRange(window.trackAnimationStartTime, currentTime);
            }, 16); // ~60 FPS
        }
    } else if (window.trackAnimationInterval) {
        clearInterval(window.trackAnimationInterval);
        window.trackAnimationInterval = null;
    }
});

// C++ callback functions
extern "C" {
    EMSCRIPTEN_KEEPALIVE void handleDistanceMeasurement(double distance) {
        if (MapDisplay::Impl::distanceMeasurementCallback) {
            MapDisplay::Impl::distanceMeasurementCallback(distance);
        }
    }
    
    EMSCRIPTEN_KEEPALIVE void handleAreaMeasurement(double area) {
        if (MapDisplay::Impl::areaMeasurementCallback) {
            MapDisplay::Impl::areaMeasurementCallback(area);
        }
    }
    
    EMSCRIPTEN_KEEPALIVE void handleBearingMeasurement(double bearing) {
        if (MapDisplay::Impl::bearingMeasurementCallback) {
            MapDisplay::Impl::bearingMeasurementCallback(bearing);
        }
    }
}

struct MapDisplay::Impl {
    MapConfig config;
    static std::function<void(double lat, double lon)> clickCallback;
    static std::function<void(double)> distanceMeasurementCallback;
    static std::function<void(double)> areaMeasurementCallback;
    static std::function<void(double)> bearingMeasurementCallback;
    std::string containerId;
    std::map<int, SignalMarker> signals;
    std::unique_ptr<TileManager> tileManager;
    double minFreq = 0.0;
    double maxFreq = 1e12;
    double minPower = -200.0;
    double minConfidence = 0.0;
    std::map<int, TrackInfo> tracks;
    
    Impl() : containerId("map-container") {}
};

std::function<void(double lat, double lon)> MapDisplay::Impl::clickCallback;
std::function<void(double)> MapDisplay::Impl::distanceMeasurementCallback;
std::function<void(double)> MapDisplay::Impl::areaMeasurementCallback;
std::function<void(double)> MapDisplay::Impl::bearingMeasurementCallback;

MapDisplay::MapDisplay() : pImpl(std::make_unique<Impl>()) {}

MapDisplay::~MapDisplay() = default;

bool MapDisplay::initialize(const MapConfig& config) {
    pImpl->config = config;
    
    // Initialize tile manager if using local tiles
    if (config.tileConfig.source == TileSource::OSM_LOCAL) {
        if (config.tileConfig.localTilePath.empty()) {
            std::cerr << "Local tile path is required for OSM_LOCAL tile source" << std::endl;
            return false;
        }

        try {
            // Create tile manager configuration
            TileManagerConfig tileConfig;
            tileConfig.cachePath = config.tileConfig.localTilePath;
            tileConfig.serverPort = 8080;  // Default port
            tileConfig.enableCompression = true;
            tileConfig.maxCacheSize = 1024 * 1024 * 1024;  // 1GB
            tileConfig.maxConcurrentDownloads = 4;
            
            // Create and start tile manager
            pImpl->tileManager = std::make_unique<TileManager>(tileConfig);
            if (!pImpl->tileManager->start()) {
                std::cerr << "Failed to start tile manager" << std::endl;
                return false;
            }
            
            // Initialize map with local tile server
            std::string tileUrl = "http://localhost:" + std::to_string(tileConfig.serverPort) + "/{z}/{x}/{y}.png";
            js_initializeMapWithLocalTiles(
                pImpl->containerId.c_str(),
                config.initialLat,
                config.initialLon,
                config.initialZoom,
                tileUrl.c_str(),
                OSM_MIN_ZOOM,
                OSM_MAX_ZOOM,
                OSM_ATTRIBUTION
            );
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize tile manager: " << e.what() << std::endl;
            return false;
        }
    } else {
        // Initialize with online OSM tiles
        js_initializeMapWithOSM(
            pImpl->containerId.c_str(),
            config.initialLat,
            config.initialLon,
            config.initialZoom
        );
    }
    
    if (config.enableClustering) {
        js_initializeMarkerClustering(config.clusterRadius);
    }
    
    return true;
}

void MapDisplay::setView(double lat, double lon, int zoom) {
    js_setMapView(lat, lon, zoom);
}

void MapDisplay::setStyle(const std::string& style) {
    js_setMapStyle(pImpl->config.mapboxToken.c_str(), style.c_str());
}

void MapDisplay::resize(int width, int height) {
    // Update container size through CSS
    EM_ASM({
        const container = document.getElementById(UTF8ToString($0));
        if (container) {
            container.style.width = $1 + 'px';
            container.style.height = $2 + 'px';
            window.dfMap.invalidateSize();
        }
    }, pImpl->containerId.c_str(), width, height);
    
    pImpl->config.width = width;
    pImpl->config.height = height;
}

int MapDisplay::addMarker(double lat, double lon, const std::string& label, const std::string& color) {
    return js_addMarker(lat, lon, label.c_str(), color.c_str());
}

bool MapDisplay::removeMarker(int markerId) {
    return js_removeMarker(markerId);
}

int MapDisplay::addConfidenceEllipse(double centerLat, double centerLon,
                                   double semiMajorAxis, double semiMinorAxis,
                                   double rotationAngle, const std::string& color,
                                   double fillOpacity) {
    return js_addConfidenceEllipse(centerLat, centerLon,
                                  semiMajorAxis, semiMinorAxis,
                                  rotationAngle, color.c_str(),
                                  fillOpacity);
}

bool MapDisplay::removeConfidenceEllipse(int ellipseId) {
    return js_removeConfidenceEllipse(ellipseId);
}

void MapDisplay::setClickCallback(std::function<void(double lat, double lon)> callback) {
    pImpl->clickCallback = callback;
    js_setClickCallback(reinterpret_cast<void (*)(double, double)>(handleMapClick));
}

std::pair<double, double> MapDisplay::getCenter() const {
    double lat = 0.0, lon = 0.0;
    
    EM_ASM({
        if (window.dfMap) {
            const center = window.dfMap.getCenter();
            setValue($0, center.lat, 'double');
            setValue($1, center.lng, 'double');
        }
    }, &lat, &lon);
    
    return {lat, lon};
}

int MapDisplay::getZoom() const {
    return EM_ASM_INT({
        return window.dfMap ? window.dfMap.getZoom() : 0;
    });
}

int MapDisplay::addSignal(const SignalInfo& signal) {
    // Check if signal passes current filter
    if (signal.frequency < pImpl->minFreq || signal.frequency > pImpl->maxFreq ||
        signal.power < pImpl->minPower || signal.confidenceLevel < pImpl->minConfidence) {
        return -1;
    }
    
    // Create signal marker
    SignalMarker marker(signal);
    
    // Add marker to map
    int markerId = js_addSignalMarker(signal.latitude, signal.longitude,
                                     marker.getColor().c_str(),
                                     marker.getOpacity(),
                                     marker.getTooltipContent().c_str(),
                                     signal.confidenceLevel);
    
    if (markerId >= 0) {
        pImpl->signals[markerId] = std::move(marker);
        
        // Add confidence ellipse if parameters are available
        if (signal.semiMajorAxis && signal.semiMinorAxis && signal.orientation) {
            int ellipseId = addConfidenceEllipse(signal.latitude, signal.longitude,
                                               *signal.semiMajorAxis,
                                               *signal.semiMinorAxis,
                                               *signal.orientation,
                                               marker.getColor(),
                                               0.2);
            if (ellipseId >= 0) {
                // Store ellipse ID with marker for later cleanup
                // This would require adding a field to SignalMarker to store the ellipse ID
            }
        }
    }
    
    return markerId;
}

bool MapDisplay::updateSignal(int signalId, const SignalInfo& signal) {
    auto it = pImpl->signals.find(signalId);
    if (it == pImpl->signals.end()) {
        return false;
    }
    
    // Remove existing marker
    removeSignal(signalId);
    
    // Add new marker with updated information
    int newId = addSignal(signal);
    return newId >= 0;
}

bool MapDisplay::removeSignal(int signalId) {
    auto it = pImpl->signals.find(signalId);
    if (it == pImpl->signals.end()) {
        return false;
    }
    
    // Remove marker from map
    bool success = js_removeMarker(signalId);
    if (success) {
        pImpl->signals.erase(it);
    }
    
    return success;
}

void MapDisplay::setSignalFilter(double minFreq, double maxFreq,
                               double minPower, double minConfidence) {
    pImpl->minFreq = minFreq;
    pImpl->maxFreq = maxFreq;
    pImpl->minPower = minPower;
    pImpl->minConfidence = minConfidence;
    
    // Re-filter existing signals
    std::vector<int> toRemove;
    for (const auto& pair : pImpl->signals) {
        const auto& signal = pair.second.getInfo();
        if (signal.frequency < minFreq || signal.frequency > maxFreq ||
            signal.power < minPower || signal.confidenceLevel < minConfidence) {
            toRemove.push_back(pair.first);
        }
    }
    
    for (int id : toRemove) {
        removeSignal(id);
    }
}

void MapDisplay::setClusteringEnabled(bool enable, int radius) {
    if (enable == pImpl->config.enableClustering) {
        return;
    }
    
    pImpl->config.enableClustering = enable;
    pImpl->config.clusterRadius = radius;
    
    if (enable) {
        js_initializeMarkerClustering(radius);
    } else {
        // TODO: Add JavaScript function to disable clustering
        // This would require moving all markers back to the main map layer
    }
}

void MapDisplay::startDistanceMeasurement(std::function<void(double)> callback) {
    pImpl->distanceMeasurementCallback = callback;
    js_startDistanceMeasurement(reinterpret_cast<void (*)(double)>(handleDistanceMeasurement));
}

void MapDisplay::startAreaMeasurement(std::function<void(double)> callback) {
    pImpl->areaMeasurementCallback = callback;
    js_startAreaMeasurement(reinterpret_cast<void (*)(double)>(handleAreaMeasurement));
}

void MapDisplay::startBearingMeasurement(std::function<void(double)> callback) {
    pImpl->bearingMeasurementCallback = callback;
    js_startBearingMeasurement(reinterpret_cast<void (*)(double)>(handleBearingMeasurement));
}

void MapDisplay::cancelMeasurement() {
    EM_ASM({
        if (window.dfMap) {
            if (window.dfMeasureControl) {
                window.dfMeasureControl.stopMeasuring();
            }
            if (window.dfBearingControl) {
                window.dfBearingControl.stopBearing();
            }
        }
    });
}

int MapDisplay::addTrack(const std::vector<std::tuple<double, double, int64_t>>& points,
                        const std::string& color,
                        double width) {
    if (points.empty()) {
        return -1;
    }

    // Convert points to JSON array
    nlohmann::json pointsJson = nlohmann::json::array();
    for (const auto& [lat, lon, timestamp] : points) {
        pointsJson.push_back({lat, lon, timestamp});
    }

    return js_addTrack(pointsJson.dump().c_str(), color.c_str(), width);
}

bool MapDisplay::removeTrack(int trackId) {
    return js_removeTrack(trackId);
}

void MapDisplay::setTrackTimeRange(int64_t startTime, int64_t endTime) {
    js_setTrackTimeRange(startTime, endTime);
}

void MapDisplay::setTrackAnimation(bool enabled, double speed) {
    js_setTrackAnimation(enabled, speed);
}

} // namespace ui
} // namespace df 