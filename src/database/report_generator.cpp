#include "report_generator.h"
#include "signal_db_manager.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <filesystem>

namespace tdoa {
namespace database {

ReportGenerator::ReportGenerator(std::shared_ptr<SignalDBManager> db_manager)
    : db_manager_(std::move(db_manager))
    , query_interface_(std::make_shared<SignalQueryInterface>(db_manager_)) {
}

void ReportGenerator::registerTemplate(const ReportTemplate& template_config) {
    validateTemplate(template_config);
    templates_[template_config.name] = template_config;
}

void ReportGenerator::removeTemplate(const std::string& template_name) {
    if (templates_.erase(template_name) == 0) {
        throw std::runtime_error("Template not found: " + template_name);
    }
}

std::vector<ReportTemplate> ReportGenerator::listTemplates() const {
    std::vector<ReportTemplate> result;
    result.reserve(templates_.size());
    for (const auto& [_, templ] : templates_) {
        result.push_back(templ);
    }
    return result;
}

std::optional<ReportTemplate> ReportGenerator::getTemplate(const std::string& template_name) const {
    auto it = templates_.find(template_name);
    if (it != templates_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void ReportGenerator::scheduleReport(const ReportSchedule& schedule) {
    validateSchedule(schedule);
    schedules_[schedule.report_name] = schedule;
}

void ReportGenerator::updateSchedule(const std::string& report_name, const ReportSchedule& new_schedule) {
    if (schedules_.find(report_name) == schedules_.end()) {
        throw std::runtime_error("Schedule not found: " + report_name);
    }
    validateSchedule(new_schedule);
    schedules_[report_name] = new_schedule;
}

void ReportGenerator::removeSchedule(const std::string& report_name) {
    if (schedules_.erase(report_name) == 0) {
        throw std::runtime_error("Schedule not found: " + report_name);
    }
}

std::vector<ReportSchedule> ReportGenerator::listSchedules() const {
    std::vector<ReportSchedule> result;
    result.reserve(schedules_.size());
    for (const auto& [_, schedule] : schedules_) {
        result.push_back(schedule);
    }
    return result;
}

void ReportGenerator::enableSchedule(const std::string& report_name, bool enabled) {
    auto it = schedules_.find(report_name);
    if (it == schedules_.end()) {
        throw std::runtime_error("Schedule not found: " + report_name);
    }
    it->second.is_enabled = enabled;
}

std::string ReportGenerator::generateReport(
    const std::string& template_name,
    const ReportOptions& options
) {
    auto templ = getTemplate(template_name);
    if (!templ) {
        throw std::runtime_error("Template not found: " + template_name);
    }

    validateOptions(*templ, options);
    std::string report_data = processTemplate(*templ, options);
    return formatReportData(report_data, options.format);
}

void ReportGenerator::exportToCSV(const std::string& report_data, const std::string& output_path) {
    std::ofstream file(output_path);
    if (!file) {
        throw std::runtime_error("Failed to open file for writing: " + output_path);
    }
    file << report_data;
}

void ReportGenerator::exportToKML(const std::string& report_data, const std::string& output_path) {
    std::ofstream file(output_path);
    if (!file) {
        throw std::runtime_error("Failed to open file for writing: " + output_path);
    }
    
    // KML header
    file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         << "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n"
         << "<Document>\n"
         << "<name>Signal Detection Report</name>\n"
         << report_data
         << "</Document>\n"
         << "</kml>";
}

void ReportGenerator::exportToPDF(const std::string& report_data, const std::string& output_path) {
    // Note: This is a placeholder. In a real implementation, we would use a PDF
    // generation library like libharu, PDFlib, or wkhtmltopdf
    throw std::runtime_error("PDF export not implemented");
}

void ReportGenerator::exportToJSON(const std::string& report_data, const std::string& output_path) {
    std::ofstream file(output_path);
    if (!file) {
        throw std::runtime_error("Failed to open file for writing: " + output_path);
    }
    file << report_data;
}

void ReportGenerator::processScheduledReports() {
    auto now = std::chrono::system_clock::now();
    auto due_reports = getDueReports();

    for (const auto& schedule : due_reports) {
        if (!schedule.is_enabled) {
            continue;
        }

        try {
            // Create report options from schedule parameters
            ReportOptions options;
            options.format = schedule.format;
            
            // Set time range to last interval
            options.time_range = TimeRange{
                now - schedule.interval,
                now
            };

            // Generate and export report
            auto report_data = generateReport(schedule.report_name, options);
            
            switch (schedule.format) {
                case ReportFormat::CSV:
                    exportToCSV(report_data, schedule.output_path);
                    break;
                case ReportFormat::KML:
                    exportToKML(report_data, schedule.output_path);
                    break;
                case ReportFormat::PDF:
                    exportToPDF(report_data, schedule.output_path);
                    break;
                case ReportFormat::JSON:
                    exportToJSON(report_data, schedule.output_path);
                    break;
            }

            // Update next run time
            auto& mutable_schedule = schedules_[schedule.report_name];
            mutable_schedule.next_run = now + schedule.interval;

        } catch (const std::exception& e) {
            // Log error but continue processing other reports
            std::cerr << "Failed to process scheduled report " << schedule.report_name
                     << ": " << e.what() << std::endl;
        }
    }
}

std::vector<ReportSchedule> ReportGenerator::getDueReports() const {
    std::vector<ReportSchedule> due_reports;
    auto now = std::chrono::system_clock::now();

    for (const auto& [_, schedule] : schedules_) {
        if (schedule.is_enabled && schedule.next_run <= now) {
            due_reports.push_back(schedule);
        }
    }

    return due_reports;
}

std::string ReportGenerator::processTemplate(
    const ReportTemplate& templ,
    const ReportOptions& options
) {
    std::stringstream report;

    for (const auto& section : templ.sections) {
        if (section == "signal_summary") {
            report << generateSignalSummary(options);
        } else if (section == "tracking_summary") {
            report << generateTrackingSummary(options);
        } else if (section == "geolocation_summary") {
            report << generateGeolocationSummary(options);
        } else if (section == "frequency_analysis") {
            report << generateFrequencyAnalysis(options);
        } else if (section == "event_summary") {
            report << generateEventSummary(options);
        }
        report << "\n";
    }

    return report.str();
}

std::string ReportGenerator::generateSignalSummary(const ReportOptions& options) {
    auto stats = query_interface_->getSignalStatistics(options.time_range, options.node_id);
    
    std::stringstream ss;
    ss << "Signal Summary\n"
       << "-------------\n"
       << "Total Signals: " << stats.total_signals << "\n"
       << "Frequency Range: " << stats.min_frequency << " - " << stats.max_frequency << " MHz\n"
       << "Average Power: " << stats.avg_power << " dBm\n"
       << "Average SNR: " << stats.avg_snr << " dB\n\n"
       << "Signal Classes:\n";
    
    for (const auto& [class_name, count] : stats.signals_by_class) {
        ss << "  " << class_name << ": " << count << "\n";
    }

    return ss.str();
}

std::string ReportGenerator::generateTrackingSummary(const ReportOptions& options) {
    if (!options.track_id) {
        return "No track specified for tracking summary.\n";
    }

    auto track_signals = query_interface_->getTrackHistory(*options.track_id, options.time_range);
    auto track_path = query_interface_->getTrackPath(*options.track_id, options.time_range);

    std::stringstream ss;
    ss << "Track Summary: " << *options.track_id << "\n"
       << "--------------\n"
       << "Total Points: " << track_signals.size() << "\n"
       << "Geolocations: " << track_path.size() << "\n\n"
       << "Signal History:\n";

    for (const auto& signal : track_signals) {
        ss << "  Time: " << std::chrono::system_clock::to_time_t(signal.timestamp)
           << ", Freq: " << signal.frequency
           << " MHz, Power: " << signal.power
           << " dBm, SNR: " << signal.snr << " dB\n";
    }

    return ss.str();
}

std::string ReportGenerator::generateGeolocationSummary(const ReportOptions& options) {
    auto stats = query_interface_->getGeolocationStatistics(options.time_range);
    
    std::stringstream ss;
    ss << "Geolocation Summary\n"
       << "-------------------\n"
       << "Total Locations: " << stats.total_locations << "\n"
       << "Average Confidence: " << stats.avg_confidence << "%\n"
       << "Coverage Area:\n"
       << "  Latitude: " << stats.coverage_area.min_latitude << "° to "
       << stats.coverage_area.max_latitude << "°\n"
       << "  Longitude: " << stats.coverage_area.min_longitude << "° to "
       << stats.coverage_area.max_longitude << "°\n\n"
       << "Methods Used:\n";

    for (const auto& [method, count] : stats.locations_by_method) {
        ss << "  " << method << ": " << count << "\n";
    }

    return ss.str();
}

std::string ReportGenerator::generateFrequencyAnalysis(const ReportOptions& options) {
    if (!options.freq_range) {
        return "No frequency range specified for analysis.\n";
    }

    auto density = query_interface_->getFrequencyDensity(
        *options.freq_range,
        1.0, // 1 MHz bins
        options.time_range
    );

    std::stringstream ss;
    ss << "Frequency Analysis\n"
       << "------------------\n"
       << "Range: " << options.freq_range->min_frequency << " - "
       << options.freq_range->max_frequency << " MHz\n\n"
       << std::setw(12) << "Freq (MHz)" << " | "
       << std::setw(6) << "Count" << " | "
       << std::setw(12) << "Avg Power" << " | "
       << std::setw(8) << "Avg SNR" << "\n"
       << std::string(44, '-') << "\n";

    for (const auto& bin : density) {
        ss << std::fixed << std::setprecision(2)
           << std::setw(12) << bin.frequency << " | "
           << std::setw(6) << bin.signal_count << " | "
           << std::setw(12) << bin.avg_power << " | "
           << std::setw(8) << bin.avg_snr << "\n";
    }

    return ss.str();
}

std::string ReportGenerator::generateEventSummary(const ReportOptions& options) {
    PaginationParams pagination{0, 100, "timestamp", true};
    auto events = query_interface_->searchEvents(
        options.time_range,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        pagination
    );

    std::stringstream ss;
    ss << "Event Summary\n"
       << "-------------\n"
       << "Total Events: " << events.total_count << "\n\n"
       << "Recent Events:\n";

    for (const auto& event : events.items) {
        ss << "  [" << std::chrono::system_clock::to_time_t(event.timestamp) << "] "
           << event.event_type << " (" << static_cast<int>(event.severity) << "): "
           << event.description << "\n";
    }

    return ss.str();
}

std::string ReportGenerator::formatReportData(
    const std::string& report_data,
    ReportFormat format
) {
    switch (format) {
        case ReportFormat::CSV:
            // Convert report data to CSV format
            return report_data; // Placeholder: implement proper CSV formatting
        case ReportFormat::KML:
            // Convert report data to KML format
            return report_data; // Placeholder: implement proper KML formatting
        case ReportFormat::PDF:
            // Convert report data to PDF-compatible format
            return report_data; // Placeholder: implement proper PDF formatting
        case ReportFormat::JSON:
            // Convert report data to JSON format
            return report_data; // Placeholder: implement proper JSON formatting
        default:
            throw std::runtime_error("Unsupported report format");
    }
}

void ReportGenerator::validateTemplate(const ReportTemplate& template_config) const {
    if (template_config.name.empty()) {
        throw std::invalid_argument("Template name cannot be empty");
    }
    if (template_config.sections.empty()) {
        throw std::invalid_argument("Template must have at least one section");
    }
    if (template_config.supported_formats.empty()) {
        throw std::invalid_argument("Template must support at least one format");
    }
}

void ReportGenerator::validateSchedule(const ReportSchedule& schedule) const {
    if (schedule.report_name.empty()) {
        throw std::invalid_argument("Schedule report name cannot be empty");
    }
    if (schedule.interval.count() <= 0) {
        throw std::invalid_argument("Schedule interval must be positive");
    }
    if (schedule.output_path.empty()) {
        throw std::invalid_argument("Schedule output path cannot be empty");
    }
}

void ReportGenerator::validateOptions(
    const ReportTemplate& templ,
    const ReportOptions& options
) const {
    // Check if the requested format is supported by the template
    if (std::find(templ.supported_formats.begin(),
                  templ.supported_formats.end(),
                  options.format) == templ.supported_formats.end()) {
        throw std::invalid_argument("Requested format is not supported by the template");
    }

    // Validate required parameters
    for (const auto& [param_name, param_value] : templ.parameters) {
        if (param_value == "required" &&
            options.custom_parameters.find(param_name) == options.custom_parameters.end()) {
            throw std::invalid_argument("Missing required parameter: " + param_name);
        }
    }
}

} // namespace database
} // namespace tdoa 