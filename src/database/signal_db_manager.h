#pragma once

#include "signal_db_schema.h"
#include <sqlite3.h>
#include <memory>
#include <mutex>
#include <functional>

namespace tdoa {
namespace database {

class SignalDBManager {
public:
    // Constructor and destructor
    explicit SignalDBManager(const std::string& db_path);
    ~SignalDBManager();

    // Initialization
    bool initialize();
    bool isInitialized() const;
    int getSchemaVersion() const;

    // Signal operations
    int64_t insertSignal(const SignalRecord& signal);
    bool updateSignal(const SignalRecord& signal);
    bool deleteSignal(int64_t id);
    std::optional<SignalRecord> getSignal(int64_t id) const;
    std::vector<SignalRecord> querySignals(const QueryParams& params) const;
    size_t countSignals(const QueryParams& params) const;

    // Geolocation operations
    int64_t insertGeolocation(const GeolocationRecord& geolocation);
    bool updateGeolocation(const GeolocationRecord& geolocation);
    bool deleteGeolocation(int64_t id);
    std::optional<GeolocationRecord> getGeolocation(int64_t id) const;
    std::vector<GeolocationRecord> queryGeolocations(const GeoQueryParams& params) const;
    size_t countGeolocations(const GeoQueryParams& params) const;

    // Event operations
    int64_t insertEvent(const EventRecord& event);
    bool deleteEvent(int64_t id);
    std::optional<EventRecord> getEvent(int64_t id) const;
    std::vector<EventRecord> queryEvents(const EventQueryParams& params) const;
    size_t countEvents(const EventQueryParams& params) const;

    // Report operations
    int64_t insertReport(const ReportRecord& report);
    bool deleteReport(int64_t id);
    std::optional<ReportRecord> getReport(int64_t id) const;
    std::vector<ReportRecord> queryReports(const ReportQueryParams& params) const;
    size_t countReports(const ReportQueryParams& params) const;

    // Track operations
    std::vector<SignalRecord> getTrackSignals(const std::string& track_id) const;
    std::vector<GeolocationRecord> getTrackGeolocations(const std::string& track_id) const;
    bool deleteTrack(const std::string& track_id);

    // Maintenance operations
    bool vacuum();
    bool backup(const std::string& backup_path) const;
    bool restore(const std::string& backup_path);
    bool purgeOldRecords(const std::chrono::system_clock::time_point& older_than);

    // Transaction management
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

    // Statistics
    struct DBStats {
        size_t total_signals;
        size_t total_geolocations;
        size_t total_events;
        size_t total_reports;
        size_t db_size_bytes;
        std::chrono::system_clock::time_point oldest_record;
        std::chrono::system_clock::time_point newest_record;
    };
    DBStats getStats() const;

private:
    // SQLite database handle
    sqlite3* db_;
    std::string db_path_;
    bool initialized_;
    mutable std::recursive_mutex mutex_;

    // Statement cache
    struct PreparedStatements;
    std::unique_ptr<PreparedStatements> stmts_;

    // Helper functions
    bool createTables();
    bool prepareStatements();
    void finalizeStatements();
    bool checkSchemaVersion();
    bool upgradeSchema(int from_version, int to_version);

    // Query builders
    std::string buildSignalQuery(const QueryParams& params) const;
    std::string buildGeolocationQuery(const GeoQueryParams& params) const;
    std::string buildEventQuery(const EventQueryParams& params) const;
    std::string buildReportQuery(const ReportQueryParams& params) const;

    // Record converters
    SignalRecord rowToSignalRecord(sqlite3_stmt* stmt) const;
    GeolocationRecord rowToGeolocationRecord(sqlite3_stmt* stmt) const;
    EventRecord rowToEventRecord(sqlite3_stmt* stmt) const;
    ReportRecord rowToReportRecord(sqlite3_stmt* stmt) const;

    // Utility functions
    static int64_t timestampToInt(const std::chrono::system_clock::time_point& tp);
    static std::chrono::system_clock::time_point intToTimestamp(int64_t timestamp);
    static std::string serializeJson(const nlohmann::json& j);
    static nlohmann::json deserializeJson(const std::string& s);
    static std::string severityToString(EventSeverity severity);
    static EventSeverity stringToSeverity(const std::string& severity);

    // Prevent copying
    SignalDBManager(const SignalDBManager&) = delete;
    SignalDBManager& operator=(const SignalDBManager&) = delete;
};

} // namespace database
} // namespace tdoa 