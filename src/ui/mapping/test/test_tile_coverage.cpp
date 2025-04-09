#include "../tile_coverage.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>

using namespace df::mapping;

// Helper function to create test tile files
void createTestTiles(const std::filesystem::path& cache_path) {
    std::filesystem::create_directories(cache_path);

    // Create a simple tile structure for testing
    // Zoom level 0 (complete coverage)
    std::filesystem::create_directories(cache_path / "0/0");
    std::ofstream(cache_path / "0/0/0.png").put('x');

    // Zoom level 1 (partial coverage)
    std::filesystem::create_directories(cache_path / "1/0");
    std::filesystem::create_directories(cache_path / "1/1");
    std::ofstream(cache_path / "1/0/0.png").put('x');
    std::ofstream(cache_path / "1/0/1.png").put('x');
    std::ofstream(cache_path / "1/1/0.png").put('x');

    // Zoom level 2 (sparse coverage)
    std::filesystem::create_directories(cache_path / "2/1");
    std::filesystem::create_directories(cache_path / "2/2");
    std::ofstream(cache_path / "2/1/1.png").put('x');
    std::ofstream(cache_path / "2/2/2.png").put('x');

    // Add some files with different ages
    auto now = std::filesystem::file_time_type::clock::now();
    std::filesystem::last_write_time(cache_path / "1/0/0.png", now);
    std::filesystem::last_write_time(cache_path / "1/0/1.png", now - std::chrono::hours(24));
    std::filesystem::last_write_time(cache_path / "1/1/0.png", now - std::chrono::hours(48));
}

void test_coverage_analysis() {
    std::cout << "Testing coverage analysis..." << std::endl;

    // Create temporary cache directory
    std::filesystem::path cache_path = "test_tile_cache";
    createTestTiles(cache_path);

    TileCoverageVisualizer visualizer;
    bool progress_called = false;
    visualizer.setProgressCallback([&](float progress, const std::string& status) {
        progress_called = true;
        std::cout << "Progress: " << (progress * 100) << "% - " << status << std::endl;
    });

    // Test full analysis
    auto stats = visualizer.analyzeCoverage(cache_path);
    assert(stats.total_tiles > 0);
    assert(stats.cached_tiles > 0);
    assert(stats.coverage_percentage > 0);
    assert(stats.coverage_percentage <= 100);
    assert(!stats.coverage_by_zoom.empty());
    assert(progress_called);

    // Test analysis for specific zoom levels
    auto zoom_stats = visualizer.analyzeCoverage(cache_path, {0, 1});
    assert(zoom_stats.coverage_by_zoom.size() == 2);
    assert(zoom_stats.coverage_by_zoom[0].first == 0);  // zoom level
    assert(zoom_stats.coverage_by_zoom[0].second == 100);  // 100% coverage at zoom 0
    assert(zoom_stats.coverage_by_zoom[1].first == 1);  // zoom level
    assert(zoom_stats.coverage_by_zoom[1].second == 75);  // 75% coverage at zoom 1

    std::cout << "Coverage analysis test passed." << std::endl;
}

void test_bounds_calculation() {
    std::cout << "Testing bounds calculation..." << std::endl;

    std::filesystem::path cache_path = "test_tile_cache";

    TileCoverageVisualizer visualizer;
    auto bounds = visualizer.getBounds(cache_path, 1);

    assert(bounds.min_x == 0);
    assert(bounds.min_y == 0);
    assert(bounds.max_x == 1);
    assert(bounds.max_y == 1);
    assert(bounds.zoom == 1);

    // Test invalid zoom level
    bool caught_exception = false;
    try {
        visualizer.getBounds(cache_path, 10);
    } catch (const std::runtime_error&) {
        caught_exception = true;
    }
    assert(caught_exception);

    std::cout << "Bounds calculation test passed." << std::endl;
}

void test_heatmap_generation() {
    std::cout << "Testing heatmap generation..." << std::endl;

    std::filesystem::path cache_path = "test_tile_cache";
    std::filesystem::path output_path = "test_heatmap.png";

    TileCoverageVisualizer visualizer;
    auto stats = visualizer.analyzeCoverage(cache_path);
    visualizer.generateHeatmap(output_path, stats, 1);

    assert(std::filesystem::exists(output_path));
    assert(std::filesystem::file_size(output_path) > 0);

    std::filesystem::remove(output_path);

    std::cout << "Heatmap generation test passed." << std::endl;
}

void test_coverage_report() {
    std::cout << "Testing coverage report generation..." << std::endl;

    std::filesystem::path cache_path = "test_tile_cache";
    std::filesystem::path output_path = "test_coverage_report.json";

    TileCoverageVisualizer visualizer;
    auto stats = visualizer.analyzeCoverage(cache_path);
    visualizer.generateCoverageReport(output_path, stats);

    assert(std::filesystem::exists(output_path));
    assert(std::filesystem::file_size(output_path) > 0);

    // Verify JSON content
    std::ifstream file(output_path);
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    assert(content.find("\"total_tiles\"") != std::string::npos);
    assert(content.find("\"coverage_percentage\"") != std::string::npos);
    assert(content.find("\"coverage_by_zoom\"") != std::string::npos);

    std::filesystem::remove(output_path);

    std::cout << "Coverage report generation test passed." << std::endl;
}

void cleanup() {
    std::filesystem::path cache_path = "test_tile_cache";
    if (std::filesystem::exists(cache_path)) {
        std::filesystem::remove_all(cache_path);
    }
}

int main() {
    try {
        cleanup();  // Clean up any leftover test files

        test_coverage_analysis();
        test_bounds_calculation();
        test_heatmap_generation();
        test_coverage_report();

        cleanup();  // Clean up test files

        std::cout << "\nAll tests passed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with error: " << e.what() << std::endl;
        cleanup();  // Clean up test files even if tests fail
        return 1;
    }
} 