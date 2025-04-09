/**
 * @file gps_device_factory.cpp
 * @brief Implementation of the GPS device factory
 */

#include "gps_device.h"
#include "gpsd_device.h"
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace tdoa {
namespace devices {

/**
 * @brief Create a GPS device of the specified type
 * @param deviceType Type of GPS device to create (e.g., "GPSD", "NMEA", "UBLOX")
 * @return Unique pointer to GPS device
 */
std::unique_ptr<GPSDevice> createGPSDevice(const std::string& deviceType) {
    // Convert to uppercase for case-insensitive comparison
    std::string upperType = deviceType;
    std::transform(upperType.begin(), upperType.end(), upperType.begin(),
                  [](unsigned char c) { return std::toupper(c); });
    
    if (upperType == "GPSD") {
        return std::make_unique<GPSDDevice>();
    } else if (upperType == "UBLOX") {
        // Add UBLOX support in the future
        throw std::runtime_error("UBLOX GPS device not implemented yet");
    } else if (upperType == "NMEA") {
        // Add NMEA support in the future
        throw std::runtime_error("NMEA GPS device not implemented yet");
    } else {
        throw std::runtime_error("Unknown GPS device type: " + deviceType);
    }
}

// Implementation of GPSData::toSystemTime()
std::chrono::system_clock::time_point GPSData::toSystemTime() const {
    // Create a tm structure
    struct tm timeinfo = {};
    timeinfo.tm_year = year - 1900; // tm_year is years since 1900
    timeinfo.tm_mon = month - 1;    // tm_mon is months since January (0-11)
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;
    
    // Convert to time_t (seconds since epoch)
    time_t rawtime = mktime(&timeinfo);
    
    // Convert to system_clock time_point
    auto timePoint = std::chrono::system_clock::from_time_t(rawtime);
    
    // Add nanoseconds part
    timePoint += std::chrono::nanoseconds(nanos);
    
    return timePoint;
}

} // namespace devices
} // namespace tdoa 