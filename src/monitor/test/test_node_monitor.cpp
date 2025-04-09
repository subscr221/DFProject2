#include "../node_monitor.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include <iomanip>

using namespace tdoa::monitor;

// Helper function to print a track point
void printNodeInfo(const NodeInfo& info) {
    std::cout << "Node ID: " << info.id << std::endl;
    std::cout << "  Name: " << info.config.name << std::endl;
    std::cout << "  Type: " << static_cast<int>(info.config.type) << std::endl;
    std::cout << "  Status: " << static_cast<int>(info.status) << std::endl;
    std::cout << "  Metrics:" << std::endl;
    std::cout << "    CPU Usage: " << std::fixed << std::setprecision(2) << info.metrics.cpuUsage << "%" << std::endl;
    std::cout << "    Memory Usage: " << info.metrics.memoryUsage << "%" << std::endl;
    std::cout << "    Active Signals: " << info.metrics.activeSignals << std::endl;
    std::cout << "  Health:" << std::endl;
    std::cout << "    Healthy: " << (info.health.healthy ? "Yes" : "No") << std::endl;
    if (!info.health.issues.empty()) {
        std::cout << "    Issues:" << std::endl;
        for (const auto& issue : info.health.issues) {
            std::cout << "      - " << issue << std::endl;
        }
    }
    std::cout << std::endl;
}

// Helper function to print system metrics
void printSystemMetrics(const SystemMetrics& metrics) {
    std::cout << "System Metrics:" << std::endl;
    std::cout << "  Total Nodes: " << metrics.totalNodes << std::endl;
    std::cout << "  Active Nodes: " << metrics.activeNodes << std::endl;
    std::cout << "  Average CPU Usage: " << std::fixed << std::setprecision(2) << metrics.averageCpuUsage << "%" << std::endl;
    std::cout << "  Average Memory Usage: " << metrics.averageMemoryUsage << "%" << std::endl;
    std::cout << "  Total Network Throughput: " << metrics.totalNetworkThroughput << " MB/s" << std::endl;
    std::cout << "  Total Active Signals: " << metrics.totalActiveSignals << std::endl;
    std::cout << "  Total Queued Tasks: " << metrics.totalQueuedTasks << std::endl;
    std::cout << std::endl;
}

// Event callback for testing
void onNodeEvent(const NodeEvent& event) {
    std::cout << "Event Received:" << std::endl;
    std::cout << "  Node ID: " << event.nodeId << std::endl;
    std::cout << "  Type: " << static_cast<int>(event.type) << std::endl;
    std::cout << "  Data: " << event.data.dump(2) << std::endl;
    std::cout << std::endl;
}

int main() {
    try {
        std::cout << "Starting Node Monitor Tests..." << std::endl;
        std::cout << "==============================" << std::endl << std::endl;

        // Create and initialize monitor
        NodeMonitor monitor;
        assert(monitor.initialize());

        // Register event callback
        monitor.registerEventCallback(onNodeEvent);

        // Test 1: Node Registration
        std::cout << "Test 1: Node Registration" << std::endl;
        std::cout << "------------------------" << std::endl;

        NodeConfig config1;
        config1.name = "TestNode1";
        config1.type = NodeType::DETECTOR;
        config1.version = "1.0.0";
        config1.address = "localhost";
        config1.port = 8080;
        config1.parameters = {{"mode", "active"}, {"sensitivity", 0.8}};

        std::string nodeId1 = monitor.registerNode(config1);
        assert(!nodeId1.empty());
        std::cout << "Registered node with ID: " << nodeId1 << std::endl << std::endl;

        // Test 2: Update Node Status
        std::cout << "Test 2: Update Node Status" << std::endl;
        std::cout << "-------------------------" << std::endl;

        assert(monitor.updateNodeStatus(nodeId1, NodeStatus::ONLINE));
        auto nodeInfo = monitor.getNodeInfo(nodeId1);
        assert(nodeInfo);
        printNodeInfo(*nodeInfo);

        // Test 3: Update Node Metrics
        std::cout << "Test 3: Update Node Metrics" << std::endl;
        std::cout << "-------------------------" << std::endl;

        NodeMetrics metrics;
        metrics.timestamp = std::chrono::system_clock::now();
        metrics.cpuUsage = 75.5;
        metrics.memoryUsage = 82.3;
        metrics.diskUsage = 65.0;
        metrics.networkThroughput = 150.5;
        metrics.signalProcessingLoad = 88.7;
        metrics.activeSignals = 42;
        metrics.queuedTasks = 15;

        assert(monitor.updateNodeMetrics(nodeId1, metrics));
        nodeInfo = monitor.getNodeInfo(nodeId1);
        assert(nodeInfo);
        printNodeInfo(*nodeInfo);

        // Test 4: Register Multiple Nodes
        std::cout << "Test 4: Register Multiple Nodes" << std::endl;
        std::cout << "-----------------------------" << std::endl;

        NodeConfig config2;
        config2.name = "TestNode2";
        config2.type = NodeType::ANALYZER;
        config2.version = "1.0.0";
        config2.address = "localhost";
        config2.port = 8081;

        std::string nodeId2 = monitor.registerNode(config2);
        assert(!nodeId2.empty());
        std::cout << "Registered second node with ID: " << nodeId2 << std::endl;

        assert(monitor.updateNodeStatus(nodeId2, NodeStatus::ONLINE));
        
        NodeMetrics metrics2;
        metrics2.cpuUsage = 45.2;
        metrics2.memoryUsage = 38.7;
        metrics2.activeSignals = 28;
        assert(monitor.updateNodeMetrics(nodeId2, metrics2));

        auto systemMetrics = monitor.getSystemMetrics();
        printSystemMetrics(systemMetrics);

        // Test 5: Node Maintenance
        std::cout << "Test 5: Node Maintenance" << std::endl;
        std::cout << "----------------------" << std::endl;

        assert(monitor.startMaintenance(nodeId1));
        nodeInfo = monitor.getNodeInfo(nodeId1);
        assert(nodeInfo);
        std::cout << "Node1 status after maintenance start:" << std::endl;
        printNodeInfo(*nodeInfo);

        assert(monitor.endMaintenance(nodeId1));
        nodeInfo = monitor.getNodeInfo(nodeId1);
        assert(nodeInfo);
        std::cout << "Node1 status after maintenance end:" << std::endl;
        printNodeInfo(*nodeInfo);

        // Test 6: Export Monitoring Data
        std::cout << "Test 6: Export Monitoring Data" << std::endl;
        std::cout << "----------------------------" << std::endl;

        std::string exportData = monitor.exportMonitoringData();
        std::cout << "Exported monitoring data:" << std::endl;
        std::cout << exportData << std::endl << std::endl;

        // Test 7: Node Unregistration
        std::cout << "Test 7: Node Unregistration" << std::endl;
        std::cout << "--------------------------" << std::endl;

        assert(monitor.unregisterNode(nodeId1));
        assert(!monitor.getNodeInfo(nodeId1));
        std::cout << "Successfully unregistered node: " << nodeId1 << std::endl;

        systemMetrics = monitor.getSystemMetrics();
        printSystemMetrics(systemMetrics);

        std::cout << "All tests completed successfully!" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 