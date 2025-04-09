#include "tile_source.h"
#include <unordered_map>
#include <fstream>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace df {
namespace mapping {

namespace {
class OSMTileSource : public TileSource {
public:
    explicit OSMTileSource(const TileSourceConfig& config)
        : name_(config.name)
        , url_template_(config.url_template)
        , attribution_(config.attribution)
        , min_zoom_(config.min_zoom)
        , max_zoom_(config.max_zoom)
        , requires_https_(config.requires_https)
        , retina_(config.retina)
        , stats_{}
    {
        if (config.api_key) {
            api_key_ = *config.api_key;
        }
        if (config.max_requests_per_minute) {
            rate_limit_ = *config.max_requests_per_minute;
        }
        if (config.subdomains) {
            setSubdomains(*config.subdomains);
        }
    }

    std::string getName() const override { return name_; }
    std::string getAttribution() const override { return attribution_; }

    std::string buildUrl(int x, int y, int z) const override {
        if (!isValidTile(x, y, z)) {
            throw std::invalid_argument("Invalid tile coordinates");
        }

        std::string url = url_template_;
        std::string subdomain = current_subdomain();

        // Replace placeholders in URL template
        url = std::regex_replace(url, std::regex("\\{x\\}"), std::to_string(x));
        url = std::regex_replace(url, std::regex("\\{y\\}"), std::to_string(y));
        url = std::regex_replace(url, std::regex("\\{z\\}"), std::to_string(z));
        url = std::regex_replace(url, std::regex("\\{s\\}"), subdomain);
        
        if (!api_key_.empty()) {
            url += (url.find('?') == std::string::npos ? "?" : "&");
            url += "apikey=" + api_key_;
        }

        if (requires_https_ && url.substr(0, 7) == "http://") {
            url = "https://" + url.substr(7);
        }

        if (retina_) {
            url = std::regex_replace(url, std::regex("(?=\\.[a-zA-Z]+$)"), "@2x");
        }

        return url;
    }

    bool isValidTile(int x, int y, int z) const override {
        if (z < min_zoom_ || z > max_zoom_) return false;
        int max_tile = 1 << z;
        return x >= 0 && x < max_tile && y >= 0 && y < max_tile;
    }

    std::pair<int, int> getZoomRange() const override {
        return {min_zoom_, max_zoom_};
    }

    void setApiKey(const std::string& api_key) override {
        api_key_ = api_key;
    }

    void setRateLimit(int requests_per_minute) override {
        if (requests_per_minute <= 0) {
            throw std::invalid_argument("Rate limit must be positive");
        }
        rate_limit_ = requests_per_minute;
    }

    void setRetina(bool enabled) override {
        retina_ = enabled;
    }

    void setSubdomains(const std::string& subdomains) override {
        subdomains_.clear();
        std::istringstream iss(subdomains);
        char subdomain;
        while (iss >> subdomain) {
            if (std::isalnum(subdomain)) {
                subdomains_.push_back(subdomain);
            }
        }
        if (subdomains_.empty()) {
            subdomains_.push_back('a');
        }
        subdomain_index_ = 0;
    }

    TileSourceStats getStats() const override {
        return stats_;
    }

    void resetStats() override {
        stats_ = TileSourceStats{};
    }

private:
    std::string current_subdomain() const {
        if (subdomains_.empty()) return "";
        size_t index = subdomain_index_++;
        subdomain_index_ %= subdomains_.size();
        return std::string(1, subdomains_[index]);
    }

