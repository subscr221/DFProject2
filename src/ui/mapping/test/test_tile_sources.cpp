#include "../tile_source.h"
#include <iostream>
#include <cassert>
#include <filesystem>

using namespace df::mapping;

void test_tile_source_config() {
    std::cout << "Testing tile source configuration..." << std::endl;

    TileSourceConfig config{
        .name = "OpenStreetMap",
        .url_template = "https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png",
        .attribution = "© OpenStreetMap contributors",
        .min_zoom = 0,
        .max_zoom = 19,
        .requires_https = true,
        .retina = false
    };

    auto source = TileSource::create(config);
    assert(source->getName() == "OpenStreetMap");
    assert(source->getAttribution() == "© OpenStreetMap contributors");
    
    auto [min_zoom, max_zoom] = source->getZoomRange();
    assert(min_zoom == 0);
    assert(max_zoom == 19);

    std::cout << "Tile source configuration test passed." << std::endl;
}

void test_tile_url_generation() {
    std::cout << "Testing tile URL generation..." << std::endl;

    TileSourceConfig config{
        .name = "Test Source",
        .url_template = "https://{s}.example.com/{z}/{x}/{y}.png",
        .attribution = "Test Attribution",
        .min_zoom = 0,
        .max_zoom = 18,
        .subdomains = "abc",
        .requires_https = true,
        .retina = false
    };

    auto source = TileSource::create(config);
    
    // Test URL generation
    std::string url = source->buildUrl(1, 2, 3);
    assert(url.find("https://") == 0);
    assert(url.find("/3/1/2.png") != std::string::npos);

    // Test invalid coordinates
    bool caught_exception = false;
    try {
        source->buildUrl(1, 2, 19); // Beyond max zoom
    } catch (const std::invalid_argument&) {
        caught_exception = true;
    }
    assert(caught_exception);

    std::cout << "Tile URL generation test passed." << std::endl;
}

void test_tile_source_manager() {
    std::cout << "Testing tile source manager..." << std::endl;

    TileSourceManager manager;

    // Test adding sources
    TileSourceConfig osm{
        .name = "OpenStreetMap",
        .url_template = "https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png",
        .attribution = "© OpenStreetMap contributors",
        .min_zoom = 0,
        .max_zoom = 19,
        .requires_https = true,
        .retina = false
    };

    TileSourceConfig satellite{
        .name = "Satellite",
        .url_template = "https://api.example.com/satellite/{z}/{x}/{y}.jpg",
        .attribution = "© Satellite Provider",
        .min_zoom = 0,
        .max_zoom = 18,
        .requires_https = true,
        .retina = true
    };

    manager.addSource(osm);
    manager.addSource(satellite);

    // Test listing sources
    auto sources = manager.listSources();
    assert(sources.size() == 2);
    assert(std::find(sources.begin(), sources.end(), "OpenStreetMap") != sources.end());
    assert(std::find(sources.begin(), sources.end(), "Satellite") != sources.end());

    // Test default source
    auto default_source = manager.getDefaultSource();
    assert(default_source->getName() == "OpenStreetMap");

    // Test changing default source
    manager.setDefaultSource("Satellite");
    default_source = manager.getDefaultSource();
    assert(default_source->getName() == "Satellite");

    // Test removing source
    manager.removeSource("OpenStreetMap");
    sources = manager.listSources();
    assert(sources.size() == 1);
    assert(sources[0] == "Satellite");

    std::cout << "Tile source manager test passed." << std::endl;
}

void test_config_file_operations() {
    std::cout << "Testing config file operations..." << std::endl;

    TileSourceManager manager;
    
    // Add some sources
    TileSourceConfig osm{
        .name = "OpenStreetMap",
        .url_template = "https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png",
        .attribution = "© OpenStreetMap contributors",
        .min_zoom = 0,
        .max_zoom = 19,
        .requires_https = true,
        .retina = false
    };

    manager.addSource(osm);
    manager.setGlobalApiKey("MapProvider", "test-api-key");

    // Save configuration
    std::filesystem::path config_path = "test_config.json";
    manager.saveConfig(config_path);

    // Create new manager and load configuration
    TileSourceManager new_manager;
    new_manager.loadConfig(config_path);

    // Verify loaded configuration
    auto sources = new_manager.listSources();
    assert(sources.size() == 1);
    assert(sources[0] == "OpenStreetMap");

    auto source = new_manager.getSource("OpenStreetMap");
    assert(source->getName() == "OpenStreetMap");
    assert(source->getAttribution() == "© OpenStreetMap contributors");

    // Clean up
    std::filesystem::remove(config_path);

    std::cout << "Config file operations test passed." << std::endl;
}

int main() {
    try {
        test_tile_source_config();
        test_tile_url_generation();
        test_tile_source_manager();
        test_config_file_operations();
        
        std::cout << "\nAll tests passed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with error: " << e.what() << std::endl;
        return 1;
    }
} 