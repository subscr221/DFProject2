#include "tile_server.h"
#include <httplib.h>
#include <zlib.h>
#include <json.hpp>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <curl/curl.h>

using json = nlohmann::json;

namespace df {
namespace ui {

namespace {
    // Constants
    constexpr int TILE_SIZE = 256;
    constexpr int EARTH_RADIUS = 6378137;
    constexpr double PI = 3.14159265358979323846;
    
    // Helper functions
    std::string getTilePath(const std::string& basePath, int z, int x, int y) {
        std::stringstream ss;
        ss << basePath << "/" << z << "/" << x << "/" << y << ".png";
        return ss.str();
    }
    
    std::vector<uint8_t> compressTile(const std::vector<uint8_t>& data, int level) {
        if (data.empty()) return {};
        
        z_stream zs;
        zs.zalloc = Z_NULL;
        zs.zfree = Z_NULL;
        zs.opaque = Z_NULL;
        
        if (deflateInit(&zs, level) != Z_OK) {
            return {};
        }
        
        zs.avail_in = static_cast<uInt>(data.size());
        zs.next_in = const_cast<Bytef*>(data.data());
        
        std::vector<uint8_t> compressed;
        compressed.resize(deflateBound(&zs, data.size()));
        
        zs.avail_out = static_cast<uInt>(compressed.size());
        zs.next_out = compressed.data();
        
        if (deflate(&zs, Z_FINISH) != Z_STREAM_END) {
            deflateEnd(&zs);
            return {};
        }
        
        compressed.resize(zs.total_out);
        deflateEnd(&zs);
        
        return compressed;
    }
    
    std::vector<uint8_t> decompressTile(const std::vector<uint8_t>& data) {
        if (data.empty()) return {};
        
        z_stream zs;
        zs.zalloc = Z_NULL;
        zs.zfree = Z_NULL;
        zs.opaque = Z_NULL;
        
        if (inflateInit(&zs) != Z_OK) {
            return {};
        }
        
        zs.avail_in = static_cast<uInt>(data.size());
        zs.next_in = const_cast<Bytef*>(data.data());
        
        std::vector<uint8_t> decompressed;
        const size_t CHUNK = 16384;
        std::vector<uint8_t> chunk(CHUNK);
        
        do {
            zs.avail_out = static_cast<uInt>(chunk.size());
            zs.next_out = chunk.data();
            
            int ret = inflate(&zs, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END) {
                inflateEnd(&zs);
                return {};
            }
            
            decompressed.insert(decompressed.end(),
                              chunk.begin(),
                              chunk.begin() + (chunk.size() - zs.avail_out));
        } while (zs.avail_out == 0);
        
        inflateEnd(&zs);
        return decompressed;
    }
    
    size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        auto* response = static_cast<std::vector<uint8_t>*>(userp);
        size_t realsize = size * nmemb;
        response->insert(response->end(),
                       static_cast<uint8_t*>(contents),
                       static_cast<uint8_t*>(contents) + realsize);
        return realsize;
    }
}

struct TileServer::Impl {
    std::string cachePath;
    size_t maxCacheSize;
    size_t maxConcurrentDownloads;
    std::atomic<bool> running{false};
    std::unique_ptr<httplib::Server> server;
    std::thread serverThread;
    
    // Download queue
    struct DownloadRequest {
        TileRequest request;
        std::function<void(const TileResponse&)> callback;
    };
    std::queue<DownloadRequest> downloadQueue;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    std::vector<std::thread> downloadThreads;
    
    // Statistics
    TileStats stats;
    std::mutex statsMutex;
    
    // Rate limiting
    size_t requestsPerMinute = 15000;  // OSM default limit
    std::chrono::system_clock::time_point lastRateCheck;
    size_t requestsThisMinute = 0;
    std::mutex rateMutex;
    
    // Compression
    bool compressionEnabled = true;
    int compressionLevel = 6;
    
    // Update policy
    int checkIntervalHours = 24;
    int maxAgeHours = 168;  // 1 week
    std::thread updateThread;
    
    Impl(const std::string& path, size_t maxSize, size_t maxDownloads)
        : cachePath(path), maxCacheSize(maxSize), maxConcurrentDownloads(maxDownloads) {
        std::filesystem::create_directories(cachePath);
        curl_global_init(CURL_GLOBAL_ALL);
    }
    
    ~Impl() {
        stop();
        curl_global_cleanup();
    }
    
