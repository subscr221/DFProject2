#include "signal_query_interface.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <numeric>

namespace tdoa {
namespace database {

SignalQueryInterface::SignalQueryInterface(std::shared_ptr<SignalDBManager> db_manager)
    : db_manager_(std::move(db_manager)) {
}

SearchResult<SignalRecord> SignalQueryInterface::searchSignals(
    const std::optional<TimeRange>& time_range,
    const std::optional<FrequencyRange>& freq_range,
    const std::optional<SignalCharacteristics>& characteristics,
    const std::optional<std::string>& node_id,
    const std::optional<std::string>& track_id,
    const PaginationParams& pagination
) {
    QueryParams params = buildSignalQueryParams(
        time_range, freq_range, characteristics,
        node_id, track_id, pagination
    );

    size_t total_count = db_manager_->countSignals(params);
    std::vector<SignalRecord> signals = db_manager_->querySignals(params);

    return buildSearchResult(signals, total_count, pagination);
}

SearchResult<GeolocationRecord> SignalQueryInterface::searchGeolocations(
    const std::optional<TimeRange>& time_range,
    const std::optional<GeoRegion>& region,
    const std::optional<std::string>& track_id,
    const std::optional<std::string>& method,
    const std::optional<double>& min_confidence,
    const PaginationParams& pagination
) {
    GeoQueryParams params = buildGeoQueryParams(
        time_range, region, track_id, method,
        min_confidence, pagination
    );

    size_t total_count = db_manager_->countGeolocations(params);
    std::vector<GeolocationRecord> locations = db_manager_->queryGeolocations(params);

    return buildSearchResult(locations, total_count, pagination);
}

std::vector<SignalRecord> SignalQueryInterface::getTrackHistory(
    const std::string& track_id,
    const std::optional<TimeRange>& time_range
) {
    QueryParams params;
    params.track_id = track_id;
    if (time_range) {
        params.start_time = time_range->start;
        params.end_time = time_range->end;
    }
    params.order_by = "timestamp";
    params.ascending = true;

    return db_manager_->querySignals(params);
}

std::vector<GeolocationRecord> SignalQueryInterface::getTrackPath(
    const std::string& track_id,
    const std::optional<TimeRange>& time_range
) {
    GeoQueryParams params;
    params.track_id = track_id;
    if (time_range) {
        params.start_time = time_range->start;
        params.end_time = time_range->end;
    }
    params.order_by = "timestamp";
    params.ascending = true;

    return db_manager_->queryGeolocations(params);
}

SignalQueryInterface::SignalStats SignalQueryInterface::getSignalStatistics(
    const std::optional<TimeRange>& time_range,
    const std::optional<std::string>& node_id
) {
    QueryParams params;
    if (time_range) {
        params.start_time = time_range->start;
        params.end_time = time_range->end;
    }
    if (node_id) {
        params.node_id = *node_id;
    }

    std::vector<SignalRecord> signals = db_manager_->querySignals(params);
    SignalStats stats{};
    stats.total_signals = signals.size();

    if (!signals.empty()) {
        // Calculate averages and find frequency range
        double total_power = 0.0;
        double total_snr = 0.0;
        stats.min_frequency = signals[0].frequency;
        stats.max_frequency = signals[0].frequency;

        for (const auto& signal : signals) {
            total_power += signal.power;
            total_snr += signal.snr;
            stats.min_frequency = std::min(stats.min_frequency, signal.frequency);
            stats.max_frequency = std::max(stats.max_frequency, signal.frequency);

            // Count by class
            if (signal.signal_class) {
                stats.signals_by_class[*signal.signal_class]++;
            }

            // Count by node
            stats.signals_by_node[signal.node_id]++;
        }

        stats.avg_power = total_power / signals.size();
        stats.avg_snr = total_snr / signals.size();
    }

    return stats;
}

SignalQueryInterface::GeoStats SignalQueryInterface::getGeolocationStatistics(
    const std::optional<TimeRange>& time_range,
    const std::optional<std::string>& method
) {
    GeoQueryParams params;
    if (time_range) {
        params.start_time = time_range->start;
        params.end_time = time_range->end;
    }
    if (method) {
        params.method = *method;
    }

    std::vector<GeolocationRecord> locations = db_manager_->queryGeolocations(params);
    GeoStats stats{};
    stats.total_locations = locations.size();

    if (!locations.empty()) {
        // Calculate average confidence and find coverage area
        double total_confidence = 0.0;
        stats.coverage_area.min_latitude = locations[0].latitude;
        stats.coverage_area.max_latitude = locations[0].latitude;
        stats.coverage_area.min_longitude = locations[0].longitude;
        stats.coverage_area.max_longitude = locations[0].longitude;

        for (const auto& location : locations) {
            if (location.confidence) {
                total_confidence += *location.confidence;
            }

            stats.coverage_area.min_latitude = std::min(stats.coverage_area.min_latitude, location.latitude);
            stats.coverage_area.max_latitude = std::max(stats.coverage_area.max_latitude, location.latitude);
            stats.coverage_area.min_longitude = std::min(stats.coverage_area.min_longitude, location.longitude);
            stats.coverage_area.max_longitude = std::max(stats.coverage_area.max_longitude, location.longitude);

            // Count by method
            stats.locations_by_method[location.method]++;
        }

        stats.avg_confidence = total_confidence / locations.size();
    }

    return stats;
}

std::vector<std::string> SignalQueryInterface::findRelatedTracks(
    const std::string& track_id,
    double frequency_tolerance,
    double time_tolerance_seconds
) {
    // Get signals from the target track
    std::vector<SignalRecord> track_signals = getTrackHistory(track_id);
    if (track_signals.empty()) {
        return {};
    }

    // Find time range for the track
    auto [min_time_it, max_time_it] = std::minmax_element(
        track_signals.begin(), track_signals.end(),
        [](const auto& a, const auto& b) { return a.timestamp < b.timestamp; }
    );

    // Extend time range by tolerance
    TimeRange time_range{
        min_time_it->timestamp - std::chrono::seconds(static_cast<int>(time_tolerance_seconds)),
        max_time_it->timestamp + std::chrono::seconds(static_cast<int>(time_tolerance_seconds))
    };

    // Find frequency range
    auto [min_freq_it, max_freq_it] = std::minmax_element(
        track_signals.begin(), track_signals.end(),
        [](const auto& a, const auto& b) { return a.frequency < b.frequency; }
    );

    FrequencyRange freq_range{
        min_freq_it->frequency - frequency_tolerance,
        max_freq_it->frequency + frequency_tolerance
    };

    // Search for signals in the extended range
    QueryParams params;
    params.start_time = time_range.start;
    params.end_time = time_range.end;
    params.min_frequency = freq_range.min_frequency;
    params.max_frequency = freq_range.max_frequency;

    std::vector<SignalRecord> nearby_signals = db_manager_->querySignals(params);

    // Collect unique track IDs (excluding the input track)
    std::unordered_set<std::string> related_tracks;
    for (const auto& signal : nearby_signals) {
        if (signal.track_id && *signal.track_id != track_id) {
            related_tracks.insert(*signal.track_id);
        }
    }

    return std::vector<std::string>(related_tracks.begin(), related_tracks.end());
}

std::vector<SignalQueryInterface::SignalDensity> SignalQueryInterface::getFrequencyDensity(
    const FrequencyRange& range,
    double bin_size,
    const std::optional<TimeRange>& time_range
) {
    QueryParams params;
    params.min_frequency = range.min_frequency;
    params.max_frequency = range.max_frequency;
    if (time_range) {
        params.start_time = time_range->start;
        params.end_time = time_range->end;
    }

    std::vector<SignalRecord> signals = db_manager_->querySignals(params);
    if (signals.empty()) {
        return {};
    }

    // Calculate number of bins
    size_t num_bins = static_cast<size_t>(std::ceil((range.max_frequency - range.min_frequency) / bin_size));
    std::vector<SignalDensity> density(num_bins);

    // Initialize bin frequencies
    for (size_t i = 0; i < num_bins; ++i) {
        density[i].frequency = range.min_frequency + (i * bin_size);
    }

    // Distribute signals into bins
    for (const auto& signal : signals) {
        size_t bin_index = static_cast<size_t>((signal.frequency - range.min_frequency) / bin_size);
        if (bin_index < num_bins) {
            auto& bin = density[bin_index];
            bin.signal_count++;
            bin.avg_power = ((bin.avg_power * (bin.signal_count - 1)) + signal.power) / bin.signal_count;
            bin.avg_snr = ((bin.avg_snr * (bin.signal_count - 1)) + signal.snr) / bin.signal_count;
        }
    }

    return density;
}

SearchResult<EventRecord> SignalQueryInterface::searchEvents(
    const std::optional<TimeRange>& time_range,
    const std::optional<std::string>& event_type,
    const std::optional<EventSeverity>& min_severity,
    const std::optional<std::string>& source,
    const PaginationParams& pagination
) {
    EventQueryParams params = buildEventQueryParams(
        time_range, event_type, min_severity,
        source, pagination
    );

    size_t total_count = db_manager_->countEvents(params);
    std::vector<EventRecord> events = db_manager_->queryEvents(params);

    return buildSearchResult(events, total_count, pagination);
}

SearchResult<ReportRecord> SignalQueryInterface::searchReports(
    const std::optional<TimeRange>& time_range,
    const std::optional<std::string>& report_type,
    const std::optional<std::string>& created_by,
    const PaginationParams& pagination
) {
    ReportQueryParams params = buildReportQueryParams(
        time_range, report_type, created_by, pagination
    );

    size_t total_count = db_manager_->countReports(params);
    std::vector<ReportRecord> reports = db_manager_->queryReports(params);

    return buildSearchResult(reports, total_count, pagination);
}

QueryParams SignalQueryInterface::buildSignalQueryParams(
    const std::optional<TimeRange>& time_range,
    const std::optional<FrequencyRange>& freq_range,
    const std::optional<SignalCharacteristics>& characteristics,
    const std::optional<std::string>& node_id,
    const std::optional<std::string>& track_id,
    const PaginationParams& pagination
) const {
    QueryParams params;

    if (time_range) {
        params.start_time = time_range->start;
        params.end_time = time_range->end;
    }

    if (freq_range) {
        params.min_frequency = freq_range->min_frequency;
        params.max_frequency = freq_range->max_frequency;
    }

    if (characteristics) {
        params.min_power = characteristics->min_power;
        params.min_snr = characteristics->min_snr;
        params.signal_class = characteristics->signal_class;
    }

    params.node_id = node_id;
    params.track_id = track_id;

    params.limit = pagination.page_size;
    params.offset = pagination.page_size * pagination.page_number;
    params.order_by = pagination.sort_by;
    params.ascending = pagination.ascending;

    return params;
}

GeoQueryParams SignalQueryInterface::buildGeoQueryParams(
    const std::optional<TimeRange>& time_range,
    const std::optional<GeoRegion>& region,
    const std::optional<std::string>& track_id,
    const std::optional<std::string>& method,
    const std::optional<double>& min_confidence,
    const PaginationParams& pagination
) const {
    GeoQueryParams params;

    if (time_range) {
        params.start_time = time_range->start;
        params.end_time = time_range->end;
    }

    if (region) {
        params.min_latitude = region->min_latitude;
        params.max_latitude = region->max_latitude;
        params.min_longitude = region->min_longitude;
        params.max_longitude = region->max_longitude;
    }

    params.track_id = track_id;
    params.method = method;
    params.min_confidence = min_confidence;

    params.limit = pagination.page_size;
    params.offset = pagination.page_size * pagination.page_number;
    params.order_by = pagination.sort_by;
    params.ascending = pagination.ascending;

    return params;
}

EventQueryParams SignalQueryInterface::buildEventQueryParams(
    const std::optional<TimeRange>& time_range,
    const std::optional<std::string>& event_type,
    const std::optional<EventSeverity>& min_severity,
    const std::optional<std::string>& source,
    const PaginationParams& pagination
) const {
    EventQueryParams params;

    if (time_range) {
        params.start_time = time_range->start;
        params.end_time = time_range->end;
    }

    params.event_type = event_type;
    params.min_severity = min_severity;
    params.source = source;

    params.limit = pagination.page_size;
    params.offset = pagination.page_size * pagination.page_number;

    return params;
}

ReportQueryParams SignalQueryInterface::buildReportQueryParams(
    const std::optional<TimeRange>& time_range,
    const std::optional<std::string>& report_type,
    const std::optional<std::string>& created_by,
    const PaginationParams& pagination
) const {
    ReportQueryParams params;

    if (time_range) {
        params.start_time = time_range->start;
        params.end_time = time_range->end;
    }

    params.report_type = report_type;
    params.created_by = created_by;

    params.limit = pagination.page_size;
    params.offset = pagination.page_size * pagination.page_number;

    return params;
}

template<typename T>
SearchResult<T> SignalQueryInterface::buildSearchResult(
    const std::vector<T>& items,
    size_t total_count,
    const PaginationParams& pagination
) const {
    SearchResult<T> result;
    result.items = items;
    result.total_count = total_count;
    result.current_page = pagination.page_number;
    result.page_count = (total_count + pagination.page_size - 1) / pagination.page_size;
    result.has_previous_page = pagination.page_number > 0;
    result.has_next_page = (pagination.page_number + 1) < result.page_count;
    return result;
}

} // namespace database
} // namespace tdoa 