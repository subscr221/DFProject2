#include "signal_db_manager.h"
#include <sstream>
#include <filesystem>
#include <fstream>

namespace tdoa {
namespace database {

// Helper struct to hold prepared statements
struct SignalDBManager::PreparedStatements {
    sqlite3_stmt* insert_signal;
    sqlite3_stmt* update_signal;
    sqlite3_stmt* delete_signal;
    sqlite3_stmt* get_signal;
    
    sqlite3_stmt* insert_geolocation;
    sqlite3_stmt* update_geolocation;
    sqlite3_stmt* delete_geolocation;
    sqlite3_stmt* get_geolocation;
    
    sqlite3_stmt* insert_event;
    sqlite3_stmt* delete_event;
    sqlite3_stmt* get_event;
    
    sqlite3_stmt* insert_report;
    sqlite3_stmt* delete_report;
    sqlite3_stmt* get_report;
    
    PreparedStatements() :
        insert_signal(nullptr), update_signal(nullptr), delete_signal(nullptr), get_signal(nullptr),
        insert_geolocation(nullptr), update_geolocation(nullptr), delete_geolocation(nullptr), get_geolocation(nullptr),
        insert_event(nullptr), delete_event(nullptr), get_event(nullptr),
        insert_report(nullptr), delete_report(nullptr), get_report(nullptr) {}
    
    ~PreparedStatements() {
        sqlite3_finalize(insert_signal);
        sqlite3_finalize(update_signal);
        sqlite3_finalize(delete_signal);
        sqlite3_finalize(get_signal);
        
        sqlite3_finalize(insert_geolocation);
        sqlite3_finalize(update_geolocation);
        sqlite3_finalize(delete_geolocation);
        sqlite3_finalize(get_geolocation);
        
        sqlite3_finalize(insert_event);
        sqlite3_finalize(delete_event);
        sqlite3_finalize(get_event);
        
        sqlite3_finalize(insert_report);
        sqlite3_finalize(delete_report);
        sqlite3_finalize(get_report);
    }
};

SignalDBManager::SignalDBManager(const std::string& db_path)
    : db_(nullptr), db_path_(db_path), initialized_(false), stmts_(nullptr) {
}

SignalDBManager::~SignalDBManager() {
    if (db_) {
        stmts_.reset();
        sqlite3_close(db_);
    }
}

bool SignalDBManager::initialize() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    if (initialized_) {
        return true;
    }
    
    // Open database
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        return false;
    }
    
    // Enable foreign keys
    char* error_msg = nullptr;
    rc = sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &error_msg);
    if (rc != SQLITE_OK) {
        sqlite3_free(error_msg);
        return false;
    }
    
    // Create tables
    if (!createTables()) {
        return false;
    }
    
    // Check schema version
    if (!checkSchemaVersion()) {
        return false;
    }
    
    // Prepare statements
    stmts_ = std::make_unique<PreparedStatements>();
    if (!prepareStatements()) {
        return false;
    }
    
    initialized_ = true;
    return true;
}

bool SignalDBManager::isInitialized() const {
    return initialized_;
}

int SignalDBManager::getSchemaVersion() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    sqlite3_stmt* stmt = nullptr;
    const char* query = "SELECT value FROM metadata WHERE key = 'schema_version';";
    
    int version = 0;
    int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            version = std::stoi(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        }
    }
    
    sqlite3_finalize(stmt);
    return version;
}

bool SignalDBManager::createTables() {
    char* error_msg = nullptr;
    
    // Create tables
    const char* tables[] = {
        CREATE_METADATA_TABLE,
        CREATE_SIGNALS_TABLE,
        CREATE_GEOLOCATIONS_TABLE,
        CREATE_EVENTS_TABLE,
        CREATE_REPORTS_TABLE
    };
    
    for (const char* sql : tables) {
        int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &error_msg);
        if (rc != SQLITE_OK) {
            sqlite3_free(error_msg);
            return false;
        }
    }
    
    // Insert schema version if not exists
    const char* insert_version = R"(
        INSERT OR IGNORE INTO metadata (key, value) VALUES ('schema_version', ?);
    )";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, insert_version, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, std::to_string(SCHEMA_VERSION).c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return rc == SQLITE_DONE;
}