    bool start(uint16_t port) {
        if (running) return false;
        
        server = std::make_unique<httplib::Server>();
        
        // Set up routes
        server->Get("/tile/:z/:x/:y", [this](const httplib::Request& req, httplib::Response& res) {
            handleTileRequest(req, res);
        });
        
        server->Get("/stats", [this](const httplib::Request&, httplib::Response& res) {
            handleStatsRequest(res);
        });
        
        // Start server
        running = true;
        serverThread = std::thread([this, port]() {
            server->listen("localhost", port);
        });
        
        // Start download threads
        for (size_t i = 0; i < maxConcurrentDownloads; ++i) {
            downloadThreads.emplace_back([this]() {
                downloadWorker();
            });
        }
        
        // Start update thread
        updateThread = std::thread([this]() {
            updateWorker();
        });
        
        return true;
    }
    
    void stop() {
        if (!running) return;
        
        running = false;
        
        // Stop server
        if (server) {
            server->stop();
        }
        if (serverThread.joinable()) {
            serverThread.join();
        }
        
        // Stop download threads
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            queueCV.notify_all();
        }
        for (auto& thread : downloadThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        downloadThreads.clear();
        
        // Stop update thread
        if (updateThread.joinable()) {
            updateThread.join();
        }
    }
    
    void handleTileRequest(const httplib::Request& req, httplib::Response& res) {
        // Parse parameters
        int z = std::stoi(req.matches[1]);
        int x = std::stoi(req.matches[2]);
        int y = std::stoi(req.matches[3]);
        
        // Check rate limit
        if (!checkRateLimit()) {
            res.status = 429;  // Too Many Requests
            return;
        }
        
        // Try to get tile from cache
        auto tilePath = getTilePath(cachePath, z, x, y);
        if (std::filesystem::exists(tilePath)) {
            serveCachedTile(tilePath, res);
            return;
        }
        
        // Download tile
        downloadTile(z, x, y, [this, &res](const TileResponse& tileRes) {
            if (tileRes.data.empty()) {
                res.status = 404;
                return;
            }
            
            res.set_content(
                reinterpret_cast<const char*>(tileRes.data.data()),
                tileRes.data.size(),
                tileRes.contentType
            );
            if (!tileRes.etag.empty()) {
                res.set_header("ETag", tileRes.etag);
            }
        });
    }
    
    void handleStatsRequest(httplib::Response& res) {
        std::lock_guard<std::mutex> lock(statsMutex);
        json j = {
            {"totalTiles", stats.totalTiles},
            {"cachedTiles", stats.cachedTiles},
            {"totalSizeBytes", stats.totalSizeBytes},
            {"requestsServed", stats.requestsServed},
            {"averageLatencyMs", stats.averageLatencyMs},
            {"cacheHits", stats.cacheHits},
            {"cacheMisses", stats.cacheMisses}
        };
        res.set_content(j.dump(), "application/json");
    }
    
    void downloadWorker() {
        CURL* curl = curl_easy_init();
        if (!curl) return;
        
        while (running) {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCV.wait(lock, [this]() {
                return !running || !downloadQueue.empty();
            });
            
            if (!running) break;
            
            auto request = downloadQueue.front();
            downloadQueue.pop();
            lock.unlock();
            
            // Download tile
            std::string url = "https://tile.openstreetmap.org/" +
                            std::to_string(request.request.z) + "/" +
                            std::to_string(request.request.x) + "/" +
                            std::to_string(request.request.y) + ".png";
            
            std::vector<uint8_t> response;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            
            auto start = std::chrono::steady_clock::now();
            CURLcode res = curl_easy_perform(curl);
            auto end = std::chrono::steady_clock::now();
            
            if (res == CURLE_OK) {
                // Save to cache
                auto tilePath = getTilePath(cachePath, request.request.z,
                                         request.request.x, request.request.y);
                std::filesystem::create_directories(
                    std::filesystem::path(tilePath).parent_path()
                );
                
                if (compressionEnabled) {
                    response = compressTile(response, compressionLevel);
                }
                
                std::ofstream file(tilePath, std::ios::binary);
                file.write(reinterpret_cast<const char*>(response.data()),
                          response.size());
                
                // Update stats
                {
                    std::lock_guard<std::mutex> statsLock(statsMutex);
                    stats.totalTiles++;
                    stats.cachedTiles++;
                    stats.totalSizeBytes += response.size();
                    stats.averageLatencyMs = (stats.averageLatencyMs * stats.requestsServed +
                        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()) /
                        (stats.requestsServed + 1);
                    stats.requestsServed++;
                    stats.cacheMisses++;
                }
                
                // Call callback
                TileResponse tileRes;
                tileRes.data = response;
                tileRes.contentType = "image/png";
                request.callback(tileRes);
            }
        }
        
        curl_easy_cleanup(curl);
    }
    
