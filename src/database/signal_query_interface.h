#pragma once

#include "signal_db_manager.h"
#include <memory>
#include <vector>
#include <optional>
#include <functional>

namespace tdoa {
namespace database {

// Forward declarations
class SignalDBManager;

// Time range for queries
struct TimeRange {
    std::chrono::system_clock::time_point start;
    std::chrono::system_clock::time_point end;
};

// Frequency range for queries
struct FrequencyRange {
    double min_frequency;
    double max_frequency;
};

// Geographic region for queries
struct GeoRegion {
    double min_latitude;
    double max_latitude;
    double min_longitude;
    double max_longitude;
    std::optional<double> min_altitude;
    std::optional<double> max_altitude;
};

// Signal characteristics for filtering
struct SignalCharacteristics {
    std::optional<double> min_power;
    std::optional<double> max_power;
    std::optional<double> min_snr;
    std::optional<double> max_snr;
    std::optional<double> min_bandwidth;
    std::optional<double> max_bandwidth;
    std::optional<std::string> signal_class;
    std::optional<double> min_confidence;
};

// Pagination parameters
struct PaginationParams {
    size_t page_size = 100;
    size_t page_number = 0;
    std::string sort_by = "timestamp";
    bool ascending = true;
};

// Search result with pagination info
template<typename T>
struct SearchResult {
    std::vector<T> items;
    size_t total_count;
    size_t page_count;
    size_t current_page;
    bool has_next_page;
    bool has_previous_page;
};

class SignalQueryInterface {
public:
    explicit SignalQueryInterface(std::shared_ptr<SignalDBManager> db_manager);
    ~SignalQueryInterface() = default;

    // Signal search methods
    SearchResult<SignalRecord> searchSignals(
        const std::optional<TimeRange>& time_range = std::nullopt,
        const std::optional<FrequencyRange>& freq_range = std::nullopt,
        const std::optional<SignalCharacteristics>& characteristics = std::nullopt,
        const std::optional<std::string>& node_id = std::nullopt,
        const std::optional<std::string>& track_id = std::nullopt,
        const PaginationParams& pagination = PaginationParams()
    );

    // Geolocation search methods
    SearchResult<GeolocationRecord> searchGeolocations(
        const std::optional<TimeRange>& time_range = std::nullopt,
        const std::optional<GeoRegion>& region = std::nullopt,
        const std::optional<std::string>& track_id = std::nullopt,
        const std::optional<std::string>& method = std::nullopt,
        const std::optional<double>& min_confidence = std::nullopt,
        const PaginationParams& pagination = PaginationParams()
    );

    // Track-based queries
    std::vector<SignalRecord> getTrackHistory(
        const std::string& track_id,
        const std::optional<TimeRange>& time_range = std::nullopt
    );

    std::vector<GeolocationRecord> getTrackPath(
        const std::string& track_id,
        const std::optional<TimeRange>& time_range = std::nullopt
    );

    // Statistical queries
    struct SignalStats {
        size_t total_signals;
        double avg_power;
        double avg_snr;
        double min_frequency;
        double max_frequency;
        std::map<std::string, size_t> signals_by_class;
        std::map<std::string, size_t> signals_by_node;
    };

    SignalStats getSignalStatistics(
        const std::optional<TimeRange>& time_range = std::nullopt,
        const std::optional<std::string>& node_id = std::nullopt
    );

    struct GeoStats {
        size_t total_locations;
        double avg_confidence;
        GeoRegion coverage_area;
        std::map<std::string, size_t> locations_by_method;
    };

    GeoStats getGeolocationStatistics(
        const std::optional<TimeRange>& time_range = std::nullopt,
        const std::optional<std::string>& method = std::nullopt
    );

    // Advanced queries
    std::vector<std::string> findRelatedTracks(
        const std::string& track_id,
        double frequency_tolerance,
        double time_tolerance_seconds
    );

    struct SignalDensity {
        double frequency;
        size_t signal_count;
        double avg_power;
        double avg_snr;
    };

    std::vector<SignalDensity> getFrequencyDensity(
        const FrequencyRange& range,
        double bin_size,
        const std::optional<TimeRange>& time_range = std::nullopt
    );

    // Event queries
    SearchResult<EventRecord> searchEvents(
        const std::optional<TimeRange>& time_range = std::nullopt,
        const std::optional<std::string>& event_type = std::nullopt,
        const std::optional<EventSeverity>& min_severity = std::nullopt,
        const std::optional<std::string>& source = std::nullopt,
        const PaginationParams& pagination = PaginationParams()
    );

    // Report queries
    SearchResult<ReportRecord> searchReports(
        const std::optional<TimeRange>& time_range = std::nullopt,
        const std::optional<std::string>& report_type = std::nullopt,
        const std::optional<std::string>& created_by = std::nullopt,
        const PaginationParams& pagination = PaginationParams()
    );

private:
    std::shared_ptr<SignalDBManager> db_manager_;

    // Helper methods
    QueryParams buildSignalQueryParams(
        const std::optional<TimeRange>& time_range,
        const std::optional<FrequencyRange>& freq_range,
        const std::optional<SignalCharacteristics>& characteristics,
        const std::optional<std::string>& node_id,
        const std::optional<std::string>& track_id,
        const PaginationParams& pagination
    ) const;

    GeoQueryParams buildGeoQueryParams(
        const std::optional<TimeRange>& time_range,
        const std::optional<GeoRegion>& region,
        const std::optional<std::string>& track_id,
        const std::optional<std::string>& method,
        const std::optional<double>& min_confidence,
        const PaginationParams& pagination
    ) const;

    EventQueryParams buildEventQueryParams(
        const std::optional<TimeRange>& time_range,
        const std::optional<std::string>& event_type,
        const std::optional<EventSeverity>& min_severity,
        const std::optional<std::string>& source,
        const PaginationParams& pagination
    ) const;

    ReportQueryParams buildReportQueryParams(
        const std::optional<TimeRange>& time_range,
        const std::optional<std::string>& report_type,
        const std::optional<std::string>& created_by,
        const PaginationParams& pagination
    ) const;

    template<typename T>
    SearchResult<T> buildSearchResult(
        const std::vector<T>& items,
        size_t total_count,
        const PaginationParams& pagination
    ) const;
};

} // namespace database
} // namespace tdoa 