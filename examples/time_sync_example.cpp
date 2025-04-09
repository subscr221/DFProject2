/**
 * @file time_sync_example.cpp
 * @brief Example application demonstrating time synchronization and reference protocol
 */

#include "../src/time_sync/time_sync_interface.h"
#include "../src/time_sync/gps_time_sync.h"
#include "../src/time_sync/time_reference_protocol.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <ctime>

using namespace tdoa::time_sync;

// Global variables for signal handling
std::atomic<bool> running{true};

// Signal handler
void signalHandler(int signal) {
    std::cout << "\nCaught signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

// Format time with nanoseconds
std::string formatTime(uint64_t timestamp) {
    // Split timestamp into seconds and nanoseconds
    time_t seconds = timestamp / 1000000000ULL;
    uint32_t nanos = timestamp % 1000000000ULL;
    
    // Convert to local time structure
    struct tm timeinfo;
    localtime_r(&seconds, &timeinfo);
    
    // Format time with nanoseconds
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
    
    // Add nanoseconds
    char result[48];
    std::snprintf(result, sizeof(result), "%s.%09u", buffer, nanos);
    
    return std::string(result);
}

// Format sync status
std::string formatSyncStatus(SyncStatus status) {
    switch (status) {
        case SyncStatus::Unknown:       return "Unknown";
        case SyncStatus::Unsynchronized: return "Unsynchronized";
        case SyncStatus::Acquiring:      return "Acquiring";
        case SyncStatus::Synchronized:   return "Synchronized";
        case SyncStatus::Holdover:       return "Holdover";
        case SyncStatus::Error:          return "Error";
        default:                        return "Invalid";
    }
}

// Main application
int main(int argc, char** argv) {
    // Register signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Parse command line arguments
    std::string nodeId = "node1";
    std::string gpsDevice = "GPSD:localhost";
    uint16_t port = 7777;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--node-id" && i + 1 < argc) {
            nodeId = argv[++i];
        } else if (arg == "--gps" && i + 1 < argc) {
            gpsDevice = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --node-id ID       Set node ID (default: node1)" << std::endl;
            std::cout << "  --gps DEVICE       Set GPS device (default: GPSD:localhost)" << std::endl;
            std::cout << "  --port PORT        Set UDP port (default: 7777)" << std::endl;
            std::cout << "  --help             Show this help message" << std::endl;
            return 0;
        }
    }
    
    std::cout << "Starting time synchronization example" << std::endl;
    std::cout << "Node ID: " << nodeId << std::endl;
    std::cout << "GPS device: " << gpsDevice << std::endl;
    std::cout << "UDP port: " << port << std::endl;
    
    try {
        // Create GPS time synchronization instance
        auto timeSync = std::make_shared<GPSTimeSync>();
        
        // Initialize GPS device
        if (!timeSync->initialize(gpsDevice)) {
            std::cerr << "Failed to initialize GPS time synchronization" << std::endl;
            return 1;
        }
        
        // Register event callback
        timeSync->registerEventCallback([](const TimeReference& ref, const std::string& event) {
            std::cout << "Event: " << event << std::endl;
            std::cout << "  Time: " << formatTime(ref.nanoseconds) << std::endl;
            std::cout << "  Uncertainty: " << ref.uncertainty << " ns" << std::endl;
            std::cout << "  Status: " << formatSyncStatus(ref.status) << std::endl;
            std::cout << std::endl;
        });
        
        // Start time synchronization
        if (!timeSync->start()) {
            std::cerr << "Failed to start GPS time synchronization" << std::endl;
            return 1;
        }
        
        // Create time reference protocol instance
        TimeReferenceProtocol protocol(*timeSync);
        
        // Create UDP transport
        auto transport = createUDPTransport(port);
        
        // Initialize protocol
        if (!protocol.initialize(nodeId, transport)) {
            std::cerr << "Failed to initialize time reference protocol" << std::endl;
            return 1;
        }
        
        // Register alert callback
        protocol.registerAlertCallback([](const std::string& nodeId, const std::string& message) {
            std::cout << "Alert from " << nodeId << ": " << message << std::endl;
        });
        
        // Start protocol
        if (!protocol.start()) {
            std::cerr << "Failed to start time reference protocol" << std::endl;
            return 1;
        }
        
        std::cout << "Time synchronization started" << std::endl;
        std::cout << "Press Ctrl+C to exit" << std::endl;
        std::cout << std::endl;
        
        // Main loop
        while (running) {
            // Get time reference
            TimeReference ref = timeSync->getTimeReference();
            
            // Get statistics
            SyncStatistics stats = timeSync->getStatistics();
            
            // Get protocol statistics
            ProtocolStatistics protoStats = protocol.getStatistics();
            
            // Get node statuses
            auto nodeStatuses = protocol.getNodeStatuses();
            
            // Print current time
            std::cout << "\033[2J\033[1;1H";  // Clear screen
            std::cout << "=== Time Synchronization Example ===\n" << std::endl;
            std::cout << "Node ID: " << nodeId << std::endl;
            std::cout << "Time: " << formatTime(ref.nanoseconds) << std::endl;
            std::cout << "Status: " << formatSyncStatus(ref.status) << std::endl;
            std::cout << "Uncertainty: " << std::fixed << std::setprecision(3) << ref.uncertainty << " ns" << std::endl;
            std::cout << "Drift rate: " << std::fixed << std::setprecision(3) << stats.driftRate << " ppb" << std::endl;
            std::cout << "Allan deviation: " << std::scientific << std::setprecision(3) << stats.allanDeviation << std::endl;
            std::cout << std::endl;
            
            // Print protocol statistics
            std::cout << "--- Protocol Statistics ---" << std::endl;
            std::cout << "Messages sent: " << protoStats.messagesSent << std::endl;
            std::cout << "Messages received: " << protoStats.messagesReceived << std::endl;
            std::cout << "Time references sent: " << protoStats.timeReferencesSent << std::endl;
            std::cout << "Time references received: " << protoStats.timeReferencesReceived << std::endl;
            std::cout << std::endl;
            
            // Print known nodes
            std::cout << "--- Known Nodes ---" << std::endl;
            if (nodeStatuses.empty()) {
                std::cout << "No nodes discovered yet" << std::endl;
            } else {
                for (const auto& pair : nodeStatuses) {
                    if (pair.first == nodeId) {
                        continue;  // Skip local node
                    }
                    
                    const NodeStatus& status = pair.second;
                    double timeDiff = protocol.getTimeDifference(pair.first);
                    
                    std::cout << "Node: " << pair.first << std::endl;
                    std::cout << "  Status: " << formatSyncStatus(status.syncStatus) << std::endl;
                    std::cout << "  Uncertainty: " << std::fixed << std::setprecision(3) 
                              << status.uncertaintyNs << " ns" << std::endl;
                    std::cout << "  Time difference: " << std::fixed << std::setprecision(3) 
                              << timeDiff << " ns" << std::endl;
                    std::cout << std::endl;
                }
            }
            
            // Sleep
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        std::cout << "Shutting down..." << std::endl;
        
        // Stop protocol
        protocol.stop();
        
        // Stop time synchronization
        timeSync->stop();
        
        std::cout << "Done" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 