bool SignalDBManager::prepareStatements() {
    const char* sql_insert_signal = R"(
        INSERT INTO signals (
            timestamp, frequency, bandwidth, power, snr, signal_class,
            confidence, node_id, track_id, metadata, created_at, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";
    
    const char* sql_update_signal = R"(
        UPDATE signals SET
            timestamp = ?, frequency = ?, bandwidth = ?, power = ?, snr = ?,
            signal_class = ?, confidence = ?, node_id = ?, track_id = ?,
            metadata = ?, updated_at = ?
        WHERE id = ?;
    )";
    
    const char* sql_delete_signal = "DELETE FROM signals WHERE id = ?;";
    const char* sql_get_signal = "SELECT * FROM signals WHERE id = ?;";
    
    // Prepare signal statements
    int rc = sqlite3_prepare_v2(db_, sql_insert_signal, -1, &stmts_->insert_signal, nullptr);
    if (rc != SQLITE_OK) return false;
    
    rc = sqlite3_prepare_v2(db_, sql_update_signal, -1, &stmts_->update_signal, nullptr);
    if (rc != SQLITE_OK) return false;
    
    rc = sqlite3_prepare_v2(db_, sql_delete_signal, -1, &stmts_->delete_signal, nullptr);
    if (rc != SQLITE_OK) return false;
    
    rc = sqlite3_prepare_v2(db_, sql_get_signal, -1, &stmts_->get_signal, nullptr);
    if (rc != SQLITE_OK) return false;
    
    // Similar statements for geolocations, events, and reports...
    // (Implementation omitted for brevity but follows the same pattern)
    
    return true;
}

void SignalDBManager::finalizeStatements() {
    stmts_.reset();
}

int64_t SignalDBManager::insertSignal(const SignalRecord& signal) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    sqlite3_reset(stmts_->insert_signal);
    sqlite3_clear_bindings(stmts_->insert_signal);
    
    // Bind parameters
    sqlite3_bind_int64(stmts_->insert_signal, 1, timestampToInt(signal.timestamp));
    sqlite3_bind_double(stmts_->insert_signal, 2, signal.frequency);
    sqlite3_bind_double(stmts_->insert_signal, 3, signal.bandwidth);
    sqlite3_bind_double(stmts_->insert_signal, 4, signal.power);
    sqlite3_bind_double(stmts_->insert_signal, 5, signal.snr);
    
    if (signal.signal_class) {
        sqlite3_bind_text(stmts_->insert_signal, 6, signal.signal_class->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmts_->insert_signal, 6);
    }
    
    if (signal.confidence) {
        sqlite3_bind_double(stmts_->insert_signal, 7, *signal.confidence);
    } else {
        sqlite3_bind_null(stmts_->insert_signal, 7);
    }
    
    sqlite3_bind_text(stmts_->insert_signal, 8, signal.node_id.c_str(), -1, SQLITE_TRANSIENT);
    
    if (signal.track_id) {
        sqlite3_bind_text(stmts_->insert_signal, 9, signal.track_id->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmts_->insert_signal, 9);
    }
    
    if (signal.metadata) {
        sqlite3_bind_text(stmts_->insert_signal, 10, serializeJson(*signal.metadata).c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmts_->insert_signal, 10);
    }
    
    auto now = std::chrono::system_clock::now();
    sqlite3_bind_int64(stmts_->insert_signal, 11, timestampToInt(now));
    sqlite3_bind_int64(stmts_->insert_signal, 12, timestampToInt(now));
    
    int rc = sqlite3_step(stmts_->insert_signal);
    if (rc != SQLITE_DONE) {
        return -1;
    }
    
    return sqlite3_last_insert_rowid(db_);
}

