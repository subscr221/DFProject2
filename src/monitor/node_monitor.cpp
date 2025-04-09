#include "node_monitor.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>

namespace tdoa {
namespace monitor {

namespace {
// Helper function to generate a random string
std::string generateRandomString(size_t length) {
    static const std::string chars =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, chars.size() - 1);
    
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += chars[dis(gen)];
    }
    return result;
}

// Helper function to get current timestamp
std::chrono::system_clock::time_point getCurrentTimestamp() {
    return std::chrono::system_clock::now();
}

} // anonymous namespace

NodeMonitor::NodeMonitor() {
}

bool NodeMonitor::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Initialize any required resources
        nodes_.clear();
        eventCallbacks_.clear();
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

std::string NodeMonitor::registerNode(const NodeConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Validate node configuration
        if (!validateNodeConfig(config)) {
            return "";
        }
        
        // Generate unique node ID
        std::string nodeId = generateNodeId();
        
        // Create node info
        NodeInfo info;
        info.id = nodeId;
        info.config = config;
        info.status = NodeStatus::OFFLINE;
        info.lastSeen = getCurrentTimestamp();
        
        // Initialize metrics
        info.metrics = NodeMetrics{};
        info.metrics.timestamp = info.lastSeen;
        
        // Initialize health
        info.health.healthy = true;
        info.health.lastCheck = info.lastSeen;
        
        // Store node
        nodes_[nodeId] = info;
        
        // Notify event listeners
        NodeEvent event;
        event.nodeId = nodeId;
        event.type = NodeEventType::CONNECTION_CHANGE;
        event.data = {
            {"action", "register"},
            {"config", {
                {"name", config.name},
                {"type", static_cast<int>(config.type)},
                {"version", config.version},
                {"address", config.address},
                {"port", config.port}
            }}
        };
        event.timestamp = getCurrentTimestamp();
        notifyEvent(event);
        
        // Update system metrics
        updateSystemMetrics();
        
        return nodeId;
    }
    catch (const std::exception& e) {
        return "";
    }
}

bool NodeMonitor::unregisterNode(const std::string& nodeId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        if (!checkNodeExists(nodeId)) {
            return false;
        }
        
        // Remove node
        nodes_.erase(nodeId);
        
        // Notify event listeners
        NodeEvent event;
        event.nodeId = nodeId;
        event.type = NodeEventType::CONNECTION_CHANGE;
        event.data = {{"action", "unregister"}};
        event.timestamp = getCurrentTimestamp();
        notifyEvent(event);
        
        // Update system metrics
        updateSystemMetrics();
        
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

bool NodeMonitor::updateNodeStatus(const std::string& nodeId, NodeStatus status) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        if (!checkNodeExists(nodeId)) {
            return false;
        }
        
        auto& node = nodes_[nodeId];
        
        // Update status if changed
        if (node.status != status) {
            NodeStatus oldStatus = node.status;
            node.status = status;
            node.lastSeen = getCurrentTimestamp();
            
            // Notify event listeners
            NodeEvent event;
            event.nodeId = nodeId;
            event.type = NodeEventType::STATUS_CHANGE;
            event.data = {
                {"oldStatus", static_cast<int>(oldStatus)},
                {"newStatus", static_cast<int>(status)}
            };
            event.timestamp = getCurrentTimestamp();
            notifyEvent(event);
            
            // Update system metrics
            updateSystemMetrics();
        }
        
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

bool NodeMonitor::updateNodeMetrics(const std::string& nodeId, const NodeMetrics& metrics) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        if (!checkNodeExists(nodeId)) {
            return false;
        }
        
        auto& node = nodes_[nodeId];
        
        // Update metrics
        node.metrics = metrics;
        node.lastSeen = getCurrentTimestamp();
        
        // Check for health issues based on metrics
        std::vector<std::string> issues;
        
        if (metrics.cpuUsage > 90.0) {
            issues.push_back("High CPU usage");
        }
        if (metrics.memoryUsage > 90.0) {
            issues.push_back("High memory usage");
        }
        if (metrics.diskUsage > 90.0) {
            issues.push_back("High disk usage");
        }
        if (metrics.signalProcessingLoad > 90.0) {
            issues.push_back("High signal processing load");
        }
        
        // Update health if issues found
        if (!issues.empty()) {
            node.health.healthy = false;
            node.health.issues = std::move(issues);
            node.health.lastCheck = getCurrentTimestamp();
            
            // Notify health alert
            NodeEvent healthEvent;
            healthEvent.nodeId = nodeId;
            healthEvent.type = NodeEventType::HEALTH_ALERT;
            healthEvent.data = {{"issues", node.health.issues}};
            healthEvent.timestamp = getCurrentTimestamp();
            notifyEvent(healthEvent);
        }
        
        // Notify metrics update
        NodeEvent metricsEvent;
        metricsEvent.nodeId = nodeId;
        metricsEvent.type = NodeEventType::METRICS_UPDATE;
        metricsEvent.data = {
            {"cpuUsage", metrics.cpuUsage},
            {"memoryUsage", metrics.memoryUsage},
            {"diskUsage", metrics.diskUsage},
            {"networkThroughput", metrics.networkThroughput},
            {"signalProcessingLoad", metrics.signalProcessingLoad},
            {"activeSignals", metrics.activeSignals},
            {"queuedTasks", metrics.queuedTasks}
        };
        metricsEvent.timestamp = getCurrentTimestamp();
        notifyEvent(metricsEvent);
        
        // Update system metrics
        updateSystemMetrics();
        
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

bool NodeMonitor::updateNodeHealth(const std::string& nodeId, const NodeHealth& health) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        if (!checkNodeExists(nodeId)) {
            return false;
        }
        
        auto& node = nodes_[nodeId];
        
        // Update health
        node.health = health;
        node.lastSeen = getCurrentTimestamp();
        
        // Notify health alert if not healthy
        if (!health.healthy) {
            NodeEvent event;
            event.nodeId = nodeId;
            event.type = NodeEventType::HEALTH_ALERT;
            event.data = {{"issues", health.issues}};
            event.timestamp = getCurrentTimestamp();
            notifyEvent(event);
        }
        
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

std::optional<NodeInfo> NodeMonitor::getNodeInfo(const std::string& nodeId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto it = nodes_.find(nodeId);
        if (it == nodes_.end()) {
            return std::nullopt;
        }
        
        return it->second;
    }
    catch (const std::exception& e) {
        return std::nullopt;
    }
}

