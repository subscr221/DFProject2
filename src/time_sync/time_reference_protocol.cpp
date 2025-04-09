/**
 * @file time_reference_protocol.cpp
 * @brief Implementation of time reference protocol
 */

#include "time_reference_protocol.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace tdoa {
namespace time_sync {

/**
 * @class TimeReferenceProtocol::Impl
 * @brief Implementation details for TimeReferenceProtocol
 */
class TimeReferenceProtocol::Impl {
public:
    /**
     * @brief Constructor
     * @param timeSync Time synchronization instance
     */
    Impl(TimeSync& timeSync)
        : timeSync_(timeSync)
        , running_(false)
        , nodeId_("unknown")
        , nextSequence_(0)
        , ptpFallbackEnabled_(false)
        , consensusActive_(false)
        , localNodeWeight_(1.0)
    {
    }
    
    /**
     * @brief Destructor
     */
    ~Impl() {
        stop();
    }
    
    /**
     * @brief Initialize the protocol
     * @param nodeId Local node ID
     * @param transport Transport interface for message exchange
     * @return True if initialization was successful
     */
    bool initialize(const std::string& nodeId, std::shared_ptr<ProtocolTransport> transport) {
        if (transport == nullptr) {
            std::cerr << "Null transport interface" << std::endl;
            return false;
        }
        
        nodeId_ = nodeId;
        transport_ = transport;
        
        // Register message callback
        transport_->registerMessageCallback([this](const ProtocolMessage& message) {
            handleMessage(message);
        });
        
        // Initialize transport
        if (!transport_->initialize(nodeId)) {
            std::cerr << "Failed to initialize transport" << std::endl;
            return false;
        }
        
        // Initialize statistics
        statistics_ = ProtocolStatistics();
        
        return true;
    }
    
    /**
     * @brief Start the protocol
     * @return True if started successfully
     */
    bool start() {
        if (!transport_) {
            std::cerr << "Transport not initialized" << std::endl;
            return false;
        }
        
        if (running_) {
            return true;  // Already running
        }
        
        // Start transport
        if (!transport_->start()) {
            std::cerr << "Failed to start transport" << std::endl;
            return false;
        }
        
        running_ = true;
        
        // Start protocol threads
        statusThread_ = std::thread(&Impl::statusThreadFunc, this);
        
        return true;
    }
    
    /**
     * @brief Stop the protocol
     * @return True if stopped successfully
     */
    bool stop() {
        if (!running_) {
            return true;  // Already stopped
        }
        
        running_ = false;
        
        // Wait for threads to exit
        if (statusThread_.joinable()) {
            statusThread_.join();
        }
        
        // Stop transport
        if (transport_) {
            transport_->stop();
        }
        
        return true;
    }
    
    /**
     * @brief Register callback for alert messages
     * @param callback Function to call when alert is received
     */
    void registerAlertCallback(AlertCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        alertCallback_ = callback;
    }
    
    /**
     * @brief Get protocol statistics
     * @return Protocol statistics
     */
    ProtocolStatistics getStatistics() const {
        std::lock_guard<std::mutex> lock(statsMutex_);
        return statistics_;
    }
    
    /**
     * @brief Get status of all known nodes
     * @return Map of node IDs to node status
     */
    std::map<std::string, NodeStatus> getNodeStatuses() const {
        std::lock_guard<std::mutex> lock(nodesMutex_);
        return nodeStatuses_;
    }
    
    /**
     * @brief Get status of a specific node
     * @param nodeId Node ID
     * @return Node status (empty nodeId if not found)
     */
    NodeStatus getNodeStatus(const std::string& nodeId) const {
        std::lock_guard<std::mutex> lock(nodesMutex_);
        auto it = nodeStatuses_.find(nodeId);
        if (it == nodeStatuses_.end()) {
            return NodeStatus();  // Return empty status
        }
        
        return it->second;
    }
    
