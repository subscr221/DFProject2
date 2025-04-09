/**
 * @file time_reference_protocol.h
 * @brief Protocol for exchanging time reference information between nodes
 */

#pragma once

#include "time_sync_interface.h"
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <map>
#include <memory>
#include <chrono>
#include <atomic>

namespace tdoa {
namespace time_sync {

/**
 * @brief Protocol message types
 */
enum class MessageType {
    TimeReference,      ///< Time reference information
    SyncRequest,        ///< Request for synchronization
    SyncResponse,       ///< Response to synchronization request
    StatusUpdate,       ///< Node status update
    StatusRequest,      ///< Request for node status
    StatusResponse,     ///< Response to status request
    ConsensusProposal,  ///< Proposal for time consensus
    ConsensusVote,      ///< Vote for time consensus
    Alert               ///< Alert message (failures, errors)
};

/**
 * @brief Protocol message structure
 */
struct ProtocolMessage {
    MessageType type;              ///< Message type
    std::string sourceNodeId;      ///< Source node ID
    std::string destNodeId;        ///< Destination node ID (empty for broadcast)
    uint64_t timestamp;            ///< Timestamp in nanoseconds
    uint32_t sequenceNumber;       ///< Message sequence number
    std::vector<uint8_t> payload;  ///< Message payload
    std::vector<uint8_t> signature;///< Message signature
    
    /**
     * @brief Default constructor
     */
    ProtocolMessage()
        : type(MessageType::TimeReference)
        , timestamp(0)
        , sequenceNumber(0)
    {}
};

/**
 * @brief Time reference protocol statistics
 */
struct ProtocolStatistics {
    uint32_t messagesSent;         ///< Total messages sent
    uint32_t messagesReceived;     ///< Total messages received
    uint32_t messagesRejected;     ///< Messages rejected (invalid, duplicates)
    uint32_t timeReferencesReceived;///< Time references received
    uint32_t timeReferencesSent;   ///< Time references sent
    uint32_t consensusRounds;      ///< Number of consensus rounds
    uint32_t alertsReceived;       ///< Alerts received
    uint32_t alertsSent;           ///< Alerts sent
    
    /**
     * @brief Constructor with default values
     */
    ProtocolStatistics()
        : messagesSent(0)
        , messagesReceived(0)
        , messagesRejected(0)
        , timeReferencesReceived(0)
        , timeReferencesSent(0)
        , consensusRounds(0)
        , alertsReceived(0)
        , alertsSent(0)
    {}
};

/**
 * @brief Node status information
 */
struct NodeStatus {
    std::string nodeId;             ///< Node ID
    SyncStatus syncStatus;          ///< Synchronization status
    SyncSource primarySyncSource;   ///< Primary synchronization source
    SyncSource secondarySyncSource; ///< Secondary synchronization source
    double uncertaintyNs;           ///< Time uncertainty in nanoseconds
    double allanDeviation;          ///< Allan deviation (1-second)
    double driftRatePpb;            ///< Drift rate in ppb
    uint32_t satelliteCount;        ///< GPS satellite count (if applicable)
    uint64_t lastUpdateTime;        ///< Last update timestamp
    
    /**
     * @brief Default constructor
     */
    NodeStatus()
        : syncStatus(SyncStatus::Unknown)
        , primarySyncSource(SyncSource::None)
        , secondarySyncSource(SyncSource::None)
        , uncertaintyNs(1000000.0)  // 1ms default
        , allanDeviation(0.0)
        , driftRatePpb(0.0)
        , satelliteCount(0)
        , lastUpdateTime(0)
    {}
};

/**
 * @brief Transport interface for protocol messages
 */
class ProtocolTransport {
public:
    /**
     * @brief Message received callback
     */
    using MessageCallback = std::function<void(const ProtocolMessage&)>;
    
    /**
     * @brief Default virtual destructor
     */
    virtual ~ProtocolTransport() = default;
    