std::vector<NodeInfo> NodeMonitor::listNodes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<NodeInfo> result;
    result.reserve(nodes_.size());
    
    for (const auto& [_, node] : nodes_) {
        result.push_back(node);
    }
    
    return result;
}

std::vector<NodeInfo> NodeMonitor::getNodesByType(NodeType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<NodeInfo> result;
    
    for (const auto& [_, node] : nodes_) {
        if (node.config.type == type) {
            result.push_back(node);
        }
    }
    
    return result;
}

std::vector<NodeInfo> NodeMonitor::getNodesByStatus(NodeStatus status) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<NodeInfo> result;
    
    for (const auto& [_, node] : nodes_) {
        if (node.status == status) {
            result.push_back(node);
        }
    }
    
    return result;
}

SystemMetrics NodeMonitor::getSystemMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    SystemMetrics metrics;
    metrics.timestamp = getCurrentTimestamp();
    metrics.totalNodes = nodes_.size();
    
    // Calculate system-wide metrics
    metrics.activeNodes = 0;
    double totalCpuUsage = 0.0;
    double totalMemoryUsage = 0.0;
    metrics.totalNetworkThroughput = 0.0;
    metrics.totalActiveSignals = 0;
    metrics.totalQueuedTasks = 0;
    
    for (const auto& [_, node] : nodes_) {
        if (node.status == NodeStatus::ONLINE) {
            metrics.activeNodes++;
            totalCpuUsage += node.metrics.cpuUsage;
            totalMemoryUsage += node.metrics.memoryUsage;
            metrics.totalNetworkThroughput += node.metrics.networkThroughput;
            metrics.totalActiveSignals += node.metrics.activeSignals;
            metrics.totalQueuedTasks += node.metrics.queuedTasks;
        }
    }
    
    // Calculate averages
    if (metrics.activeNodes > 0) {
        metrics.averageCpuUsage = totalCpuUsage / metrics.activeNodes;
        metrics.averageMemoryUsage = totalMemoryUsage / metrics.activeNodes;
    } else {
        metrics.averageCpuUsage = 0.0;
        metrics.averageMemoryUsage = 0.0;
    }
    
    return metrics;
}

bool NodeMonitor::sendNodeCommand(const std::string& nodeId,
                                const std::string& command,
                                const nlohmann::json& parameters) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        if (!checkNodeExists(nodeId)) {
            return false;
        }
        
        auto& node = nodes_[nodeId];
        
        // Check if node is online
        if (node.status != NodeStatus::ONLINE) {
            return false;
        }
        
        // TODO: Implement actual command sending logic
        // For now, just notify event listeners
        NodeEvent event;
        event.nodeId = nodeId;
        event.type = NodeEventType::CONFIG_CHANGE;
        event.data = {
            {"command", command},
            {"parameters", parameters}
        };
        event.timestamp = getCurrentTimestamp();
        notifyEvent(event);
        
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