    /**
     * @brief Force a time reference broadcast
     * @return True if broadcast was successful
     */
    bool broadcastTimeReference() {
        if (!running_ || !transport_) {
            return false;
        }
        
        return sendTimeReference();
    }
    
    /**
     * @brief Set synchronization weight for a node
     * @param nodeId Node ID
     * @param weight Synchronization weight (0.0 - 1.0)
     * @return True if weight was set successfully
     */
    bool setNodeWeight(const std::string& nodeId, double weight) {
        if (weight < 0.0 || weight > 1.0) {
            std::cerr << "Invalid weight value: " << weight << std::endl;
            return false;
        }
        
        std::lock_guard<std::mutex> lock(nodesMutex_);
        nodeWeights_[nodeId] = weight;
        
        // If setting weight for local node
        if (nodeId == nodeId_) {
            localNodeWeight_ = weight;
        }
        
        return true;
    }
    
    /**
     * @brief Calculate time difference to a specific node
     * @param nodeId Node ID
     * @return Time difference in nanoseconds (positive if remote is ahead)
     */
    double getTimeDifference(const std::string& nodeId) const {
        std::lock_guard<std::mutex> lock(nodesMutex_);
        
        // Find node time reference
        auto it = nodeTimeReferences_.find(nodeId);
        if (it == nodeTimeReferences_.end()) {
            return 0.0;  // No reference available
        }
        
        // Get local time reference
        TimeReference localRef = timeSync_.getTimeReference();
        
        // Calculate difference
        return timeSync_.calculateTimeDifference(localRef, it->second);
    }
    
    /**
     * @brief Enable or disable PTP fallback
     * @param enable Whether to enable PTP fallback
     * @return True if configuration was successful
     */
    bool enablePTPFallback(bool enable) {
        ptpFallbackEnabled_ = enable;
        return true;
    }
    
    /**
     * @brief Force a consensus round
     * @return True if consensus was started successfully
     */
    bool initiateConsensus() {
        if (!running_ || !transport_) {
            return false;
        }
        
        return startConsensusRound();
    }

private:
    TimeSync& timeSync_;                 ///< Time synchronization instance
    std::shared_ptr<ProtocolTransport> transport_; ///< Transport interface
    
    std::atomic<bool> running_;          ///< Running flag
    std::string nodeId_;                 ///< Local node ID
    
    std::thread statusThread_;           ///< Thread for periodic status updates
    
    mutable std::mutex nodesMutex_;      ///< Mutex for protecting node data
    std::map<std::string, NodeStatus> nodeStatuses_; ///< Status of known nodes
    std::map<std::string, TimeReference> nodeTimeReferences_; ///< Time references from nodes
    std::map<std::string, double> nodeWeights_; ///< Synchronization weights for nodes
    
    mutable std::mutex statsMutex_;      ///< Mutex for protecting statistics
    ProtocolStatistics statistics_;      ///< Protocol statistics
    
    std::atomic<uint32_t> nextSequence_; ///< Next sequence number
    
    AlertCallback alertCallback_;        ///< Alert callback
    mutable std::mutex callbackMutex_;   ///< Mutex for protecting callback
    
    std::atomic<bool> ptpFallbackEnabled_; ///< PTP fallback flag
    std::atomic<bool> consensusActive_;    ///< Consensus active flag
    double localNodeWeight_;               ///< Local node weight
    
