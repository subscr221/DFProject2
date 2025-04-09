#include "tile_coverage.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <regex>
#include <cmath>
#include <queue>
#include <unordered_set>
#include <opencv2/opencv.hpp>

namespace df {
namespace mapping {

namespace {
// Helper function to parse tile coordinates from filename
bool parseTileCoords(const std::string& filename, int& x, int& y, int& z) {
    static const std::regex pattern(R"((\d+)/(\d+)/(\d+)\.[a-zA-Z]+$)");
    std::smatch matches;
    if (std::regex_search(filename, matches, pattern)) {
        z = std::stoi(matches[1]);
        x = std::stoi(matches[2]);
        y = std::stoi(matches[3]);
        return true;
    }
    return false;
}

// Helper function to calculate total tiles at zoom level
size_t totalTilesAtZoom(int zoom) {
    size_t tiles_per_side = 1 << zoom;
    return tiles_per_side * tiles_per_side;
}

// Helper function to convert tile coordinates to lat/lon
void tileToLatLon(int x, int y, int z, double& lat, double& lon) {
    int n = 1 << z;
    lon = x / static_cast<double>(n) * 360.0 - 180.0;
    double lat_rad = std::atan(std::sinh(M_PI * (1 - 2 * y / static_cast<double>(n))));
    lat = lat_rad * 180.0 / M_PI;
}

// Helper function to get file size
uint64_t getFileSize(const std::filesystem::path& path) {
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    return ec ? 0 : size;
}
} // namespace

struct TileCoverageVisualizer::Impl {
    ProgressCallback progress_callback;
    
    void reportProgress(float progress, const std::string& status) const {
        if (progress_callback) {
            progress_callback(progress, status);
        }
    }
};

TileCoverageVisualizer::TileCoverageVisualizer() : pimpl(std::make_unique<Impl>()) {}
TileCoverageVisualizer::~TileCoverageVisualizer() = default;

void TileCoverageVisualizer::setProgressCallback(ProgressCallback callback) {
    pimpl->progress_callback = std::move(callback);
}

TileCoverageStats TileCoverageVisualizer::analyzeCoverage(
    const std::filesystem::path& cache_path,
    const std::vector<int>& zoom_levels) const {
    
    TileCoverageStats stats;
    stats.cache_path = cache_path;
    stats.cache_size_bytes = 0;
    stats.total_tiles = 0;
    stats.cached_tiles = 0;

    // Track tiles at each zoom level
    std::unordered_map<int, size_t> tiles_by_zoom;
    
    // Scan cache directory
    size_t total_files = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(cache_path)) {
        if (entry.is_regular_file()) {
            total_files++;
        }
    }

    size_t processed_files = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(cache_path)) {
        if (entry.is_regular_file()) {
            int x, y, z;
            if (parseTileCoords(entry.path().string(), x, y, z)) {
                if (zoom_levels.empty() || std::find(zoom_levels.begin(), zoom_levels.end(), z) != zoom_levels.end()) {
                    tiles_by_zoom[z]++;
                    stats.cache_size_bytes += getFileSize(entry.path());
                }
            }
            
            processed_files++;
            pimpl->reportProgress(
                static_cast<float>(processed_files) / total_files,
                "Analyzing cache contents..."
            );
        }
    }

    // Calculate coverage statistics
    for (const auto& [zoom, count] : tiles_by_zoom) {
        size_t total = totalTilesAtZoom(zoom);
        double coverage = static_cast<double>(count) / total * 100.0;
        stats.coverage_by_zoom.emplace_back(zoom, coverage);
        stats.total_tiles += total;
        stats.cached_tiles += count;
    }

    if (stats.total_tiles > 0) {
        stats.coverage_percentage = static_cast<double>(stats.cached_tiles) / stats.total_tiles * 100.0;
    } else {
        stats.coverage_percentage = 0.0;
    }

    return stats;
}

TileBounds TileCoverageVisualizer::getBounds(
    const std::filesystem::path& cache_path,
    int zoom_level) const {
    
    TileBounds bounds{
        .min_x = std::numeric_limits<int>::max(),
        .min_y = std::numeric_limits<int>::max(),
        .max_x = std::numeric_limits<int>::min(),
        .max_y = std::numeric_limits<int>::min(),
        .zoom = zoom_level
    };

    bool found_tiles = false;
    size_t total_files = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(cache_path)) {
        if (entry.is_regular_file()) {
            total_files++;
        }
    }

    size_t processed_files = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(cache_path)) {
        if (entry.is_regular_file()) {
            int x, y, z;
            if (parseTileCoords(entry.path().string(), x, y, z) && z == zoom_level) {
                bounds.min_x = std::min(bounds.min_x, x);
                bounds.min_y = std::min(bounds.min_y, y);
                bounds.max_x = std::max(bounds.max_x, x);
                bounds.max_y = std::max(bounds.max_y, y);
                found_tiles = true;
            }
            
            processed_files++;
            pimpl->reportProgress(
                static_cast<float>(processed_files) / total_files,
                "Calculating bounds..."
            );
        }
    }

    if (!found_tiles) {
        throw std::runtime_error("No tiles found at zoom level " + std::to_string(zoom_level));
    }

    return bounds;
}

