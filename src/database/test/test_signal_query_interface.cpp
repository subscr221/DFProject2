#include "../signal_query_interface.h"
#include "../signal_db_manager.h"
#include <iostream>
#include <iomanip>
#include <cassert>
#include <chrono>

using namespace tdoa::database;
using namespace std::chrono;

// Helper function to print signal records
void printSignalRecord(const SignalRecord& record) {
    std::cout << "Signal ID: " << record.id << "\n"
              << "  Timestamp: " << system_clock::to_time_t(record.timestamp) << "\n"
              << "  Frequency: " << record.frequency << " MHz\n"
              << "  Power: " << record.power << " dBm\n"
              << "  SNR: " << record.snr << " dB\n"
              << "  Node ID: " << record.node_id << "\n";
    if (record.track_id) {
        std::cout << "  Track ID: " << *record.track_id << "\n";
    }
    if (record.signal_class) {
        std::cout << "  Class: " << *record.signal_class << "\n";
    }
    std::cout << std::endl;
}

// Helper function to print geolocation records
void printGeolocationRecord(const GeolocationRecord& record) {
    std::cout << "Geolocation ID: " << record.id << "\n"
              << "  Timestamp: " << system_clock::to_time_t(record.timestamp) << "\n"
              << "  Latitude: " << record.latitude << "°\n"
              << "  Longitude: " << record.longitude << "°\n"
              << "  Method: " << record.method << "\n";
    if (record.confidence) {
        std::cout << "  Confidence: " << *record.confidence << "%\n";
    }
    if (record.track_id) {
        std::cout << "  Track ID: " << *record.track_id << "\n";
    }
    std::cout << std::endl;
}

// Helper function to print signal statistics
void printSignalStats(const SignalQueryInterface::SignalStats& stats) {
    std::cout << "Signal Statistics:\n"
              << "  Total Signals: " << stats.total_signals << "\n"
              << "  Frequency Range: " << stats.min_frequency << " - " << stats.max_frequency << " MHz\n"
              << "  Average Power: " << stats.avg_power << " dBm\n"
              << "  Average SNR: " << stats.avg_snr << " dB\n\n"
              << "Signals by Class:\n";
    for (const auto& [class_name, count] : stats.signals_by_class) {
        std::cout << "  " << class_name << ": " << count << "\n";
    }
    std::cout << "\nSignals by Node:\n";
    for (const auto& [node_id, count] : stats.signals_by_node) {
        std::cout << "  " << node_id << ": " << count << "\n";
    }
    std::cout << std::endl;
}

// Helper function to print frequency density
void printFrequencyDensity(const std::vector<SignalQueryInterface::SignalDensity>& density) {
    std::cout << "Frequency Density Analysis:\n"
              << "Frequency (MHz) | Count | Avg Power (dBm) | Avg SNR (dB)\n"
              << "----------------+-------+----------------+-------------\n";
    for (const auto& bin : density) {
        std::cout << std::fixed << std::setprecision(2)
                  << std::setw(14) << bin.frequency << " | "
                  << std::setw(5) << bin.signal_count << " | "
                  << std::setw(14) << bin.avg_power << " | "
                  << std::setw(11) << bin.avg_snr << "\n";
    }
    std::cout << std::endl;
}

