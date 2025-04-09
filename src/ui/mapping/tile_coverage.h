#pragma once

#include <vector>
#include <memory>
#include <string>
#include <filesystem>
#include <optional>
#include <functional>

namespace df {
namespace mapping {

struct TileBounds {
    int min_x;
    int min_y;
    int max_x;
    int max_y;
    int zoom;
};

struct TileCoverageStats {
    size_t total_tiles;
    size_t cached_tiles;
    double coverage_percentage;
    std::vector<std::pair<int, double>> coverage_by_zoom;
    std::filesystem::path cache_path;
    uint64_t cache_size_bytes;
};

class TileCoverageVisualizer {
public:
    TileCoverageVisualizer();
    ~TileCoverageVisualizer();

    // Coverage analysis
    TileCoverageStats analyzeCoverage(const std::filesystem::path& cache_path,
                                    const std::vector<int>& zoom_levels = {}) const;
    
    TileBounds getBounds(const std::filesystem::path& cache_path,
                        int zoom_level) const;

    // Visualization generation
    void generateHeatmap(const std::filesystem::path& output_path,
                        const TileCoverageStats& stats,
                        int zoom_level) const;

    void generateCoverageReport(const std::filesystem::path& output_path,
                              const TileCoverageStats& stats) const;

    // Progress callback for long operations
    using ProgressCallback = std::function<void(float progress, const std::string& status)>;
    void setProgressCallback(ProgressCallback callback);

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace mapping
} // namespace df 