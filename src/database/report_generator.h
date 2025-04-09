#pragma once

#include "signal_query_interface.h"
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <chrono>
#include <optional>

namespace tdoa {
namespace database {

// Forward declarations
class SignalDBManager;

// Supported report formats
enum class ReportFormat {
    CSV,
    KML,
    PDF,
    JSON
};

// Report template configuration
struct ReportTemplate {
    std::string name;
    std::string description;
    std::vector<std::string> sections;
    std::map<std::string, std::string> parameters;
    std::vector<ReportFormat> supported_formats;
};

// Report schedule configuration
struct ReportSchedule {
    std::string report_name;
    std::chrono::system_clock::time_point next_run;
    std::chrono::seconds interval;
    bool is_enabled;
    ReportFormat format;
    std::string output_path;
    std::map<std::string, std::string> parameters;
};

// Report generation options
struct ReportOptions {
    ReportFormat format;
    std::optional<TimeRange> time_range;
    std::optional<std::string> node_id;
    std::optional<std::string> track_id;
    std::optional<FrequencyRange> freq_range;
    std::optional<GeoRegion> geo_region;
    std::map<std::string, std::string> custom_parameters;
};

class ReportGenerator {
public:
    explicit ReportGenerator(std::shared_ptr<SignalDBManager> db_manager);

    // Template management
    void registerTemplate(const ReportTemplate& template_config);
    void removeTemplate(const std::string& template_name);
    std::vector<ReportTemplate> listTemplates() const;
    std::optional<ReportTemplate> getTemplate(const std::string& template_name) const;

    // Schedule management
    void scheduleReport(const ReportSchedule& schedule);
    void updateSchedule(const std::string& report_name, const ReportSchedule& new_schedule);
    void removeSchedule(const std::string& report_name);
    std::vector<ReportSchedule> listSchedules() const;
    void enableSchedule(const std::string& report_name, bool enabled);

    // Report generation
    std::string generateReport(
        const std::string& template_name,
        const ReportOptions& options
    );

    // Export functionality
    void exportToCSV(const std::string& report_data, const std::string& output_path);
    void exportToKML(const std::string& report_data, const std::string& output_path);
    void exportToPDF(const std::string& report_data, const std::string& output_path);
    void exportToJSON(const std::string& report_data, const std::string& output_path);

    // Schedule processing
    void processScheduledReports();
    std::vector<ReportSchedule> getDueReports() const;

private:
    std::shared_ptr<SignalDBManager> db_manager_;
    std::shared_ptr<SignalQueryInterface> query_interface_;
    std::map<std::string, ReportTemplate> templates_;
    std::map<std::string, ReportSchedule> schedules_;

    // Template processing
    std::string processTemplate(
        const ReportTemplate& templ,
        const ReportOptions& options
    );

    // Section generators
    std::string generateSignalSummary(const ReportOptions& options);
    std::string generateTrackingSummary(const ReportOptions& options);
    std::string generateGeolocationSummary(const ReportOptions& options);
    std::string generateFrequencyAnalysis(const ReportOptions& options);
    std::string generateEventSummary(const ReportOptions& options);

    // Helper functions
    std::string formatReportData(
        const std::string& report_data,
        ReportFormat format
    );
    void validateTemplate(const ReportTemplate& template_config) const;
    void validateSchedule(const ReportSchedule& schedule) const;
    void validateOptions(
        const ReportTemplate& templ,
        const ReportOptions& options
    ) const;
};

} // namespace database
} // namespace tdoa 