bool SignalDBManager::updateSignal(const SignalRecord& signal) {
    if (!signal.id) {
        return false;
    }
    
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    sqlite3_reset(stmts_->update_signal);
    sqlite3_clear_bindings(stmts_->update_signal);
    
    // Bind parameters (similar to insert)
    sqlite3_bind_int64(stmts_->update_signal, 1, timestampToInt(signal.timestamp));
    sqlite3_bind_double(stmts_->update_signal, 2, signal.frequency);
    sqlite3_bind_double(stmts_->update_signal, 3, signal.bandwidth);
    sqlite3_bind_double(stmts_->update_signal, 4, signal.power);
    sqlite3_bind_double(stmts_->update_signal, 5, signal.snr);
    
    if (signal.signal_class) {
        sqlite3_bind_text(stmts_->update_signal, 6, signal.signal_class->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmts_->update_signal, 6);
    }
    
    if (signal.confidence) {
        sqlite3_bind_double(stmts_->update_signal, 7, *signal.confidence);
    } else {
        sqlite3_bind_null(stmts_->update_signal, 7);
    }
    
    sqlite3_bind_text(stmts_->update_signal, 8, signal.node_id.c_str(), -1, SQLITE_TRANSIENT);
    
    if (signal.track_id) {
        sqlite3_bind_text(stmts_->update_signal, 9, signal.track_id->c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmts_->update_signal, 9);
    }
    
    if (signal.metadata) {
        sqlite3_bind_text(stmts_->update_signal, 10, serializeJson(*signal.metadata).c_str(), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmts_->update_signal, 10);
    }
    
    sqlite3_bind_int64(stmts_->update_signal, 11, timestampToInt(std::chrono::system_clock::now()));
    sqlite3_bind_int64(stmts_->update_signal, 12, *signal.id);
    
    int rc = sqlite3_step(stmts_->update_signal);
    return rc == SQLITE_DONE;
}

bool SignalDBManager::deleteSignal(int64_t id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    sqlite3_reset(stmts_->delete_signal);
    sqlite3_clear_bindings(stmts_->delete_signal);
    
    sqlite3_bind_int64(stmts_->delete_signal, 1, id);
    
    int rc = sqlite3_step(stmts_->delete_signal);
    return rc == SQLITE_DONE;
}

std::optional<SignalRecord> SignalDBManager::getSignal(int64_t id) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    sqlite3_reset(stmts_->get_signal);
    sqlite3_clear_bindings(stmts_->get_signal);
    
    sqlite3_bind_int64(stmts_->get_signal, 1, id);
    
    int rc = sqlite3_step(stmts_->get_signal);
    if (rc != SQLITE_ROW) {
        return std::nullopt;
    }
    
    return rowToSignalRecord(stmts_->get_signal);
}

std::vector<SignalRecord> SignalDBManager::querySignals(const QueryParams& params) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    std::string query = buildSignalQuery(params);
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(db_, query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return {};
    }
    
    std::vector<SignalRecord> results;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(rowToSignalRecord(stmt));
    }
    
    sqlite3_finalize(stmt);
    return results;
}

size_t SignalDBManager::countSignals(const QueryParams& params) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    std::string base_query = buildSignalQuery(params);
    std::string count_query = "SELECT COUNT(*) FROM (" + base_query + ")";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, count_query.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return 0;
    }
    
    size_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }
    
    sqlite3_finalize(stmt);
    return count;
}

std::string SignalDBManager::buildSignalQuery(const QueryParams& params) const {
    std::stringstream ss;
    ss << "SELECT * FROM signals WHERE 1=1";
    
    if (params.start_time) {
        ss << " AND timestamp >= " << timestampToInt(*params.start_time);
    }
    if (params.end_time) {
        ss << " AND timestamp <= " << timestampToInt(*params.end_time);
    }
    if (params.min_frequency) {
        ss << " AND frequency >= " << *params.min_frequency;
    }
    if (params.max_frequency) {
        ss << " AND frequency <= " << *params.max_frequency;
    }
    if (params.min_power) {
        ss << " AND power >= " << *params.min_power;
    }
    if (params.min_snr) {
        ss << " AND snr >= " << *params.min_snr;
    }
    if (params.signal_class) {
        ss << " AND signal_class = '" << *params.signal_class << "'";
    }
    if (params.track_id) {
        ss << " AND track_id = '" << *params.track_id << "'";
    }
    if (params.node_id) {
        ss << " AND node_id = '" << *params.node_id << "'";
    }
    
    if (params.order_by) {
        ss << " ORDER BY " << *params.order_by;
        if (params.ascending.value_or(true)) {
            ss << " ASC";
        } else {
            ss << " DESC";
        }
    }
    
    if (params.limit) {
        ss << " LIMIT " << *params.limit;
        if (params.offset) {
            ss << " OFFSET " << *params.offset;
        }
    }
    
    return ss.str();
}

