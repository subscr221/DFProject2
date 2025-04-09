#include "../report_generator.h"
#include "../signal_db_manager.h"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace tdoa::database;
using namespace std::chrono;

// Helper function to read file contents
std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

// Helper function to create a test template
ReportTemplate createTestTemplate() {
    ReportTemplate templ;
    templ.name = "daily_summary";
    templ.description = "Daily signal detection summary report";
    templ.sections = {
        "signal_summary",
        "geolocation_summary",
        "frequency_analysis",
        "event_summary"
    };
    templ.parameters = {
        {"time_range", "required"},
        {"node_id", "optional"}
    };
    templ.supported_formats = {
        ReportFormat::CSV,
        ReportFormat::JSON,
        ReportFormat::KML,
        ReportFormat::PDF
    };
    return templ;
}

// Helper function to create a test schedule
ReportSchedule createTestSchedule() {
    ReportSchedule schedule;
    schedule.report_name = "daily_summary";
    schedule.next_run = system_clock::now() + hours(24);
    schedule.interval = hours(24);
    schedule.is_enabled = true;
    schedule.format = ReportFormat::JSON;
    schedule.output_path = "test_report.json";
    schedule.parameters = {
        {"time_range", "24h"},
        {"node_id", "NODE001"}
    };
    return schedule;
}

int main() {
    // Create database manager and report generator
    auto db_manager = std::make_shared<SignalDBManager>("test_signals.db");
    ReportGenerator generator(db_manager);

    try {
        // Test 1: Template Management
        std::cout << "Test 1: Template Management\n";
        {
            auto templ = createTestTemplate();
            generator.registerTemplate(templ);

            auto templates = generator.listTemplates();
            assert(templates.size() == 1);
            assert(templates[0].name == "daily_summary");

            auto retrieved_templ = generator.getTemplate("daily_summary");
            assert(retrieved_templ);
            assert(retrieved_templ->sections.size() == 4);

            generator.removeTemplate("daily_summary");
            templates = generator.listTemplates();
            assert(templates.empty());

            std::cout << "Template management tests passed\n\n";
        }

        // Test 2: Schedule Management
        std::cout << "Test 2: Schedule Management\n";
        {
            auto schedule = createTestSchedule();
            generator.scheduleReport(schedule);

            auto schedules = generator.listSchedules();
            assert(schedules.size() == 1);
            assert(schedules[0].report_name == "daily_summary");

            generator.enableSchedule("daily_summary", false);
            schedules = generator.listSchedules();
            assert(!schedules[0].is_enabled);

            generator.removeSchedule("daily_summary");
            schedules = generator.listSchedules();
            assert(schedules.empty());

            std::cout << "Schedule management tests passed\n\n";
        }

        // Test 3: Report Generation
        std::cout << "Test 3: Report Generation\n";
        {
            // Register template
            auto templ = createTestTemplate();
            generator.registerTemplate(templ);

            // Create report options
            ReportOptions options;
            options.format = ReportFormat::JSON;
            options.time_range = TimeRange{
                system_clock::now() - hours(24),
                system_clock::now()
            };
            options.freq_range = FrequencyRange{100.0, 200.0};
            options.custom_parameters["time_range"] = "24h";

            // Generate report
            std::string report_data = generator.generateReport("daily_summary", options);
            std::cout << "Generated report:\n" << report_data << "\n\n";

            std::cout << "Report generation test passed\n\n";
        }

        // Test 4: Report Export
        std::cout << "Test 4: Report Export\n";
        {
            std::string test_data = "Test report data";
            std::string csv_path = "test_report.csv";
            std::string json_path = "test_report.json";
            std::string kml_path = "test_report.kml";

            // Test CSV export
            generator.exportToCSV(test_data, csv_path);
            assert(std::filesystem::exists(csv_path));
            assert(readFile(csv_path) == test_data);

            // Test JSON export
            generator.exportToJSON(test_data, json_path);
            assert(std::filesystem::exists(json_path));
            assert(readFile(json_path) == test_data);

            // Test KML export
            generator.exportToKML(test_data, kml_path);
            assert(std::filesystem::exists(kml_path));

            // Clean up test files
            std::filesystem::remove(csv_path);
            std::filesystem::remove(json_path);
            std::filesystem::remove(kml_path);

            std::cout << "Report export tests passed\n\n";
        }

        // Test 5: Scheduled Report Processing
        std::cout << "Test 5: Scheduled Report Processing\n";
        {
            // Register template
            auto templ = createTestTemplate();
            generator.registerTemplate(templ);

            // Create and register an immediate schedule
            auto schedule = createTestSchedule();
            schedule.next_run = system_clock::now() - seconds(1); // Due immediately
            schedule.interval = seconds(30);
            schedule.format = ReportFormat::JSON;
            schedule.output_path = "scheduled_report.json";
            generator.scheduleReport(schedule);

            // Process scheduled reports
            generator.processScheduledReports();

            // Verify the report was generated
            assert(std::filesystem::exists("scheduled_report.json"));
            std::filesystem::remove("scheduled_report.json");

            std::cout << "Scheduled report processing test passed\n\n";
        }

        std::cout << "All tests completed successfully!\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Test failed with error: " << e.what() << std::endl;
        return 1;
    }
} 