int main() {
    // Create a database manager instance
    auto db_manager = std::make_shared<SignalDBManager>("test_signals.db");
    
    // Create the query interface
    SignalQueryInterface query_interface(db_manager);

    // Test 1: Basic signal search
    std::cout << "Test 1: Basic Signal Search\n";
    {
        TimeRange time_range{
            system_clock::now() - hours(24),
            system_clock::now()
        };
        FrequencyRange freq_range{100.0, 200.0};
        PaginationParams pagination{0, 10, "timestamp", true};

        auto result = query_interface.searchSignals(
            time_range,
            freq_range,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            pagination
        );

        std::cout << "Found " << result.total_count << " signals\n";
        for (const auto& signal : result.items) {
            printSignalRecord(signal);
        }
    }

    // Test 2: Geolocation search with region
    std::cout << "\nTest 2: Geolocation Search\n";
    {
        GeoRegion region{30.0, 45.0, -100.0, -80.0};
        PaginationParams pagination{0, 10, "timestamp", true};

        auto result = query_interface.searchGeolocations(
            std::nullopt,
            region,
            std::nullopt,
            std::nullopt,
            0.8,
            pagination
        );

        std::cout << "Found " << result.total_count << " geolocations\n";
        for (const auto& location : result.items) {
            printGeolocationRecord(location);
        }
    }

    // Test 3: Track history
    std::cout << "\nTest 3: Track History\n";
    {
        std::string track_id = "TRACK001";
        TimeRange time_range{
            system_clock::now() - hours(12),
            system_clock::now()
        };

        auto track_signals = query_interface.getTrackHistory(track_id, time_range);
        std::cout << "Found " << track_signals.size() << " signals for track " << track_id << "\n";
        for (const auto& signal : track_signals) {
            printSignalRecord(signal);
        }
    }

    // Test 4: Signal statistics
    std::cout << "\nTest 4: Signal Statistics\n";
    {
        TimeRange time_range{
            system_clock::now() - hours(24),
            system_clock::now()
        };

        auto stats = query_interface.getSignalStatistics(time_range, std::nullopt);
        printSignalStats(stats);
    }

    // Test 5: Frequency density analysis
    std::cout << "\nTest 5: Frequency Density Analysis\n";
    {
        FrequencyRange range{100.0, 200.0};
        double bin_size = 10.0;
        TimeRange time_range{
            system_clock::now() - hours(24),
            system_clock::now()
        };

        auto density = query_interface.getFrequencyDensity(range, bin_size, time_range);
        printFrequencyDensity(density);
    }

    // Test 6: Related tracks
    std::cout << "\nTest 6: Related Tracks\n";
    {
        std::string track_id = "TRACK001";
        auto related_tracks = query_interface.findRelatedTracks(track_id, 1.0, 300.0);
        
        std::cout << "Found " << related_tracks.size() << " related tracks for " << track_id << ":\n";
        for (const auto& related_id : related_tracks) {
            std::cout << "  " << related_id << "\n";
        }
        std::cout << std::endl;
    }

    // Test 7: Event search
    std::cout << "\nTest 7: Event Search\n";
    {
        TimeRange time_range{
            system_clock::now() - hours(24),
            system_clock::now()
        };
        PaginationParams pagination{0, 10, "timestamp", true};

        auto result = query_interface.searchEvents(
            time_range,
            "INTERFERENCE",
            EventSeverity::WARNING,
            std::nullopt,
            pagination
        );

        std::cout << "Found " << result.total_count << " events\n";
        for (const auto& event : result.items) {
            std::cout << "Event ID: " << event.id << "\n"
                     << "  Type: " << event.event_type << "\n"
                     << "  Severity: " << static_cast<int>(event.severity) << "\n"
                     << "  Timestamp: " << system_clock::to_time_t(event.timestamp) << "\n"
                     << "  Description: " << event.description << "\n\n";
        }
    }

    // Test 8: Report search
    std::cout << "\nTest 8: Report Search\n";
    {
        TimeRange time_range{
            system_clock::now() - hours(24),
            system_clock::now()
        };
        PaginationParams pagination{0, 10, "timestamp", true};

        auto result = query_interface.searchReports(
            time_range,
            "DAILY_SUMMARY",
            "system",
            pagination
        );

        std::cout << "Found " << result.total_count << " reports\n";
        for (const auto& report : result.items) {
            std::cout << "Report ID: " << report.id << "\n"
                     << "  Type: " << report.report_type << "\n"
                     << "  Created By: " << report.created_by << "\n"
                     << "  Timestamp: " << system_clock::to_time_t(report.timestamp) << "\n"
                     << "  Title: " << report.title << "\n\n";
        }
    }

    std::cout << "All tests completed successfully!\n";
    return 0;
} 