/**
 * @file gps_device.h
 * @brief GPS device interface for time synchronization
 */

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <chrono>

namespace tdoa {
namespace devices {

/**
 * @brief GPS satellite information
 */
struct GPSSatellite {
    int id;                ///< Satellite ID/PRN
    double elevation;      ///< Elevation in degrees
    double azimuth;        ///< Azimuth in degrees
    double signalStrength; ///< Signal strength (SNR) in dB
    bool used;             ///< Whether this satellite is used in position solution
    
    GPSSatellite() 
        : id(0)
        , elevation(0.0)
        , azimuth(0.0)
        , signalStrength(0.0)
        , used(false)
    {}
};

/**
 * @brief GPS position and time information
 */
struct GPSData {
    // Position
    double latitude;   ///< Latitude in degrees (positive is North)
    double longitude;  ///< Longitude in degrees (positive is East)
    double altitude;   ///< Altitude in meters above mean sea level
    
    // Time
    uint16_t year;     ///< Year (4 digits)
    uint8_t month;     ///< Month (1-12)
    uint8_t day;       ///< Day (1-31)
    uint8_t hour;      ///< Hour (0-23)
    uint8_t minute;    ///< Minute (0-59)
    uint8_t second;    ///< Second (0-59)
    uint32_t nanos;    ///< Nanoseconds part (0-999,999,999)
    
    // Quality indicators
    bool fix;          ///< Whether we have a position fix
    uint8_t fixType;   ///< Fix type (0=none, 1=GPS, 2=DGPS, 3=PPS, 4=RTK, 5=Float RTK)
    uint8_t satellites;///< Number of satellites used in solution
    double hdop;       ///< Horizontal dilution of precision
    double pdop;       ///< Position dilution of precision
    double vdop;       ///< Vertical dilution of precision
    double accuracy;   ///< Estimated position accuracy in meters
    
    // Satellite information
    std::vector<GPSSatellite> satelliteInfo; ///< Information about visible satellites
    
    GPSData()
        : latitude(0.0)
        , longitude(0.0)
        , altitude(0.0)
        , year(0)
        , month(0)
        , day(0)
        , hour(0)
        , minute(0)
        , second(0)
        , nanos(0)
        , fix(false)
        , fixType(0)
        , satellites(0)
        , hdop(99.99)
        , pdop(99.99)
        , vdop(99.99)
        , accuracy(9999.0)
    {}
    
    /**
     * @brief Convert GPS time to system time
     * @return System time point corresponding to GPS time
     */
    std::chrono::system_clock::time_point toSystemTime() const;
};

/**
 * @brief Callback for GPS data updates
 */
using GPSDataCallback = std::function<void(const GPSData&)>;

/**
 * @brief Callback for PPS (Pulse Per Second) events
 */
using PPSCallback = std::function<void(uint64_t timestamp)>;

/**
 * @class GPSDevice
 * @brief Interface for GPS devices used in time synchronization
 */
class GPSDevice {
public:
    /**
     * @brief Default constructor
     */
    GPSDevice() = default;
    
    /**
     * @brief Virtual destructor
     */
    virtual ~GPSDevice() = default;
    
    /**
     * @brief Open GPS device connection
     * @param port Serial port or other device identifier
     * @return True if connection successful
     */
    virtual bool open(const std::string& port) = 0;
    
    /**
     * @brief Close GPS device connection
     * @return True if closed successfully
     */
    virtual bool close() = 0;
    
    /**
     * @brief Check if device is connected
     * @return True if device is connected
     */
    virtual bool isConnected() const = 0;
    
    /**
     * @brief Get last received GPS data
     * @return Latest GPS data
     */
    virtual GPSData getLastData() const = 0;
    
    /**
     * @brief Register callback for GPS data updates
     * @param callback Function to call when new GPS data arrives
     */
    virtual void registerDataCallback(GPSDataCallback callback) = 0;
    
    /**
     * @brief Register callback for PPS signals
     * @param callback Function to call when PPS signal is detected
     */
    virtual void registerPPSCallback(PPSCallback callback) = 0;
    
    /**
     * @brief Get the delay between PPS signal and GPS epoch
     * @return Delay in nanoseconds
     */
    virtual double getPPSOffset() const = 0;
    
    /**
     * @brief Set the GPIO pin used for PPS input
     * @param pin GPIO pin number
     * @return True if successful
     */
    virtual bool setPPSInputPin(int pin) = 0;
    
    /**
     * @brief Configure the GPS receiver
     * @param configOption Configuration option name
     * @param value Value to set
     * @return True if successful
     */
    virtual bool configure(const std::string& configOption, const std::string& value) = 0;
};

/**
 * @brief Create a GPS device of the specified type
 * @param deviceType Type of GPS device to create (e.g., "GPSD", "NMEA", "UBLOX")
 * @return Unique pointer to GPS device
 */
std::unique_ptr<GPSDevice> createGPSDevice(const std::string& deviceType);

} // namespace devices
} // namespace tdoa 