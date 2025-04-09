#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <functional>
#include <chrono>
#include <optional>
#include <nlohmann/json.hpp>

namespace tdoa {
namespace monitor {

/**
 * @brief Node status enumeration
 */
enum class NodeStatus {
    ONLINE,
    OFFLINE,
    DEGRADED,
    MAINTENANCE,
    ERROR
};

/**
 * @brief Node type enumeration
 */
enum class NodeType {
    DETECTOR,
    ANALYZER,
    CLASSIFIER,
    TRACKER,
    CONTROLLER
};

/**
 * @brief Performance metrics for a node
 */
struct NodeMetrics {
    double cpuUsage;                  ///< CPU usage percentage
    double memoryUsage;               ///< Memory usage percentage
    double diskUsage;                 ///< Disk usage percentage
    double networkThroughput;         ///< Network throughput in MB/s
    double signalProcessingLoad;      ///< Signal processing load percentage
    size_t activeSignals;            ///< Number of active signals being processed
    size_t queuedTasks;              ///< Number of tasks in queue
    std::chrono::system_clock::time_point timestamp;  ///< When metrics were collected
};

/**
 * @brief Node health information
 */
struct NodeHealth {
    bool healthy;                     ///< Overall health status
    std::vector<std::string> issues;  ///< List of current issues
    std::map<std::string, double> thresholds;  ///< Health check thresholds
    std::chrono::system_clock::time_point lastCheck;  ///< Last health check time
};

/**
 * @brief Node configuration
 */
struct NodeConfig {
    std::string name;                 ///< Node name
    NodeType type;                    ///< Node type
    std::string version;             ///< Software version
    std::string address;             ///< Network address
    uint16_t port;                   ///< Network port
    std::map<std::string, std::string> parameters;  ///< Configuration parameters
};

/**
 * @brief Node information
 */
struct NodeInfo {
    std::string id;                  ///< Unique node identifier
    NodeConfig config;               ///< Node configuration
    NodeStatus status;               ///< Current status
    NodeMetrics metrics;             ///< Current performance metrics
    NodeHealth health;               ///< Current health information
    std::chrono::system_clock::time_point lastSeen;  ///< Last contact time
};

/**
 * @brief System-wide metrics
 */
struct SystemMetrics {
    size_t totalNodes;               ///< Total number of nodes
    size_t activeNodes;              ///< Number of active nodes
    double averageCpuUsage;          ///< Average CPU usage across nodes
    double averageMemoryUsage;       ///< Average memory usage across nodes
    double totalNetworkThroughput;   ///< Total network throughput
    size_t totalActiveSignals;       ///< Total active signals being processed
    size_t totalQueuedTasks;         ///< Total queued tasks
    std::chrono::system_clock::time_point timestamp;  ///< When metrics were collected
};

/**
 * @brief Node event types
 */
enum class NodeEventType {
    STATUS_CHANGE,
    METRICS_UPDATE,
    HEALTH_ALERT,
    CONFIG_CHANGE,
    CONNECTION_CHANGE
};

/**
 * @brief Node event information
 */
struct NodeEvent {
    std::string nodeId;              ///< ID of the node
    NodeEventType type;              ///< Type of event
    nlohmann::json data;             ///< Event data
    std::chrono::system_clock::time_point timestamp;  ///< When event occurred
};

/**
 * @brief Callback type for node events
 */
using NodeEventCallback = std::function<void(const NodeEvent&)>;

/**
 * @brief Node monitor class
 * 
 * Provides monitoring and management capabilities for system nodes,
 * including health checks, performance monitoring, and remote control.
 */
class NodeMonitor {
public:
    /**
     * @brief Constructor
     */
    NodeMonitor();
    
    /**
     * @brief Initialize the node monitor
     * @return true if initialization successful, false otherwise
     */
    bool initialize();
    
    /**
     * @brief Register a new node
     * @param config Node configuration
     * @return Node ID if successful, empty string otherwise
     */
    std::string registerNode(const NodeConfig& config);
    
    /**
     * @brief Unregister a node
     * @param nodeId Node identifier
     * @return true if successful, false otherwise
     */
    bool unregisterNode(const std::string& nodeId);
    
    /**
     * @brief Update node status
     * @param nodeId Node identifier
     * @param status New status
     * @return true if successful, false otherwise
     */
    bool updateNodeStatus(const std::string& nodeId, NodeStatus status);
    
    /**
     * @brief Update node metrics
     * @param nodeId Node identifier
     * @param metrics New metrics
     * @return true if successful, false otherwise
     */
    bool updateNodeMetrics(const std::string& nodeId, const NodeMetrics& metrics);
    
    /**
     * @brief Update node health
     * @param nodeId Node identifier
     * @param health New health information
     * @return true if successful, false otherwise
     */
    bool updateNodeHealth(const std::string& nodeId, const NodeHealth& health);
    
    /**
     * @brief Get node information
     * @param nodeId Node identifier
     * @return Optional containing node info if found
     */
    std::optional<NodeInfo> getNodeInfo(const std::string& nodeId) const;
    
    /**
     * @brief List all nodes
     * @return Vector of node information
     */
    std::vector<NodeInfo> listNodes() const;
    
    /**
     * @brief Get nodes by type
     * @param type Node type
     * @return Vector of node information
     */
    std::vector<NodeInfo> getNodesByType(NodeType type) const;
    
    /**
     * @brief Get nodes by status
     * @param status Node status
     * @return Vector of node information
     */
    std::vector<NodeInfo> getNodesByStatus(NodeStatus status) const;
    
    /**
     * @brief Get system-wide metrics
     * @return Current system metrics
     */
    SystemMetrics getSystemMetrics() const;
    
    /**
     * @brief Send command to node
     * @param nodeId Node identifier
     * @param command Command to send
     * @param parameters Command parameters
     * @return true if successful, false otherwise
     */
    bool sendNodeCommand(const std::string& nodeId,
                        const std::string& command,
                        const nlohmann::json& parameters);
    
    /**
     * @brief Register event callback
     * @param callback Function to call when events occur
     * @return true if successful, false otherwise
     */
    bool registerEventCallback(NodeEventCallback callback);
    
    /**
     * @brief Start maintenance mode for node
     * @param nodeId Node identifier
     * @return true if successful, false otherwise
     */
    bool startMaintenance(const std::string& nodeId);
    
    /**
     * @brief End maintenance mode for node
     * @param nodeId Node identifier
     * @return true if successful, false otherwise
     */
    bool endMaintenance(const std::string& nodeId);
    
    /**
     * @brief Export monitoring data
     * @return JSON string containing monitoring data
     */
    std::string exportMonitoringData() const;
    
private:
    std::map<std::string, NodeInfo> nodes_;
    std::vector<NodeEventCallback> eventCallbacks_;
    mutable std::mutex mutex_;
    
    void notifyEvent(const NodeEvent& event);
    std::string generateNodeId() const;
    void updateSystemMetrics();
    bool validateNodeConfig(const NodeConfig& config) const;
    bool checkNodeExists(const std::string& nodeId) const;
};

} // namespace monitor
} // namespace tdoa 