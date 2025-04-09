/**
 * @file gpsd_device.h
 * @brief GPSD implementation of the GPS device interface
 */

#pragma once

#include "gps_device.h"
#include <mutex>
#include <thread>
#include <atomic>
#include <deque>
#include <condition_variable>

// Forward declare GPSD structures to avoid including gpsd headers directly
struct gps_data_t;

namespace tdoa {
namespace devices {

/**
 * @class GPSDDevice
 * @brief Implementation of GPS device interface using GPSD
 * 
 * This class interfaces with the GPSD daemon for obtaining GPS data
 * and handles PPS signal capture for precise timing.
 */
class GPSDDevice : public GPSDevice {
public:
    /**
     * @brief Constructor
     */
    GPSDDevice();
    
    /**
     * @brief Destructor
     */
    ~GPSDDevice() override;
    
    /**
     * @brief Open GPS device connection
     * @param port GPSD server address (host:port or just host)
     * @return True if connection successful
     */
    bool open(const std::string& port) override;
    
    /**
     * @brief Close GPS device connection
     * @return True if closed successfully
     */
    bool close() override;
    
    /**
     * @brief Check if device is connected
     * @return True if device is connected
     */
    bool isConnected() const override;
    
    /**
     * @brief Get last received GPS data
     * @return Latest GPS data
     */
    GPSData getLastData() const override;
    
    /**
     * @brief Register callback for GPS data updates
     * @param callback Function to call when new GPS data arrives
     */
    void registerDataCallback(GPSDataCallback callback) override;
    
    /**
     * @brief Register callback for PPS signals
     * @param callback Function to call when PPS signal is detected
     */
    void registerPPSCallback(PPSCallback callback) override;
    
    /**
     * @brief Get the delay between PPS signal and GPS epoch
     * @return Delay in nanoseconds
     */
    double getPPSOffset() const override;
    
    /**
     * @brief Set the GPIO pin used for PPS input
     * @param pin GPIO pin number
     * @return True if successful
     */
    bool setPPSInputPin(int pin) override;
    
    /**
     * @brief Configure the GPS receiver
     * @param configOption Configuration option name
     * @param value Value to set
     * @return True if successful
     */
    bool configure(const std::string& configOption, const std::string& value) override;

private:
    // Private implementation details
    gps_data_t* gpsData_;                 ///< GPSD data structure
    std::string serverHost_;              ///< GPSD server host
    int serverPort_;                      ///< GPSD server port
    int ppsPinNumber_;                    ///< GPIO pin number for PPS input
    double ppsOffset_;                    ///< PPS offset in nanoseconds
    
    std::atomic<bool> running_;           ///< Thread control flag
    std::atomic<bool> connected_;         ///< Connection status
    mutable std::mutex dataMutex_;        ///< Mutex for protecting data access
    GPSData lastGpsData_;                 ///< Last received GPS data
    
    GPSDataCallback dataCallback_;        ///< Callback for GPS data updates
    PPSCallback ppsCallback_;             ///< Callback for PPS events
    
    std::thread gpsdThread_;              ///< Thread for GPSD polling
    std::thread ppsThread_;               ///< Thread for PPS monitoring
    
    int ppsFileDescriptor_;               ///< File descriptor for PPS device
    
    /**
     * @brief GPSD polling thread function
     */
    void gpsdThreadFunc();
    
    /**
     * @brief PPS monitoring thread function
     */
    void ppsThreadFunc();
    
    /**
     * @brief Convert GPSD data to our GPS data structure
     * @param data GPSD data
     * @return Our GPS data structure
     */
    GPSData convertGPSDData(const gps_data_t* data);
    
    /**
     * @brief Open PPS device
     * @return True if successful
     */
    bool openPPSDevice();
    
    /**
     * @brief Close PPS device
     */
    void closePPSDevice();
    
    /**
     * @brief Parse server address string (host:port)
     * @param server Server address string
     * @return True if parsed successfully
     */
    bool parseServerAddress(const std::string& server);
};

} // namespace devices
} // namespace tdoa 