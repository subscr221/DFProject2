/**
 * @file gpsd_device.cpp
 * @brief Implementation of GPS device interface using GPSD
 */

#include "gpsd_device.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <cstring>
#include <cmath>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>

// Include GPSD headers if available
#ifdef HAVE_GPSD
#include <gps.h>
#endif

namespace tdoa {
namespace devices {

GPSDDevice::GPSDDevice()
    : gpsData_(nullptr)
    , serverHost_("localhost")
    , serverPort_(2947)
    , ppsPinNumber_(-1)
    , ppsOffset_(0.0)
    , running_(false)
    , connected_(false)
    , ppsFileDescriptor_(-1)
{
}

GPSDDevice::~GPSDDevice() {
    close();
}

bool GPSDDevice::open(const std::string& port) {
#ifdef HAVE_GPSD
    // Parse server address (host:port)
    if (!parseServerAddress(port)) {
        std::cerr << "Invalid GPSD server address: " << port << std::endl;
        return false;
    }

    // Allocate GPSD data structure
    gpsData_ = new gps_data_t;
    if (!gpsData_) {
        std::cerr << "Failed to allocate GPSD data structure" << std::endl;
        return false;
    }

    // Connect to GPSD
    if (gps_open(serverHost_.c_str(), std::to_string(serverPort_).c_str(), 
                 static_cast<gps_data_t*>(gpsData_)) != 0) {
        std::cerr << "Failed to connect to GPSD: " << gps_errstr(errno) << std::endl;
        delete static_cast<gps_data_t*>(gpsData_);
        gpsData_ = nullptr;
        return false;
    }

    // Set watch parameters
    gps_stream(static_cast<gps_data_t*>(gpsData_), WATCH_ENABLE | WATCH_JSON, nullptr);

    // Start GPSD polling thread
    running_ = true;
    connected_ = true;
    gpsdThread_ = std::thread(&GPSDDevice::gpsdThreadFunc, this);

    // Start PPS thread if pin is set
    if (ppsPinNumber_ >= 0) {
        if (openPPSDevice()) {
            ppsThread_ = std::thread(&GPSDDevice::ppsThreadFunc, this);
        }
    }

    return true;
#else
    std::cerr << "GPSD support not compiled in" << std::endl;
    return false;
#endif
}

bool GPSDDevice::close() {
    // Stop threads
    running_ = false;
    
    if (gpsdThread_.joinable()) {
        gpsdThread_.join();
    }
    
    if (ppsThread_.joinable()) {
        ppsThread_.join();
    }
    
    // Close GPSD connection
#ifdef HAVE_GPSD
    if (gpsData_) {
        gps_stream(static_cast<gps_data_t*>(gpsData_), WATCH_DISABLE, nullptr);
        gps_close(static_cast<gps_data_t*>(gpsData_));
        delete static_cast<gps_data_t*>(gpsData_);
        gpsData_ = nullptr;
    }
#endif
    
    // Close PPS device
    closePPSDevice();
    
    connected_ = false;
    
    return true;
}

bool GPSDDevice::isConnected() const {
    return connected_;
}

GPSData GPSDDevice::getLastData() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    return lastGpsData_;
}

void GPSDDevice::registerDataCallback(GPSDataCallback callback) {
    std::lock_guard<std::mutex> lock(dataMutex_);
    dataCallback_ = callback;
}

void GPSDDevice::registerPPSCallback(PPSCallback callback) {
    std::lock_guard<std::mutex> lock(dataMutex_);
    ppsCallback_ = callback;
}

double GPSDDevice::getPPSOffset() const {
    return ppsOffset_;
}

bool GPSDDevice::setPPSInputPin(int pin) {
    // If already running, we need to restart the PPS thread
    bool wasRunning = running_;
    
    if (wasRunning && ppsThread_.joinable()) {
        running_ = false;
        ppsThread_.join();
    }
    
    ppsPinNumber_ = pin;
    
    // If device was already running, restart the PPS thread
    if (wasRunning) {
        running_ = true;
        if (openPPSDevice()) {
            ppsThread_ = std::thread(&GPSDDevice::ppsThreadFunc, this);
            return true;
        }
        return false;
    }
    
    return true;
}

bool GPSDDevice::configure(const std::string& configOption, const std::string& value) {
    // Store PPS offset if specified
    if (configOption == "pps_offset") {
        try {
            ppsOffset_ = std::stod(value);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Invalid PPS offset value: " << value << std::endl;
            return false;
        }
    }
    
    // Unknown option
    std::cerr << "Unknown configuration option: " << configOption << std::endl;
    return false;
}

void GPSDDevice::gpsdThreadFunc() {
#ifdef HAVE_GPSD
    while (running_) {
        // Wait for data from GPSD
        if (gps_waiting(static_cast<gps_data_t*>(gpsData_), 500000)) {
            // Read data
            if (gps_read(static_cast<gps_data_t*>(gpsData_)) == -1) {
                std::cerr << "Error reading from GPSD" << std::endl;
                connected_ = false;
                break;
            }
            
            // Convert and store GPS data
            GPSData data = convertGPSDData(static_cast<gps_data_t*>(gpsData_));
            
            {
                std::lock_guard<std::mutex> lock(dataMutex_);
                lastGpsData_ = data;
                
                // Call data callback if registered
                if (dataCallback_) {
                    dataCallback_(data);
                }
            }
        } else {
            // No data available, sleep to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
#endif
}

void GPSDDevice::ppsThreadFunc() {
    // PPS signal monitoring loop
    while (running_ && ppsFileDescriptor_ >= 0) {
        // Wait for PPS event
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(ppsFileDescriptor_, &readfds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int result = select(ppsFileDescriptor_ + 1, &readfds, nullptr, nullptr, &timeout);
        
        if (result > 0 && FD_ISSET(ppsFileDescriptor_, &readfds)) {
            // PPS event detected, get high-resolution timestamp
            auto timestamp = std::chrono::high_resolution_clock::now();
            auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
                timestamp.time_since_epoch()).count();
            
            // Call PPS callback if registered
            std::lock_guard<std::mutex> lock(dataMutex_);
            if (ppsCallback_) {
                ppsCallback_(nanoseconds);
            }
        } else if (result < 0) {
            // Error in select
            std::cerr << "Error waiting for PPS event: " << strerror(errno) << std::endl;
            break;
        }
    }
}

GPSData GPSDDevice::convertGPSDData(const gps_data_t* data) {
#ifdef HAVE_GPSD
    GPSData gpsData;
    
    if (!data) {
        return gpsData;
    }
    
    // Set fix status
    gpsData.fix = data->fix.mode >= MODE_2D;
    
    // Set fix type
    switch (data->fix.mode) {
        case MODE_NOT_SEEN:
        case MODE_NO_FIX:
            gpsData.fixType = 0;  // No fix
            break;
        case MODE_2D:
            gpsData.fixType = 1;  // 2D fix
            break;
        case MODE_3D:
            gpsData.fixType = 2;  // 3D fix
            break;
        default:
            gpsData.fixType = 0;
            break;
    }
    
    // Set position if available
    if (data->fix.mode >= MODE_2D && !std::isnan(data->fix.latitude) && !std::isnan(data->fix.longitude)) {
        gpsData.latitude = data->fix.latitude;
        gpsData.longitude = data->fix.longitude;
        
        if (data->fix.mode >= MODE_3D && !std::isnan(data->fix.altitude)) {
            gpsData.altitude = data->fix.altitude;
        }
    }
    
    // Set time information if available
    if (data->set & TIME_SET) {
        struct tm time_tm;
        time_t time_sec = (time_t)data->fix.time.tv_sec;
        
        // Convert to local time structure
        gmtime_r(&time_sec, &time_tm);
        
        gpsData.year = time_tm.tm_year + 1900;
        gpsData.month = time_tm.tm_mon + 1;
        gpsData.day = time_tm.tm_mday;
        gpsData.hour = time_tm.tm_hour;
        gpsData.minute = time_tm.tm_min;
        gpsData.second = time_tm.tm_sec;
        gpsData.nanos = data->fix.time.tv_nsec;
    }
    
    // Set quality indicators
    if (data->set & DOP_SET) {
        if (!std::isnan(data->dop.hdop)) gpsData.hdop = data->dop.hdop;
        if (!std::isnan(data->dop.pdop)) gpsData.pdop = data->dop.pdop;
        if (!std::isnan(data->dop.vdop)) gpsData.vdop = data->dop.vdop;
    }
    
    // Set satellite count if available
    if (data->satellites_visible > 0) {
        gpsData.satellites = data->satellites_used;
        
        // Fill satellite info
        for (int i = 0; i < data->satellites_visible && i < data->satellites_visible; i++) {
            GPSSatellite sat;
            sat.id = data->skyview[i].PRN;
            sat.elevation = data->skyview[i].elevation;
            sat.azimuth = data->skyview[i].azimuth;
            sat.signalStrength = data->skyview[i].ss;
            sat.used = data->skyview[i].used;
            
            gpsData.satelliteInfo.push_back(sat);
        }
    }
    
    return gpsData;
#else
    (void)data;  // Avoid unused parameter warning
    return GPSData();
#endif
}

bool GPSDDevice::openPPSDevice() {
    // Close existing device if open
    closePPSDevice();
    
    if (ppsPinNumber_ < 0) {
        std::cerr << "No PPS GPIO pin specified" << std::endl;
        return false;
    }
    
    // Construct PPS device path (system dependent)
    std::string ppsDevicePath = "/dev/pps" + std::to_string(ppsPinNumber_);
    
    // Open PPS device
    ppsFileDescriptor_ = ::open(ppsDevicePath.c_str(), O_RDONLY);
    if (ppsFileDescriptor_ < 0) {
        std::cerr << "Failed to open PPS device " << ppsDevicePath << ": " 
                 << strerror(errno) << std::endl;
        return false;
    }
    
    return true;
}

void GPSDDevice::closePPSDevice() {
    if (ppsFileDescriptor_ >= 0) {
        ::close(ppsFileDescriptor_);
        ppsFileDescriptor_ = -1;
    }
}

bool GPSDDevice::parseServerAddress(const std::string& server) {
    // Default values
    serverHost_ = "localhost";
    serverPort_ = 2947;
    
    // Parse host:port format
    size_t colonPos = server.find(':');
    if (colonPos != std::string::npos) {
        serverHost_ = server.substr(0, colonPos);
        
        try {
            serverPort_ = std::stoi(server.substr(colonPos + 1));
        } catch (const std::exception& e) {
            std::cerr << "Invalid port number: " << server.substr(colonPos + 1) << std::endl;
            return false;
        }
    } else {
        // Just host name
        serverHost_ = server;
    }
    
    return true;
}

} // namespace devices
} // namespace tdoa 