#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <functional>
#include <chrono>
#include <filesystem>

namespace df {
namespace mapping {

struct TileSourceConfig {
    std::string name;
    std::string url_template;
    std::string attribution;
    int min_zoom;
    int max_zoom;
    std::optional<std::string> api_key;
    std::optional<int> max_requests_per_minute;
    std::optional<std::string> subdomains;
    bool requires_https;
    bool retina;
};

struct TileSourceStats {
    size_t total_requests;
    size_t successful_requests;
    size_t failed_requests;
    double average_response_time_ms;
    std::chrono::system_clock::time_point last_request_time;
    size_t bytes_downloaded;
};

class TileSource {
public:
    virtual ~TileSource() = default;

    // Core functionality
    virtual std::string getName() const = 0;
    virtual std::string getAttribution() const = 0;
    virtual std::string buildUrl(int x, int y, int z) const = 0;
    virtual bool isValidTile(int x, int y, int z) const = 0;
    virtual std::pair<int, int> getZoomRange() const = 0;

    // Configuration
    virtual void setApiKey(const std::string& api_key) = 0;
    virtual void setRateLimit(int requests_per_minute) = 0;
    virtual void setRetina(bool enabled) = 0;
    virtual void setSubdomains(const std::string& subdomains) = 0;

    // Statistics
    virtual TileSourceStats getStats() const = 0;
    virtual void resetStats() = 0;

    // Factory method
    static std::unique_ptr<TileSource> create(const TileSourceConfig& config);
};

class TileSourceManager {
public:
    TileSourceManager();
    ~TileSourceManager();

    // Source management
    void addSource(const TileSourceConfig& config);
    void removeSource(const std::string& name);
    std::vector<std::string> listSources() const;
    std::shared_ptr<TileSource> getSource(const std::string& name) const;
    void setDefaultSource(const std::string& name);
    std::shared_ptr<TileSource> getDefaultSource() const;

    // Configuration
    void loadConfig(const std::filesystem::path& config_path);
    void saveConfig(const std::filesystem::path& config_path) const;
    void setGlobalApiKey(const std::string& provider, const std::string& api_key);

    // Statistics
    std::vector<std::pair<std::string, TileSourceStats>> getAllStats() const;
    void resetAllStats();

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace mapping
} // namespace df 