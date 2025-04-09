/**
 * @file time_sync_factory.cpp
 * @brief Implementation of the time synchronization factory function
 */

#include "time_sync_interface.h"
#include "gps_time_sync.h"
#include <stdexcept>

namespace tdoa {
namespace time_sync {

/**
 * @brief Factory function to create time synchronization instance
 * @param sourceType Synchronization source type
 * @return Unique pointer to time synchronization interface
 */
std::unique_ptr<TimeSync> createTimeSync(SyncSource sourceType) {
    switch (sourceType) {
        case SyncSource::GPS:
            return std::make_unique<GPSTimeSync>();
            
        case SyncSource::PTP:
            // Not implemented yet
            throw std::runtime_error("PTP time synchronization not implemented");
            
        case SyncSource::NTP:
            // Not implemented yet
            throw std::runtime_error("NTP time synchronization not implemented");
            
        case SyncSource::LocalOscillator:
            // Not implemented yet
            throw std::runtime_error("Local oscillator time synchronization not implemented");
            
        case SyncSource::Manual:
            // Not implemented yet
            throw std::runtime_error("Manual time synchronization not implemented");
            
        case SyncSource::None:
        default:
            throw std::runtime_error("Invalid time synchronization source");
    }
}

} // namespace time_sync
} // namespace tdoa 