void TileCoverageVisualizer::generateHeatmap(
    const std::filesystem::path& output_path,
    const TileCoverageStats& stats,
    int zoom_level) const {
    
    // Get bounds for the specified zoom level
    auto bounds = getBounds(stats.cache_path, zoom_level);
    
    // Create heatmap image
    int width = bounds.max_x - bounds.min_x + 1;
    int height = bounds.max_y - bounds.min_y + 1;
    cv::Mat heatmap(height, width, CV_8UC3, cv::Scalar(0, 0, 0));

    // Track which tiles exist
    std::unordered_set<std::string> existing_tiles;
    size_t total_files = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(stats.cache_path)) {
        if (entry.is_regular_file()) {
            total_files++;
        }
    }

    size_t processed_files = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(stats.cache_path)) {
        if (entry.is_regular_file()) {
            int x, y, z;
            if (parseTileCoords(entry.path().string(), x, y, z) && z == zoom_level) {
                // Normalize coordinates to image space
                int img_x = x - bounds.min_x;
                int img_y = y - bounds.min_y;
                
                // Color based on file age
                auto last_write_time = std::filesystem::last_write_time(entry.path());
                auto age = std::filesystem::file_time_type::clock::now() - last_write_time;
                auto hours = std::chrono::duration_cast<std::chrono::hours>(age).count();
                
                // Color gradient: recent = green, old = red
                int green = std::max(0, std::min(255, static_cast<int>(255 * std::exp(-hours / 720.0))));
                int red = 255 - green;
                
                heatmap.at<cv::Vec3b>(img_y, img_x) = cv::Vec3b(0, green, red);
            }
            
            processed_files++;
            pimpl->reportProgress(
                static_cast<float>(processed_files) / total_files,
                "Generating heatmap..."
            );
        }
    }

    // Scale up the image for better visibility
    cv::Mat scaled;
    cv::resize(heatmap, scaled, cv::Size(), 4, 4, cv::INTER_NEAREST);

    // Add grid lines
    for (int i = 0; i <= height; ++i) {
        cv::line(scaled, cv::Point(0, i * 4), cv::Point(width * 4, i * 4), cv::Scalar(128, 128, 128));
    }
    for (int i = 0; i <= width; ++i) {
        cv::line(scaled, cv::Point(i * 4, 0), cv::Point(i * 4, height * 4), cv::Scalar(128, 128, 128));
    }

    // Save the heatmap
    cv::imwrite(output_path.string(), scaled);
}

void TileCoverageVisualizer::generateCoverageReport(
    const std::filesystem::path& output_path,
    const TileCoverageStats& stats) const {
    
    nlohmann::json report;
    
    // Basic statistics
    report["total_tiles"] = stats.total_tiles;
    report["cached_tiles"] = stats.cached_tiles;
    report["coverage_percentage"] = stats.coverage_percentage;
    report["cache_size_mb"] = stats.cache_size_bytes / (1024.0 * 1024.0);
    
    // Coverage by zoom level
    nlohmann::json zoom_coverage = nlohmann::json::array();
    for (const auto& [zoom, coverage] : stats.coverage_by_zoom) {
        nlohmann::json level;
        level["zoom"] = zoom;
        level["coverage"] = coverage;
        level["tiles"] = totalTilesAtZoom(zoom);
        zoom_coverage.push_back(level);
    }
    report["coverage_by_zoom"] = zoom_coverage;
    
    // Generate bounds for each zoom level
    nlohmann::json bounds = nlohmann::json::array();
    for (const auto& [zoom, _] : stats.coverage_by_zoom) {
        try {
            auto tile_bounds = getBounds(stats.cache_path, zoom);
            nlohmann::json bound;
            bound["zoom"] = zoom;
            bound["min_x"] = tile_bounds.min_x;
            bound["min_y"] = tile_bounds.min_y;
            bound["max_x"] = tile_bounds.max_x;
            bound["max_y"] = tile_bounds.max_y;
            
            // Calculate lat/lon bounds
            double min_lat, min_lon, max_lat, max_lon;
            tileToLatLon(tile_bounds.min_x, tile_bounds.max_y, zoom, min_lat, min_lon);
            tileToLatLon(tile_bounds.max_x + 1, tile_bounds.min_y, zoom, max_lat, max_lon);
            
            bound["min_lat"] = min_lat;
            bound["min_lon"] = min_lon;
            bound["max_lat"] = max_lat;
            bound["max_lon"] = max_lon;
            
            bounds.push_back(bound);
        } catch (const std::exception& e) {
            // Skip zoom levels with no tiles
            continue;
        }
    }
    report["bounds"] = bounds;
    
    // Write report to file
    std::ofstream file(output_path);
    file << report.dump(2);
}

} // namespace mapping
} // namespace df 