    /**
     * @brief Thread function for periodic status updates
     */
    void statusThreadFunc() {
        // Status update interval (5 seconds)
        const auto interval = std::chrono::seconds(5);
        
        // Time reference broadcast interval (1 second)
        const auto refInterval = std::chrono::seconds(1);
        
        auto lastStatusTime = std::chrono::steady_clock::now();
        auto lastRefTime = std::chrono::steady_clock::now();
        
        while (running_) {
            auto now = std::chrono::steady_clock::now();
            
            // Send status update
            if (now - lastStatusTime >= interval) {
                sendStatusUpdate();
                lastStatusTime = now;
            }
            
            // Send time reference
            if (now - lastRefTime >= refInterval) {
                sendTimeReference();
                lastRefTime = now;
            }
            
            // Sleep to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    /**
     * @brief Handle received message
     * @param message Received message
     */
    void handleMessage(const ProtocolMessage& message) {
        if (message.sourceNodeId.empty()) {
            // Invalid message, ignore
            return;
        }
        
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            statistics_.messagesReceived++;
        }
        
        // Handle message based on type
        switch (message.type) {
            case MessageType::TimeReference:
                handleTimeReference(message);
                break;
                
            case MessageType::StatusUpdate:
                handleStatusUpdate(message);
                break;
                
            case MessageType::StatusRequest:
                handleStatusRequest(message);
                break;
                
            case MessageType::StatusResponse:
                handleStatusResponse(message);
                break;
                
            case MessageType::SyncRequest:
                handleSyncRequest(message);
                break;
                
            case MessageType::SyncResponse:
                handleSyncResponse(message);
                break;
                
            case MessageType::ConsensusProposal:
                handleConsensusProposal(message);
                break;
                
            case MessageType::ConsensusVote:
                handleConsensusVote(message);
                break;
                
            case MessageType::Alert:
                handleAlert(message);
                break;
                
            default:
                // Unknown message type, ignore
                {
                    std::lock_guard<std::mutex> lock(statsMutex_);
                    statistics_.messagesRejected++;
                }
                break;
        }
    }
    
    /**
     * @brief Handle time reference message
     * @param message Time reference message
     */
    void handleTimeReference(const ProtocolMessage& message) {
        if (message.payload.size() < sizeof(TimeReference)) {
            // Invalid payload, ignore
            return;
        }
        
        // Extract time reference from payload
        TimeReference ref;
        std::memcpy(&ref, message.payload.data(), sizeof(TimeReference));
        
        // Store reference
        {
            std::lock_guard<std::mutex> lock(nodesMutex_);
            nodeTimeReferences_[message.sourceNodeId] = ref;
        }
        
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            statistics_.timeReferencesReceived++;
        }
    }
    
    /**
     * @brief Handle status update message
     * @param message Status update message
     */
    void handleStatusUpdate(const ProtocolMessage& message) {
        if (message.payload.size() < sizeof(NodeStatus)) {
            // Invalid payload, ignore
            return;
        }
        
        // Extract node status from payload
        NodeStatus status;
        std::memcpy(&status, message.payload.data(), sizeof(NodeStatus));
        
        // Store status
        {
            std::lock_guard<std::mutex> lock(nodesMutex_);
            nodeStatuses_[message.sourceNodeId] = status;
        }
    }
    
    /**
     * @brief Handle status request message
     * @param message Status request message
     */
    void handleStatusRequest(const ProtocolMessage& message) {
        // Send status response to requester
        sendStatusResponse(message.sourceNodeId);
    }
    
    /**
     * @brief Handle status response message
     * @param message Status response message
     */
    void handleStatusResponse(const ProtocolMessage& message) {
        // Same as status update
        handleStatusUpdate(message);
    }
    
    /**
     * @brief Handle synchronization request message
     * @param message Synchronization request message
     */
    void handleSyncRequest(const ProtocolMessage& message) {
        // Send synchronization response to requester
        sendSyncResponse(message.sourceNodeId);
    }
    
    /**
     * @brief Handle synchronization response message
     * @param message Synchronization response message
     */
    void handleSyncResponse(const ProtocolMessage& message) {
        // Same as time reference
        handleTimeReference(message);
    }
    
    /**
     * @brief Handle consensus proposal message
     * @param message Consensus proposal message
     */
    void handleConsensusProposal(const ProtocolMessage& message) {
        if (!consensusActive_) {
            // Start consensus round if not already active
            consensusActive_ = true;
            
            // Process proposal
            // ...
            
            // Vote based on proposal
            sendConsensusVote(message.sourceNodeId);
        }
    }
    
    /**
     * @brief Handle consensus vote message
     * @param message Consensus vote message
     */
    void handleConsensusVote(const ProtocolMessage& message) {
        if (consensusActive_) {
            // Process vote
            // ...
            
            // Check if consensus is reached
            // ...
        }
    }
    
