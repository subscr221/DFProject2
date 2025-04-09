#include "../signal_db_manager.h"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <iomanip>

using namespace tdoa::database;

// Helper function to print a signal record
void printSignalRecord(const SignalRecord& signal) {
    std::cout << "Signal ID: " << (signal.id ? std::to_string(*signal.id) : "N/A") << std::endl;
    std::cout << "  Timestamp: " << std::chrono::system_clock::to_time_t(signal.timestamp) << std::endl;
    std::cout << "  Frequency: " << std::fixed << std::setprecision(3) << signal.frequency << " MHz" << std::endl;
    std::cout << "  Bandwidth: " << signal.bandwidth << " kHz" << std::endl;
    std::cout << "  Power: " << signal.power << " dBm" << std::endl;
    std::cout << "  SNR: " << signal.snr << " dB" << std::endl;
    std::cout << "  Class: " << (signal.signal_class ? *signal.signal_class : "Unknown") << std::endl;
    std::cout << "  Confidence: " << (signal.confidence ? std::to_string(*signal.confidence) : "N/A") << std::endl;
    std::cout << "  Node ID: " << signal.node_id << std::endl;
    std::cout << "  Track ID: " << (signal.track_id ? *signal.track_id : "N/A") << std::endl;
    if (signal.metadata) {
        std::cout << "  Metadata: " << signal.metadata->dump(2) << std::endl;
    }
    std::cout << std::endl;
}

// Helper function to print database stats
void printDBStats(const SignalDBManager::DBStats& stats) {
    std::cout << "Database Statistics:" << std::endl;
    std::cout << "  Total Signals: " << stats.total_signals << std::endl;
    std::cout << "  Total Geolocations: " << stats.total_geolocations << std::endl;
    std::cout << "  Total Events: " << stats.total_events << std::endl;
    std::cout << "  Total Reports: " << stats.total_reports << std::endl;
    std::cout << "  Database Size: " << stats.db_size_bytes << " bytes" << std::endl;
    if (stats.total_signals > 0) {
        std::cout << "  Oldest Record: " << std::chrono::system_clock::to_time_t(stats.oldest_record) << std::endl;
        std::cout << "  Newest Record: " << std::chrono::system_clock::to_time_t(stats.newest_record) << std::endl;
    }
    std::cout << std::endl;
}

int main() {
    try {
        std::cout << "Starting Signal Database Tests..." << std::endl;
        std::cout << "===============================" << std::endl << std::endl;

        // Create test database file
        const std::string test_db_path = "test_signals.db";
        if (std::filesystem::exists(test_db_path)) {
            std::filesystem::remove(test_db_path);
        }

        // Create and initialize database manager
        SignalDBManager db_manager(test_db_path);
        assert(db_manager.initialize());
        assert(db_manager.isInitialized());
        assert(db_manager.getSchemaVersion() == SCHEMA_VERSION);

        std::cout << "Test 1: Signal Insertion" << std::endl;
        std::cout << "----------------------" << std::endl;

        // Create test signal
        SignalRecord signal1;
        signal1.timestamp = std::chrono::system_clock::now();
        signal1.frequency = 145.500;
        signal1.bandwidth = 12.5;
        signal1.power = -85.2;
        signal1.snr = 15.8;
        signal1.signal_class = "FM";
        signal1.confidence = 0.95;
        signal1.node_id = "node001";
        signal1.track_id = "track001";
        signal1.metadata = nlohmann::json{
            {"modulation", "NBFM"},
            {"channel", "Amateur Radio"},
            {"notes", "Clear signal"}
        };

        int64_t signal_id = db_manager.insertSignal(signal1);
        assert(signal_id > 0);
        std::cout << "Inserted signal with ID: " << signal_id << std::endl << std::endl;

        std::cout << "Test 2: Signal Retrieval" << std::endl;
        std::cout << "----------------------" << std::endl;

        auto retrieved_signal = db_manager.getSignal(signal_id);
        assert(retrieved_signal);
        printSignalRecord(*retrieved_signal);

        std::cout << "Test 3: Signal Update" << std::endl;
        std::cout << "-------------------" << std::endl;

        retrieved_signal->power = -82.5;
        retrieved_signal->snr = 18.2;
        retrieved_signal->confidence = 0.98;
        assert(db_manager.updateSignal(*retrieved_signal));

        auto updated_signal = db_manager.getSignal(signal_id);
        assert(updated_signal);
        printSignalRecord(*updated_signal);

        std::cout << "Test 4: Multiple Signal Insertion" << std::endl;
        std::cout << "------------------------------" << std::endl;

        // Insert more test signals
        for (int i = 0; i < 5; ++i) {
            SignalRecord signal;
            signal.timestamp = std::chrono::system_clock::now() + std::chrono::seconds(i);
            signal.frequency = 146.000 + (i * 0.025);
            signal.bandwidth = 12.5;
            signal.power = -90.0 + (i * 2);
            signal.snr = 12.0 + i;
            signal.signal_class = "FM";
            signal.confidence = 0.85 + (i * 0.02);
            signal.node_id = "node001";
            signal.track_id = "track002";

            int64_t id = db_manager.insertSignal(signal);
            assert(id > 0);
            std::cout << "Inserted signal with ID: " << id << std::endl;
        }
        std::cout << std::endl;

        std::cout << "Test 5: Signal Query" << std::endl;
        std::cout << "-------------------" << std::endl;

        QueryParams params;
        params.min_frequency = 146.000;
        params.max_frequency = 146.100;
        params.min_power = -88.0;
        params.min_snr = 13.0;
        params.track_id = "track002";
        params.order_by = "frequency";
        params.ascending = true;

        auto query_results = db_manager.querySignals(params);
        std::cout << "Found " << query_results.size() << " signals matching criteria:" << std::endl;
        for (const auto& signal : query_results) {
            printSignalRecord(signal);
        }

        std::cout << "Test 6: Database Statistics" << std::endl;
        std::cout << "------------------------" << std::endl;

        auto stats = db_manager.getStats();
        printDBStats(stats);

        std::cout << "Test 7: Signal Deletion" << std::endl;
        std::cout << "---------------------" << std::endl;

        assert(db_manager.deleteSignal(signal_id));
        assert(!db_manager.getSignal(signal_id));
        std::cout << "Successfully deleted signal with ID: " << signal_id << std::endl;

        stats = db_manager.getStats();
        printDBStats(stats);

        std::cout << "All tests completed successfully!" << std::endl;

        // Cleanup
        std::filesystem::remove(test_db_path);
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 