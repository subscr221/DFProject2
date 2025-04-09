/**
 * @file signal_source_factory.cpp
 * @brief Implementation of the signal source factory
 */

#include "signal_source_factory.h"
#include "signalhound/bb60c_abstract_device.h"
#include <algorithm>
#include <cctype>

namespace tdoa {
namespace devices {

/**
 * @brief Register known device types and their creators
 */
void SignalSourceFactory::registerDeviceTypes() {
    // Register BB60C device type
    creators_[DeviceType::BB60C] = []() {
        return std::make_unique<BB60CAbstractDevice>();
    };
    
    // Register type names
    typeNames_[DeviceType::BB60C] = "BB60C";
    typeNames_[DeviceType::Unknown] = "Unknown";
}

} // namespace devices
} // namespace tdoa 