    /**
     * @brief Handle alert message
     * @param message Alert message
     */
    void handleAlert(const ProtocolMessage& message) {
        // Extract alert message from payload
        if (message.payload.empty()) {
            return;
        }
        
        std::string alertMessage(reinterpret_cast<const char*>(message.payload.data()),
                               message.payload.size());
        
        // Call alert callback
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (alertCallback_) {
            alertCallback_(message.sourceNodeId, alertMessage);
        }
        
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            statistics_.alertsReceived++;
        }
    }
    
    /**
     * @brief Send time reference message
     * @return True if message was sent successfully
     */
    bool sendTimeReference() {
        if (!running_ || !transport_) {
            return false;
        }
        
        // Get time reference
        TimeReference ref = timeSync_.getTimeReference();
        
        // Create message
        ProtocolMessage message;
        message.type = MessageType::TimeReference;
        message.destNodeId = "";  // Broadcast
        
        // Add time reference to payload
        message.payload.resize(sizeof(TimeReference));
        std::memcpy(message.payload.data(), &ref, sizeof(TimeReference));
        
        // Send message
        bool result = transport_->sendMessage(message);
        
        // Update statistics
        if (result) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            statistics_.messagesSent++;
            statistics_.timeReferencesSent++;
        }
        
        return result;
    }
    
    /**
     * @brief Send status update message
     * @return True if message was sent successfully
     */
    bool sendStatusUpdate() {
        if (!running_ || !transport_) {
            return false;
        }
        
        // Create node status
        NodeStatus status;
        status.nodeId = nodeId_;
        status.syncStatus = timeSync_.getStatus();
        
        // Get statistics
        SyncStatistics stats = timeSync_.getStatistics();
        status.allanDeviation = stats.allanDeviation;
        status.driftRatePpb = stats.driftRate;
        
        // Set primary sync source
        TimeReference ref = timeSync_.getTimeReference();
        status.primarySyncSource = ref.source;
        status.uncertaintyNs = ref.uncertainty;
        
        // Update timestamp
        status.lastUpdateTime = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        
        // Create message
        ProtocolMessage message;
        message.type = MessageType::StatusUpdate;
        message.destNodeId = "";  // Broadcast
        
        // Add status to payload
        message.payload.resize(sizeof(NodeStatus));
        std::memcpy(message.payload.data(), &status, sizeof(NodeStatus));
        
        // Send message
        bool result = transport_->sendMessage(message);
        
        // Update statistics
        if (result) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            statistics_.messagesSent++;
        }
        
        return result;
    }
    
    /**
     * @brief Send status response message
     * @param destNodeId Destination node ID
     * @return True if message was sent successfully
     */
    bool sendStatusResponse(const std::string& destNodeId) {
        if (!running_ || !transport_) {
            return false;
        }
        
        // Create node status (same as status update)
        NodeStatus status;
        status.nodeId = nodeId_;
        status.syncStatus = timeSync_.getStatus();
        
        // Get statistics
        SyncStatistics stats = timeSync_.getStatistics();
        status.allanDeviation = stats.allanDeviation;
        status.driftRatePpb = stats.driftRate;
        
        // Set primary sync source
        TimeReference ref = timeSync_.getTimeReference();
        status.primarySyncSource = ref.source;
        status.uncertaintyNs = ref.uncertainty;
        
        // Update timestamp
        status.lastUpdateTime = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        
        // Create message
        ProtocolMessage message;
        message.type = MessageType::StatusResponse;
        message.destNodeId = destNodeId;
        
        // Add status to payload
        message.payload.resize(sizeof(NodeStatus));
        std::memcpy(message.payload.data(), &status, sizeof(NodeStatus));
        
        // Send message
        bool result = transport_->sendMessage(message);
        
        // Update statistics
        if (result) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            statistics_.messagesSent++;
        }
        
        return result;
    }
    
    /**
     * @brief Send synchronization response message
     * @param destNodeId Destination node ID
     * @return True if message was sent successfully
     */
    bool sendSyncResponse(const std::string& destNodeId) {
        if (!running_ || !transport_) {
            return false;
        }
        
        // Get time reference
        TimeReference ref = timeSync_.getTimeReference();
        
        // Create message
        ProtocolMessage message;
        message.type = MessageType::SyncResponse;
        message.destNodeId = destNodeId;
        
        // Add time reference to payload
        message.payload.resize(sizeof(TimeReference));
        std::memcpy(message.payload.data(), &ref, sizeof(TimeReference));
        
        // Send message
        bool result = transport_->sendMessage(message);
        
        // Update statistics
        if (result) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            statistics_.messagesSent++;
        }
        
        return result;
    }
    
    /**
     * @brief Send consensus vote message
     * @param destNodeId Destination node ID
     * @return True if message was sent successfully
     */
    bool sendConsensusVote(const std::string& destNodeId) {
        if (!running_ || !transport_) {
            return false;
        }
        
        // Create message
        ProtocolMessage message;
        message.type = MessageType::ConsensusVote;
        message.destNodeId = destNodeId;
        
        // Add vote to payload
        // ...
        
        // Send message
        bool result = transport_->sendMessage(message);
        
        // Update statistics
        if (result) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            statistics_.messagesSent++;
        }
        
        return result;
    }
    
    /**
     * @brief Start a consensus round
     * @return True if consensus was started successfully
     */
    bool startConsensusRound() {
        if (!running_ || !transport_) {
            return false;
        }
        
        // Set consensus active
        consensusActive_ = true;
        
        // Create consensus proposal message
        ProtocolMessage message;
        message.type = MessageType::ConsensusProposal;
        message.destNodeId = "";  // Broadcast
        
        // Add proposal to payload
        // ...
        
        // Send message
        bool result = transport_->sendMessage(message);
        
        // Update statistics
        if (result) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            statistics_.messagesSent++;
            statistics_.consensusRounds++;
        }
        
        return result;
    }
};