    void updateWorker() {
        while (running) {
            std::this_thread::sleep_for(
                std::chrono::hours(checkIntervalHours)
            );
            
            if (!running) break;
            
            // Find and update old tiles
            auto now = std::chrono::system_clock::now();
            auto maxAge = std::chrono::hours(maxAgeHours);
            
            for (const auto& entry :
                 std::filesystem::recursive_directory_iterator(cachePath)) {
                if (!running) break;
                
                if (entry.is_regular_file() &&
                    entry.path().extension() == ".png") {
                    auto ftime = std::filesystem::last_write_time(entry.path());
                    auto age = now - std::chrono::file_clock::to_sys(ftime);
                    
                    if (age > maxAge) {
                        // Queue tile for update
                        auto path = entry.path();
                        auto filename = path.filename().string();
                        auto y = std::stoi(filename.substr(0, filename.find('.')));
                        auto x = std::stoi(path.parent_path().filename().string());
                        auto z = std::stoi(path.parent_path().parent_path().filename().string());
                        
                        TileRequest req;
                        req.z = z;
                        req.x = x;
                        req.y = y;
                        req.priority = false;
                        req.timestamp = std::chrono::system_clock::now();
                        
                        std::lock_guard<std::mutex> lock(queueMutex);
                        downloadQueue.push({req, [](const TileResponse&){}});
                        queueCV.notify_one();
                    }
                }
            }
        }
    }
    
    bool checkRateLimit() {
        std::lock_guard<std::mutex> lock(rateMutex);
        
        auto now = std::chrono::system_clock::now();
        if (now - lastRateCheck > std::chrono::minutes(1)) {
            lastRateCheck = now;
            requestsThisMinute = 0;
        }
        
        if (requestsThisMinute >= requestsPerMinute) {
            return false;
        }
        
        requestsThisMinute++;
        return true;
    }
    
    void serveCachedTile(const std::string& path, httplib::Response& res) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            res.status = 404;
            return;
        }
        
        std::vector<uint8_t> data(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );
        
        if (compressionEnabled) {
            data = decompressTile(data);
        }
        
        res.set_content(
            reinterpret_cast<const char*>(data.data()),
            data.size(),
            "image/png"
        );
        
        // Update stats
        std::lock_guard<std::mutex> lock(statsMutex);
        stats.requestsServed++;
        stats.cacheHits++;
    }
    
    void downloadTile(int z, int x, int y,
                     std::function<void(const TileResponse&)> callback) {
        TileRequest req;
        req.z = z;
        req.x = x;
        req.y = y;
        req.timestamp = std::chrono::system_clock::now();
        
        std::lock_guard<std::mutex> lock(queueMutex);
        downloadQueue.push({req, callback});
        queueCV.notify_one();
    }
};

TileServer::TileServer(const std::string& cachePath,
                      size_t maxCacheSize,
                      size_t maxConcurrentDownloads)
    : pImpl(std::make_unique<Impl>(cachePath, maxCacheSize, maxConcurrentDownloads)) {}

TileServer::~TileServer() = default;

bool TileServer::start(uint16_t port) {
    return pImpl->start(port);
}

void TileServer::stop() {
    pImpl->stop();
}

TileStats TileServer::getStats() const {
    std::lock_guard<std::mutex> lock(pImpl->statsMutex);
    return pImpl->stats;
}

bool TileServer::downloadArea(double minLat, double minLon,
                            double maxLat, double maxLon,
                            int minZoom, int maxZoom,
                            std::function<void(double)> progressCallback) {
    // Convert lat/lon to tile coordinates
    auto latToY = [](double lat, int z) {
        double latRad = lat * PI / 180.0;
        return static_cast<int>((1.0 - asinh(tan(latRad)) / PI) / 2.0 * (1 << z));
    };
    
    auto lonToX = [](double lon, int z) {
        return static_cast<int>((lon + 180.0) / 360.0 * (1 << z));
    };
    
    size_t totalTiles = 0;
    size_t downloadedTiles = 0;
    
    // Calculate total tiles
    for (int z = minZoom; z <= maxZoom; ++z) {
        int minX = lonToX(minLon, z);
        int maxX = lonToX(maxLon, z);
        int minY = latToY(maxLat, z);  // Note: y coordinates are inverted
        int maxY = latToY(minLat, z);
        
        totalTiles += (maxX - minX + 1) * (maxY - minY + 1);
    }
    
    // Download tiles
    std::atomic<bool> success{true};
    std::mutex callbackMutex;
    
    for (int z = minZoom; z <= maxZoom && success; ++z) {
        int minX = lonToX(minLon, z);
        int maxX = lonToX(maxLon, z);
        int minY = latToY(maxLat, z);
        int maxY = latToY(minLat, z);
        
        for (int x = minX; x <= maxX && success; ++x) {
            for (int y = minY; y <= maxY && success; ++y) {
                TileRequest req;
                req.z = z;
                req.x = x;
                req.y = y;
                req.priority = true;
                req.timestamp = std::chrono::system_clock::now();
                
                std::promise<bool> downloadPromise;
                auto downloadFuture = downloadPromise.get_future();
                
                {
                    std::lock_guard<std::mutex> lock(pImpl->queueMutex);
                    pImpl->downloadQueue.push({req, [&](const TileResponse& res) {
                        downloadPromise.set_value(!res.data.empty());
                        
                        if (progressCallback) {
                            std::lock_guard<std::mutex> cbLock(callbackMutex);
                            downloadedTiles++;
                            progressCallback(static_cast<double>(downloadedTiles) / totalTiles);
                        }
                    }});
                    pImpl->queueCV.notify_one();
                }
                
                if (!downloadFuture.get()) {
                    success = false;
                    break;
                }
            }
        }
    }
    
    return success;
}