bool NodeMonitor::registerEventCallback(NodeEventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        eventCallbacks_.push_back(callback);
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

bool NodeMonitor::startMaintenance(const std::string& nodeId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        if (!checkNodeExists(nodeId)) {
            return false;
        }
        
        auto& node = nodes_[nodeId];
        
        // Check if already in maintenance
        if (node.status == NodeStatus::MAINTENANCE) {
            return true;
        }
        
        // Update status
        NodeStatus oldStatus = node.status;
        node.status = NodeStatus::MAINTENANCE;
        
        // Notify event listeners
        NodeEvent event;
        event.nodeId = nodeId;
        event.type = NodeEventType::STATUS_CHANGE;
        event.data = {
            {"oldStatus", static_cast<int>(oldStatus)},
            {"newStatus", static_cast<int>(NodeStatus::MAINTENANCE)},
            {"maintenance", true}
        };
        event.timestamp = getCurrentTimestamp();
        notifyEvent(event);
        
        // Update system metrics
        updateSystemMetrics();
        
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

bool NodeMonitor::endMaintenance(const std::string& nodeId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        if (!checkNodeExists(nodeId)) {
            return false;
        }
        
        auto& node = nodes_[nodeId];
        
        // Check if in maintenance
        if (node.status != NodeStatus::MAINTENANCE) {
            return false;
        }
        
        // Update status
        node.status = NodeStatus::ONLINE;
        
        // Notify event listeners
        NodeEvent event;
        event.nodeId = nodeId;
        event.type = NodeEventType::STATUS_CHANGE;
        event.data = {
            {"oldStatus", static_cast<int>(NodeStatus::MAINTENANCE)},
            {"newStatus", static_cast<int>(NodeStatus::ONLINE)},
            {"maintenance", false}
        };
        event.timestamp = getCurrentTimestamp();
        notifyEvent(event);
        
        // Update system metrics
        updateSystemMetrics();
        
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

std::string NodeMonitor::exportMonitoringData() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        nlohmann::json data;
        
        // Export nodes
        data["nodes"] = nlohmann::json::array();
        for (const auto& [nodeId, node] : nodes_) {
            nlohmann::json nodeData;
            nodeData["id"] = node.id;
            nodeData["config"] = {
                {"name", node.config.name},
                {"type", static_cast<int>(node.config.type)},
                {"version", node.config.version},
                {"address", node.config.address},
                {"port", node.config.port},
                {"parameters", node.config.parameters}
            };
            nodeData["status"] = static_cast<int>(node.status);
            nodeData["metrics"] = {
                {"cpuUsage", node.metrics.cpuUsage},
                {"memoryUsage", node.metrics.memoryUsage},
                {"diskUsage", node.metrics.diskUsage},
                {"networkThroughput", node.metrics.networkThroughput},
                {"signalProcessingLoad", node.metrics.signalProcessingLoad},
                {"activeSignals", node.metrics.activeSignals},
                {"queuedTasks", node.metrics.queuedTasks}
            };
            nodeData["health"] = {
                {"healthy", node.health.healthy},
                {"issues", node.health.issues},
                {"thresholds", node.health.thresholds}
            };
            data["nodes"].push_back(nodeData);
        }
        
        // Export system metrics
        auto systemMetrics = getSystemMetrics();
        data["systemMetrics"] = {
            {"totalNodes", systemMetrics.totalNodes},
            {"activeNodes", systemMetrics.activeNodes},
            {"averageCpuUsage", systemMetrics.averageCpuUsage},
            {"averageMemoryUsage", systemMetrics.averageMemoryUsage},
            {"totalNetworkThroughput", systemMetrics.totalNetworkThroughput},
            {"totalActiveSignals", systemMetrics.totalActiveSignals},
            {"totalQueuedTasks", systemMetrics.totalQueuedTasks}
        };
        
        return data.dump(4);
    }
    catch (const std::exception& e) {
        return "{}";
    }
}

void NodeMonitor::notifyEvent(const NodeEvent& event) {
    for (const auto& callback : eventCallbacks_) {
        try {
            callback(event);
        }
        catch (...) {
            // Ignore callback errors
        }
    }
}

std::string NodeMonitor::generateNodeId() const {
    return generateRandomString(16);
}

void NodeMonitor::updateSystemMetrics() {
    // This is called internally when node data changes
    // System metrics are calculated on-demand in getSystemMetrics()
}

bool NodeMonitor::validateNodeConfig(const NodeConfig& config) const {
    return !config.name.empty() &&
           !config.version.empty() &&
           !config.address.empty() &&
           config.port > 0;
}

bool NodeMonitor::checkNodeExists(const std::string& nodeId) const {
    return nodes_.find(nodeId) != nodes_.end();
}

} // namespace monitor
} // namespace tdoa 