// TimeReferenceProtocol implementation (just delegates to the Impl)

TimeReferenceProtocol::TimeReferenceProtocol(TimeSync& timeSync)
    : pImpl(std::make_unique<Impl>(timeSync)) {
}

TimeReferenceProtocol::~TimeReferenceProtocol() = default;

bool TimeReferenceProtocol::initialize(const std::string& nodeId, std::shared_ptr<ProtocolTransport> transport) {
    return pImpl->initialize(nodeId, transport);
}

bool TimeReferenceProtocol::start() {
    return pImpl->start();
}

bool TimeReferenceProtocol::stop() {
    return pImpl->stop();
}

void TimeReferenceProtocol::registerAlertCallback(AlertCallback callback) {
    pImpl->registerAlertCallback(callback);
}

ProtocolStatistics TimeReferenceProtocol::getStatistics() const {
    return pImpl->getStatistics();
}

std::map<std::string, NodeStatus> TimeReferenceProtocol::getNodeStatuses() const {
    return pImpl->getNodeStatuses();
}

NodeStatus TimeReferenceProtocol::getNodeStatus(const std::string& nodeId) const {
    return pImpl->getNodeStatus(nodeId);
}

bool TimeReferenceProtocol::broadcastTimeReference() {
    return pImpl->broadcastTimeReference();
}

bool TimeReferenceProtocol::setNodeWeight(const std::string& nodeId, double weight) {
    return pImpl->setNodeWeight(nodeId, weight);
}

double TimeReferenceProtocol::getTimeDifference(const std::string& nodeId) const {
    return pImpl->getTimeDifference(nodeId);
}

bool TimeReferenceProtocol::enablePTPFallback(bool enable) {
    return pImpl->enablePTPFallback(enable);
}

bool TimeReferenceProtocol::initiateConsensus() {
    return pImpl->initiateConsensus();
}

} // namespace time_sync
} // namespace tdoa 