size_t TileServer::clearCache(std::optional<std::chrono::system_clock::time_point> olderThan) {
    size_t clearedTiles = 0;
    
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(pImpl->cachePath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".png") {
            bool shouldDelete = true;
            
            if (olderThan) {
                auto ftime = std::filesystem::last_write_time(entry.path());
                auto age = *olderThan - std::chrono::file_clock::to_sys(ftime);
                shouldDelete = age > std::chrono::seconds(0);
            }
            
            if (shouldDelete) {
                std::filesystem::remove(entry.path());
                clearedTiles++;
            }
        }
    }
    
    return clearedTiles;
}

std::vector<std::pair<int, size_t>> TileServer::getCoverage() const {
    std::vector<std::pair<int, size_t>> coverage;
    
    for (const auto& entry :
         std::filesystem::directory_iterator(pImpl->cachePath)) {
        if (entry.is_directory()) {
            try {
                int zoom = std::stoi(entry.path().filename().string());
                size_t count = 0;
                
                for (const auto& _ :
                     std::filesystem::recursive_directory_iterator(entry.path())) {
                    if (_.is_regular_file() && _.path().extension() == ".png") {
                        count++;
                    }
                }
                
                coverage.emplace_back(zoom, count);
            } catch (...) {
                continue;
            }
        }
    }
    
    std::sort(coverage.begin(), coverage.end());
    return coverage;
}

void TileServer::setRateLimit(size_t requestsPerMinute) {
    std::lock_guard<std::mutex> lock(pImpl->rateMutex);
    pImpl->requestsPerMinute = requestsPerMinute;
}

void TileServer::setCompression(bool enable, int level) {
    pImpl->compressionEnabled = enable;
    pImpl->compressionLevel = level;
}

void TileServer::setUpdatePolicy(int checkIntervalHours, int maxAgeHours) {
    pImpl->checkIntervalHours = checkIntervalHours;
    pImpl->maxAgeHours = maxAgeHours;
}

bool TileServer::synchronize(const std::string& serverUrl,
                           std::function<void(double)> progressCallback) {
    // Get remote coverage
    CURL* curl = curl_easy_init();
    if (!curl) return false;
    
    std::string coverageUrl = serverUrl + "/stats";
    std::vector<uint8_t> response;
    
    curl_easy_setopt(curl, CURLOPT_URL, coverageUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) return false;
    
    try {
        auto j = json::parse(response);
        size_t totalTiles = j["totalTiles"];
        size_t syncedTiles = 0;
        
        // Sync tiles
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(pImpl->cachePath)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".png") {
                continue;
            }
            
            auto path = entry.path();
            auto filename = path.filename().string();
            auto y = std::stoi(filename.substr(0, filename.find('.')));
            auto x = std::stoi(path.parent_path().filename().string());
            auto z = std::stoi(path.parent_path().parent_path().filename().string());
            
            std::string tileUrl = serverUrl + "/tile/" +
                                std::to_string(z) + "/" +
                                std::to_string(x) + "/" +
                                std::to_string(y);
            
            TileRequest req;
            req.z = z;
            req.x = x;
            req.y = y;
            req.priority = true;
            req.timestamp = std::chrono::system_clock::now();
            
            std::promise<bool> syncPromise;
            auto syncFuture = syncPromise.get_future();
            
            {
                std::lock_guard<std::mutex> lock(pImpl->queueMutex);
                pImpl->downloadQueue.push({req, [&](const TileResponse& res) {
                    syncPromise.set_value(!res.data.empty());
                    
                    if (progressCallback) {
                        syncedTiles++;
                        progressCallback(static_cast<double>(syncedTiles) / totalTiles);
                    }
                }});
                pImpl->queueCV.notify_one();
            }
            
            if (!syncFuture.get()) {
                return false;
            }
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace ui
} // namespace df 