SignalRecord SignalDBManager::rowToSignalRecord(sqlite3_stmt* stmt) const {
    SignalRecord record;
    
    record.id = sqlite3_column_int64(stmt, 0);
    record.timestamp = intToTimestamp(sqlite3_column_int64(stmt, 1));
    record.frequency = sqlite3_column_double(stmt, 2);
    record.bandwidth = sqlite3_column_double(stmt, 3);
    record.power = sqlite3_column_double(stmt, 4);
    record.snr = sqlite3_column_double(stmt, 5);
    
    if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
        record.signal_class = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    }
    
    if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) {
        record.confidence = sqlite3_column_double(stmt, 7);
    }
    
    record.node_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
    
    if (sqlite3_column_type(stmt, 9) != SQLITE_NULL) {
        record.track_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
    }
    
    if (sqlite3_column_type(stmt, 10) != SQLITE_NULL) {
        record.metadata = deserializeJson(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10)));
    }
    
    record.created_at = intToTimestamp(sqlite3_column_int64(stmt, 11));
    record.updated_at = intToTimestamp(sqlite3_column_int64(stmt, 12));
    
    return record;
}

int64_t SignalDBManager::timestampToInt(const std::chrono::system_clock::time_point& tp) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

std::chrono::system_clock::time_point SignalDBManager::intToTimestamp(int64_t timestamp) {
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(timestamp));
}

std::string SignalDBManager::serializeJson(const nlohmann::json& j) {
    return j.dump();
}

nlohmann::json SignalDBManager::deserializeJson(const std::string& s) {
    return nlohmann::json::parse(s);
}

std::string SignalDBManager::severityToString(EventSeverity severity) {
    switch (severity) {
        case EventSeverity::DEBUG: return "DEBUG";
        case EventSeverity::INFO: return "INFO";
        case EventSeverity::WARNING: return "WARNING";
        case EventSeverity::ERROR: return "ERROR";
        case EventSeverity::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

EventSeverity SignalDBManager::stringToSeverity(const std::string& severity) {
    if (severity == "DEBUG") return EventSeverity::DEBUG;
    if (severity == "INFO") return EventSeverity::INFO;
    if (severity == "WARNING") return EventSeverity::WARNING;
    if (severity == "ERROR") return EventSeverity::ERROR;
    if (severity == "CRITICAL") return EventSeverity::CRITICAL;
    return EventSeverity::INFO;
}

bool SignalDBManager::beginTransaction() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool SignalDBManager::commitTransaction() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool SignalDBManager::rollbackTransaction() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr) == SQLITE_OK;
}

SignalDBManager::DBStats SignalDBManager::getStats() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    DBStats stats{};
    
    // Get total counts
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM signals;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats.total_signals = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
        }
    }
    sqlite3_finalize(stmt);
    
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM geolocations;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats.total_geolocations = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
        }
    }
    sqlite3_finalize(stmt);
    
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM events;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats.total_events = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
        }
    }
    sqlite3_finalize(stmt);
    
    if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM reports;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats.total_reports = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
        }
    }
    sqlite3_finalize(stmt);
    
    // Get database file size
    try {
        stats.db_size_bytes = std::filesystem::file_size(db_path_);
    } catch (...) {
        stats.db_size_bytes = 0;
    }
    
    // Get oldest and newest records
    if (sqlite3_prepare_v2(db_, "SELECT MIN(timestamp), MAX(timestamp) FROM signals;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
                stats.oldest_record = intToTimestamp(sqlite3_column_int64(stmt, 0));
            }
            if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
                stats.newest_record = intToTimestamp(sqlite3_column_int64(stmt, 1));
            }
        }
    }
    sqlite3_finalize(stmt);
    
    return stats;
}

} // namespace database
} // namespace tdoa 