#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>

namespace tdoa {
namespace database {

// Database schema version
constexpr int SCHEMA_VERSION = 1;

// Table names
constexpr const char* TABLE_SIGNALS = "signals";
constexpr const char* TABLE_GEOLOCATIONS = "geolocations";
constexpr const char* TABLE_EVENTS = "events";
constexpr const char* TABLE_REPORTS = "reports";
constexpr const char* TABLE_METADATA = "metadata";

// Schema creation statements
constexpr const char* CREATE_METADATA_TABLE = R"(
    CREATE TABLE IF NOT EXISTS metadata (
        key TEXT PRIMARY KEY,
        value TEXT NOT NULL
    );
)";

constexpr const char* CREATE_SIGNALS_TABLE = R"(
    CREATE TABLE IF NOT EXISTS signals (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        timestamp INTEGER NOT NULL,
        frequency REAL NOT NULL,
        bandwidth REAL NOT NULL,
        power REAL NOT NULL,
        snr REAL NOT NULL,
        signal_class TEXT,
        confidence REAL,
        node_id TEXT NOT NULL,
        track_id TEXT,
        metadata TEXT,
        created_at INTEGER NOT NULL,
        updated_at INTEGER NOT NULL
    );
    CREATE INDEX IF NOT EXISTS idx_signals_timestamp ON signals(timestamp);
    CREATE INDEX IF NOT EXISTS idx_signals_frequency ON signals(frequency);
    CREATE INDEX IF NOT EXISTS idx_signals_track_id ON signals(track_id);
)";

constexpr const char* CREATE_GEOLOCATIONS_TABLE = R"(
    CREATE TABLE IF NOT EXISTS geolocations (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        timestamp INTEGER NOT NULL,
        latitude REAL NOT NULL,
        longitude REAL NOT NULL,
        altitude REAL,
        accuracy REAL,
        signal_id INTEGER NOT NULL,
        track_id TEXT,
        confidence REAL,
        method TEXT NOT NULL,
        metadata TEXT,
        created_at INTEGER NOT NULL,
        updated_at INTEGER NOT NULL,
        FOREIGN KEY(signal_id) REFERENCES signals(id) ON DELETE CASCADE
    );
    CREATE INDEX IF NOT EXISTS idx_geolocations_timestamp ON geolocations(timestamp);
    CREATE INDEX IF NOT EXISTS idx_geolocations_signal_id ON geolocations(signal_id);
    CREATE INDEX IF NOT EXISTS idx_geolocations_track_id ON geolocations(track_id);
)";

constexpr const char* CREATE_EVENTS_TABLE = R"(
    CREATE TABLE IF NOT EXISTS events (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        timestamp INTEGER NOT NULL,
        event_type TEXT NOT NULL,
        severity TEXT NOT NULL,
        source TEXT NOT NULL,
        description TEXT NOT NULL,
        metadata TEXT,
        created_at INTEGER NOT NULL
    );
    CREATE INDEX IF NOT EXISTS idx_events_timestamp ON events(timestamp);
    CREATE INDEX IF NOT EXISTS idx_events_type ON events(event_type);
)";

constexpr const char* CREATE_REPORTS_TABLE = R"(
    CREATE TABLE IF NOT EXISTS reports (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        timestamp INTEGER NOT NULL,
        report_type TEXT NOT NULL,
        title TEXT NOT NULL,
        description TEXT,
        parameters TEXT,
        format TEXT NOT NULL,
        file_path TEXT NOT NULL,
        created_at INTEGER NOT NULL,
        created_by TEXT NOT NULL
    );
    CREATE INDEX IF NOT EXISTS idx_reports_timestamp ON reports(timestamp);
    CREATE INDEX IF NOT EXISTS idx_reports_type ON reports(report_type);
)";

// Data structures matching the schema
struct SignalRecord {
    std::optional<int64_t> id;
    std::chrono::system_clock::time_point timestamp;
    double frequency;
    double bandwidth;
    double power;
    double snr;
    std::optional<std::string> signal_class;
    std::optional<double> confidence;
    std::string node_id;
    std::optional<std::string> track_id;
    std::optional<nlohmann::json> metadata;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
};

struct GeolocationRecord {
    std::optional<int64_t> id;
    std::chrono::system_clock::time_point timestamp;
    double latitude;
    double longitude;
    std::optional<double> altitude;
    std::optional<double> accuracy;
    int64_t signal_id;
    std::optional<std::string> track_id;
    std::optional<double> confidence;
    std::string method;
    std::optional<nlohmann::json> metadata;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point updated_at;
};

enum class EventSeverity {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

struct EventRecord {
    std::optional<int64_t> id;
    std::chrono::system_clock::time_point timestamp;
    std::string event_type;
    EventSeverity severity;
    std::string source;
    std::string description;
    std::optional<nlohmann::json> metadata;
    std::chrono::system_clock::time_point created_at;
};

struct ReportRecord {
    std::optional<int64_t> id;
    std::chrono::system_clock::time_point timestamp;
    std::string report_type;
    std::string title;
    std::optional<std::string> description;
    std::optional<nlohmann::json> parameters;
    std::string format;
    std::string file_path;
    std::chrono::system_clock::time_point created_at;
    std::string created_by;
};

// Query parameters for filtering and pagination
struct QueryParams {
    std::optional<std::chrono::system_clock::time_point> start_time;
    std::optional<std::chrono::system_clock::time_point> end_time;
    std::optional<double> min_frequency;
    std::optional<double> max_frequency;
    std::optional<double> min_power;
    std::optional<double> min_snr;
    std::optional<std::string> signal_class;
    std::optional<std::string> track_id;
    std::optional<std::string> node_id;
    std::optional<nlohmann::json> metadata_filter;
    std::optional<size_t> limit;
    std::optional<size_t> offset;
    std::optional<std::string> order_by;
    std::optional<bool> ascending;
};

// Geolocation query parameters
struct GeoQueryParams : public QueryParams {
    std::optional<double> min_latitude;
    std::optional<double> max_latitude;
    std::optional<double> min_longitude;
    std::optional<double> max_longitude;
    std::optional<double> min_confidence;
    std::optional<std::string> method;
};

// Event query parameters
struct EventQueryParams {
    std::optional<std::chrono::system_clock::time_point> start_time;
    std::optional<std::chrono::system_clock::time_point> end_time;
    std::optional<std::string> event_type;
    std::optional<EventSeverity> min_severity;
    std::optional<std::string> source;
    std::optional<std::string> description_contains;
    std::optional<size_t> limit;
    std::optional<size_t> offset;
};

// Report query parameters
struct ReportQueryParams {
    std::optional<std::chrono::system_clock::time_point> start_time;
    std::optional<std::chrono::system_clock::time_point> end_time;
    std::optional<std::string> report_type;
    std::optional<std::string> title_contains;
    std::optional<std::string> format;
    std::optional<std::string> created_by;
    std::optional<size_t> limit;
    std::optional<size_t> offset;
};

} // namespace database
} // namespace tdoa 