    /**
     * @brief Initialize the transport
     * @param nodeId Local node ID
     * @return True if initialization was successful
     */
    virtual bool initialize(const std::string& nodeId) = 0;
    
    /**
     * @brief Start the transport
     * @return True if started successfully
     */
    virtual bool start() = 0;
    
    /**
     * @brief Stop the transport
     * @return True if stopped successfully
     */
    virtual bool stop() = 0;
    
    /**
     * @brief Send a message
     * @param message Message to send
     * @return True if message was sent successfully
     */
    virtual bool sendMessage(const ProtocolMessage& message) = 0;
    
    /**
     * @brief Register callback for received messages
     * @param callback Function to call when message is received
     */
    virtual void registerMessageCallback(MessageCallback callback) = 0;
};

/**
 * @class TimeReferenceProtocol
 * @brief Protocol for exchanging time reference information between nodes
 * 
 * This class implements a protocol for exchanging time reference information
 * between nodes, monitoring synchronization status, and handling degraded
 * GPS conditions through distributed consensus.
 */
class TimeReferenceProtocol {
public:
    /**
     * @brief Alert callback
     */
    using AlertCallback = std::function<void(const std::string&, const std::string&)>;
    
    /**
     * @brief Constructor
     * @param timeSync Time synchronization instance
     */
    TimeReferenceProtocol(TimeSync& timeSync);
    
    /**
     * @brief Destructor
     */
    ~TimeReferenceProtocol();
    
    /**
     * @brief Initialize the protocol
     * @param nodeId Local node ID
     * @param transport Transport interface for message exchange
     * @return True if initialization was successful
     */
    bool initialize(const std::string& nodeId, std::shared_ptr<ProtocolTransport> transport);
    
    /**
     * @brief Start the protocol
     * @return True if started successfully
     */
    bool start();
    
    /**
     * @brief Stop the protocol
     * @return True if stopped successfully
     */
    bool stop();
    
    /**
     * @brief Register callback for alert messages
     * @param callback Function to call when alert is received
     */
    void registerAlertCallback(AlertCallback callback);
    
    /**
     * @brief Get protocol statistics
     * @return Protocol statistics
     */
    ProtocolStatistics getStatistics() const;
    
    /**
     * @brief Get status of all known nodes
     * @return Map of node IDs to node status
     */
    std::map<std::string, NodeStatus> getNodeStatuses() const;
    
    /**
     * @brief Get status of a specific node
     * @param nodeId Node ID
     * @return Node status (empty nodeId if not found)
     */
    NodeStatus getNodeStatus(const std::string& nodeId) const;
    
    /**
     * @brief Force a time reference broadcast
     * @return True if broadcast was successful
     */
    bool broadcastTimeReference();
    
    /**
     * @brief Set synchronization weight for a node
     * @param nodeId Node ID
     * @param weight Synchronization weight (0.0 - 1.0)
     * @return True if weight was set successfully
     */
    bool setNodeWeight(const std::string& nodeId, double weight);
    
    /**
     * @brief Calculate time difference to a specific node
     * @param nodeId Node ID
     * @return Time difference in nanoseconds (positive if remote is ahead)
     */
    double getTimeDifference(const std::string& nodeId) const;
    
    /**
     * @brief Enable or disable PTP fallback
     * @param enable Whether to enable PTP fallback
     * @return True if configuration was successful
     */
    bool enablePTPFallback(bool enable);
    
    /**
     * @brief Force a consensus round
     * @return True if consensus was started successfully
     */
    bool initiateConsensus();

private:
    // Forward declaration of implementation class (PIMPL idiom)
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Create a UDP transport for the time reference protocol
 * @param localPort Local UDP port
 * @param multicastGroup Multicast group for broadcast (default: 239.255.77.77)
 * @param multicastPort Multicast port (default: 7777)
 * @return Transport interface
 */
std::shared_ptr<ProtocolTransport> createUDPTransport(
    uint16_t localPort,
    const std::string& multicastGroup = "239.255.77.77",
    uint16_t multicastPort = 7777);

} // namespace time_sync
} // namespace tdoa 