/**
 * @file udp_transport.h
 * @brief UDP transport implementation for time reference protocol
 */

#pragma once

#include "time_reference_protocol.h"
#include <string>
#include <atomic>
#include <thread>
#include <vector>
#include <mutex>
#include <map>
#include <functional>

namespace tdoa {
namespace time_sync {

/**
 * @class UDPTransport
 * @brief UDP-based transport for time reference protocol messages
 * 
 * This class implements a UDP-based transport for time reference protocol messages.
 * It supports both unicast and multicast communications.
 */
class UDPTransport : public ProtocolTransport {
public:
    /**
     * @brief Constructor
     * @param localPort Local UDP port
     * @param multicastGroup Multicast group address
     * @param multicastPort Multicast port
     */
    UDPTransport(
        uint16_t localPort,
        const std::string& multicastGroup,
        uint16_t multicastPort);
    
    /**
     * @brief Destructor
     */
    ~UDPTransport() override;
    
    /**
     * @brief Initialize the transport
     * @param nodeId Local node ID
     * @return True if initialization was successful
     */
    bool initialize(const std::string& nodeId) override;
    
    /**
     * @brief Start the transport
     * @return True if started successfully
     */
    bool start() override;
    
    /**
     * @brief Stop the transport
     * @return True if stopped successfully
     */
    bool stop() override;
    
    /**
     * @brief Send a message
     * @param message Message to send
     * @return True if message was sent successfully
     */
    bool sendMessage(const ProtocolMessage& message) override;
    
    /**
     * @brief Register callback for received messages
     * @param callback Function to call when message is received
     */
    void registerMessageCallback(MessageCallback callback) override;
    
    /**
     * @brief Add a known node address for direct communication
     * @param nodeId Node ID
     * @param address IP address
     * @param port UDP port
     * @return True if node was added successfully
     */
    bool addNode(const std::string& nodeId, const std::string& address, uint16_t port);
    
    /**
     * @brief Remove a known node
     * @param nodeId Node ID
     * @return True if node was removed successfully
     */
    bool removeNode(const std::string& nodeId);
    
    /**
     * @brief Set multicast TTL
     * @param ttl Time-to-live value
     * @return True if TTL was set successfully
     */
    bool setMulticastTTL(int ttl);
    
    /**
     * @brief Enable or disable multicast loopback
     * @param enable Whether to enable loopback
     * @return True if configuration was successful
     */
    bool enableMulticastLoopback(bool enable);
    
private:
    struct NodeAddress {
        std::string address;
        uint16_t port;
    };
    
    uint16_t localPort_;              ///< Local UDP port
    std::string multicastGroup_;      ///< Multicast group address
    uint16_t multicastPort_;          ///< Multicast port
    std::string nodeId_;              ///< Local node ID
    
    std::atomic<bool> running_;       ///< Running flag
    int socketFd_;                    ///< UDP socket file descriptor
    std::thread receiveThread_;       ///< Thread for receiving messages
    
    mutable std::mutex nodesMutex_;   ///< Mutex for protecting nodes map
    std::map<std::string, NodeAddress> nodes_; ///< Known nodes for direct communication
    
    MessageCallback messageCallback_; ///< Callback for received messages
    mutable std::mutex callbackMutex_;///< Mutex for protecting callback
    
    std::atomic<uint32_t> nextSequence_; ///< Next sequence number
    
    /**
     * @brief Receive thread function
     */
    void receiveThreadFunc();
    
    /**
     * @brief Serialize message to binary
     * @param message Message to serialize
     * @return Serialized message
     */
    std::vector<uint8_t> serializeMessage(const ProtocolMessage& message);
    
    /**
     * @brief Deserialize binary to message
     * @param data Binary data
     * @param length Data length
     * @return Deserialized message (empty source node ID if invalid)
     */
    ProtocolMessage deserializeMessage(const uint8_t* data, size_t length);
    
    /**
     * @brief Join multicast group
     * @return True if joined successfully
     */
    bool joinMulticastGroup();
    
    /**
     * @brief Leave multicast group
     * @return True if left successfully
     */
    bool leaveMulticastGroup();
};

} // namespace time_sync
} // namespace tdoa 