    std::string name_;
    std::string url_template_;
    std::string attribution_;
    std::string api_key_;
    int min_zoom_;
    int max_zoom_;
    int rate_limit_{0};
    bool requires_https_;
    bool retina_;
    std::vector<char> subdomains_;
    mutable size_t subdomain_index_{0};
    TileSourceStats stats_;
};
} // namespace

std::unique_ptr<TileSource> TileSource::create(const TileSourceConfig& config) {
    return std::make_unique<OSMTileSource>(config);
}

struct TileSourceManager::Impl {
    std::unordered_map<std::string, std::shared_ptr<TileSource>> sources;
    std::string default_source;
    std::unordered_map<std::string, std::string> global_api_keys;
};

TileSourceManager::TileSourceManager() : pimpl(std::make_unique<Impl>()) {}
TileSourceManager::~TileSourceManager() = default;

void TileSourceManager::addSource(const TileSourceConfig& config) {
    auto source = std::shared_ptr<TileSource>(TileSource::create(config));
    pimpl->sources[config.name] = source;
    
    if (pimpl->sources.size() == 1) {
        setDefaultSource(config.name);
    }

    auto it = pimpl->global_api_keys.find(config.name);
    if (it != pimpl->global_api_keys.end()) {
        source->setApiKey(it->second);
    }
}

void TileSourceManager::removeSource(const std::string& name) {
    if (pimpl->default_source == name) {
        pimpl->default_source.clear();
        if (!pimpl->sources.empty()) {
            setDefaultSource(pimpl->sources.begin()->first);
        }
    }
    pimpl->sources.erase(name);
}

std::vector<std::string> TileSourceManager::listSources() const {
    std::vector<std::string> names;
    names.reserve(pimpl->sources.size());
    for (const auto& [name, _] : pimpl->sources) {
        names.push_back(name);
    }
    return names;
}

std::shared_ptr<TileSource> TileSourceManager::getSource(const std::string& name) const {
    auto it = pimpl->sources.find(name);
    if (it == pimpl->sources.end()) {
        throw std::runtime_error("Tile source not found: " + name);
    }
    return it->second;
}

void TileSourceManager::setDefaultSource(const std::string& name) {
    if (!pimpl->sources.count(name)) {
        throw std::runtime_error("Cannot set default source: source not found");
    }
    pimpl->default_source = name;
}

std::shared_ptr<TileSource> TileSourceManager::getDefaultSource() const {
    if (pimpl->default_source.empty()) {
        throw std::runtime_error("No default tile source set");
    }
    return getSource(pimpl->default_source);
}

void TileSourceManager::loadConfig(const std::filesystem::path& config_path) {
    std::ifstream file(config_path);
    if (!file) {
        throw std::runtime_error("Failed to open config file: " + config_path.string());
    }

    nlohmann::json config;
    file >> config;

    // Load global API keys
    if (config.contains("api_keys")) {
        auto& api_keys = config["api_keys"];
        for (auto it = api_keys.begin(); it != api_keys.end(); ++it) {
            pimpl->global_api_keys[it.key()] = it.value();
        }
    }

    // Load tile sources
    if (config.contains("sources")) {
        auto& sources = config["sources"];
        for (const auto& source : sources) {
            TileSourceConfig cfg{
                .name = source["name"],
                .url_template = source["url_template"],
                .attribution = source["attribution"],
                .min_zoom = source["min_zoom"],
                .max_zoom = source["max_zoom"],
                .requires_https = source.value("requires_https", false),
                .retina = source.value("retina", false)
            };

            if (source.contains("api_key")) {
                cfg.api_key = source["api_key"];
            }
            if (source.contains("max_requests_per_minute")) {
                cfg.max_requests_per_minute = source["max_requests_per_minute"];
            }
            if (source.contains("subdomains")) {
                cfg.subdomains = source["subdomains"];
            }

            addSource(cfg);
        }
    }

    // Set default source
    if (config.contains("default_source")) {
        setDefaultSource(config["default_source"]);
    }
}

void TileSourceManager::saveConfig(const std::filesystem::path& config_path) const {
    nlohmann::json config;

    // Save global API keys
    config["api_keys"] = pimpl->global_api_keys;

    // Save tile sources
    nlohmann::json sources = nlohmann::json::array();
    for (const auto& [name, source] : pimpl->sources) {
        nlohmann::json src;
        src["name"] = source->getName();
        src["attribution"] = source->getAttribution();
        auto [min_zoom, max_zoom] = source->getZoomRange();
        src["min_zoom"] = min_zoom;
        src["max_zoom"] = max_zoom;
        sources.push_back(src);
    }
    config["sources"] = sources;

    // Save default source
    if (!pimpl->default_source.empty()) {
        config["default_source"] = pimpl->default_source;
    }

    std::ofstream file(config_path);
    if (!file) {
        throw std::runtime_error("Failed to open config file for writing: " + config_path.string());
    }
    file << config.dump(2);
}

void TileSourceManager::setGlobalApiKey(const std::string& provider, const std::string& api_key) {
    pimpl->global_api_keys[provider] = api_key;
    auto it = pimpl->sources.find(provider);
    if (it != pimpl->sources.end()) {
        it->second->setApiKey(api_key);
    }
}

std::vector<std::pair<std::string, TileSourceStats>> TileSourceManager::getAllStats() const {
    std::vector<std::pair<std::string, TileSourceStats>> stats;
    stats.reserve(pimpl->sources.size());
    for (const auto& [name, source] : pimpl->sources) {
        stats.emplace_back(name, source->getStats());
    }
    return stats;
}

void TileSourceManager::resetAllStats() {
    for (auto& [_, source] : pimpl->sources) {
        source->resetStats();
    }
}

} // namespace mapping
} // namespace df 