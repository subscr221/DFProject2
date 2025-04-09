/**
 * @file udp_transport.cpp
 * @brief UDP transport implementation for time reference protocol
 */

#include "udp_transport.h"
#include <iostream>
#include <cstring>
#include <cerrno>
#include <vector>
#include <algorithm>
#include <chrono>

// Platform-specific socket includes
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define SHUT_RDWR SD_BOTH
    typedef int socklen_t;
#else
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <fcntl.h>
#endif

namespace tdoa {
namespace time_sync {

// Helper function for platform-specific socket initialization
bool initializeSockets() {
#ifdef _WIN32
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
#else
    return true;
#endif
}

// Helper function for platform-specific socket cleanup
void cleanupSockets() {
#ifdef _WIN32
    WSACleanup();
#endif
}

// Helper function for platform-specific socket close
void closeSocket(int socket) {
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

// Helper function for platform-specific error code
int getSocketError() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

// Helper function to get error message
std::string getErrorMessage(int errorCode) {
#ifdef _WIN32
    char* s = NULL;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&s, 0, NULL);
    std::string result = s;
    LocalFree(s);
    return result;
#else
    return std::string(strerror(errorCode));
#endif
}

// Implementation of UDPTransport
UDPTransport::UDPTransport(
    uint16_t localPort,
    const std::string& multicastGroup,
    uint16_t multicastPort)
    : localPort_(localPort)
    , multicastGroup_(multicastGroup)
    , multicastPort_(multicastPort)
    , running_(false)
    , socketFd_(-1)
    , nextSequence_(0)
{
    // Initialize sockets on Windows
    initializeSockets();
}

UDPTransport::~UDPTransport() {
    stop();
    
    // Cleanup sockets on Windows
    cleanupSockets();
}

bool UDPTransport::initialize(const std::string& nodeId) {
    if (socketFd_ >= 0) {
        std::cerr << "UDP transport already initialized" << std::endl;
        return false;
    }
    
    nodeId_ = nodeId;
    
    // Create UDP socket
    socketFd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socketFd_ < 0) {
        int error = getSocketError();
        std::cerr << "Failed to create UDP socket: " << getErrorMessage(error) << std::endl;
        return false;
    }
    
    // Set socket options
    int reuse = 1;
    if (setsockopt(socketFd_, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) {
        int error = getSocketError();
        std::cerr << "Failed to set SO_REUSEADDR: " << getErrorMessage(error) << std::endl;
        closeSocket(socketFd_);
        socketFd_ = -1;
        return false;
    }
    
    // Bind to local address
    struct sockaddr_in localAddr;
    memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = INADDR_ANY;
    localAddr.sin_port = htons(localPort_);
    
    if (bind(socketFd_, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        int error = getSocketError();
        std::cerr << "Failed to bind socket to port " << localPort_ << ": " 
                 << getErrorMessage(error) << std::endl;
        closeSocket(socketFd_);
        socketFd_ = -1;
        return false;
    }
    
    // Join multicast group
    if (!joinMulticastGroup()) {
        closeSocket(socketFd_);
        socketFd_ = -1;
        return false;
    }
    
    return true;
}

bool UDPTransport::start() {
    if (socketFd_ < 0) {
        std::cerr << "UDP transport not initialized" << std::endl;
        return false;
    }
    
    if (running_) {
        return true;  // Already running
    }
    
    running_ = true;
    receiveThread_ = std::thread(&UDPTransport::receiveThreadFunc, this);
    
    return true;
}

bool UDPTransport::stop() {
    if (!running_) {
        return true;  // Already stopped
    }
    
    running_ = false;
    
    // Wait for receive thread to exit
    if (receiveThread_.joinable()) {
        receiveThread_.join();
    }
    
    // Leave multicast group
    leaveMulticastGroup();
    
    // Close socket
    if (socketFd_ >= 0) {
        closeSocket(socketFd_);
        socketFd_ = -1;
    }
    
    return true;
}

bool UDPTransport::sendMessage(const ProtocolMessage& message) {
    if (socketFd_ < 0 || !running_) {
        std::cerr << "UDP transport not initialized or not running" << std::endl;
        return false;
    }
    
    // Make a copy of the message to set sequence number
    ProtocolMessage msgCopy = message;
    msgCopy.sourceNodeId = nodeId_;
    msgCopy.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    msgCopy.sequenceNumber = nextSequence_++;
    
    // Serialize message
    std::vector<uint8_t> data = serializeMessage(msgCopy);
    
    if (data.empty()) {
        std::cerr << "Failed to serialize message" << std::endl;
        return false;
    }
    
    // Send to specific node or broadcast
    if (!msgCopy.destNodeId.empty()) {
        // Find node address
        std::lock_guard<std::mutex> lock(nodesMutex_);
        auto it = nodes_.find(msgCopy.destNodeId);
        if (it == nodes_.end()) {
            std::cerr << "Unknown destination node: " << msgCopy.destNodeId << std::endl;
            return false;
        }
        
        // Send to specific node
        struct sockaddr_in destAddr;
        memset(&destAddr, 0, sizeof(destAddr));
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(it->second.port);
        destAddr.sin_addr.s_addr = inet_addr(it->second.address.c_str());
        
        if (sendto(socketFd_, (const char*)data.data(), data.size(), 0,
                 (struct sockaddr*)&destAddr, sizeof(destAddr)) < 0) {
            int error = getSocketError();
            std::cerr << "Failed to send message to " << it->second.address << ":" 
                     << it->second.port << ": " << getErrorMessage(error) << std::endl;
            return false;
        }
    } else {
        // Send to multicast group
        struct sockaddr_in destAddr;
        memset(&destAddr, 0, sizeof(destAddr));
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(multicastPort_);
        destAddr.sin_addr.s_addr = inet_addr(multicastGroup_.c_str());
        
        if (sendto(socketFd_, (const char*)data.data(), data.size(), 0,
                 (struct sockaddr*)&destAddr, sizeof(destAddr)) < 0) {
            int error = getSocketError();
            std::cerr << "Failed to send message to multicast group " << multicastGroup_ << ":" 
                     << multicastPort_ << ": " << getErrorMessage(error) << std::endl;
            return false;
        }
    }
    
    return true;
}

void UDPTransport::registerMessageCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    messageCallback_ = callback;
}

bool UDPTransport::addNode(const std::string& nodeId, const std::string& address, uint16_t port) {
    if (nodeId.empty() || address.empty() || port == 0) {
        std::cerr << "Invalid node parameters" << std::endl;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(nodesMutex_);
    nodes_[nodeId] = {address, port};
    return true;
}

bool UDPTransport::removeNode(const std::string& nodeId) {
    std::lock_guard<std::mutex> lock(nodesMutex_);
    auto it = nodes_.find(nodeId);
    if (it == nodes_.end()) {
        return false;
    }
    
    nodes_.erase(it);
    return true;
}

bool UDPTransport::setMulticastTTL(int ttl) {
    if (socketFd_ < 0) {
        std::cerr << "UDP transport not initialized" << std::endl;
        return false;
    }
    
    if (setsockopt(socketFd_, IPPROTO_IP, IP_MULTICAST_TTL, (const char*)&ttl, sizeof(ttl)) < 0) {
        int error = getSocketError();
        std::cerr << "Failed to set multicast TTL: " << getErrorMessage(error) << std::endl;
        return false;
    }
    
    return true;
}

bool UDPTransport::enableMulticastLoopback(bool enable) {
    if (socketFd_ < 0) {
        std::cerr << "UDP transport not initialized" << std::endl;
        return false;
    }
    
    int loopback = enable ? 1 : 0;
    if (setsockopt(socketFd_, IPPROTO_IP, IP_MULTICAST_LOOP, (const char*)&loopback, sizeof(loopback)) < 0) {
        int error = getSocketError();
        std::cerr << "Failed to set multicast loopback: " << getErrorMessage(error) << std::endl;
        return false;
    }
    
    return true;
}

void UDPTransport::receiveThreadFunc() {
    const size_t bufferSize = 65536;  // Max UDP packet size
    std::vector<uint8_t> buffer(bufferSize);
    
    struct sockaddr_in senderAddr;
    socklen_t senderAddrLen = sizeof(senderAddr);
    
    while (running_) {
        // Receive data
        ssize_t bytesRead = recvfrom(socketFd_, (char*)buffer.data(), buffer.size(), 0,
                                  (struct sockaddr*)&senderAddr, &senderAddrLen);
        
        if (bytesRead < 0) {
            int error = getSocketError();
#ifdef _WIN32
            // On Windows, WSAEWOULDBLOCK is returned for non-blocking operations
            if (error == WSAEWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
#else
            // On Unix, EAGAIN or EWOULDBLOCK is returned for non-blocking operations
            if (error == EAGAIN || error == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
#endif
            
            std::cerr << "Error receiving data: " << getErrorMessage(error) << std::endl;
            continue;
        }
        
        if (bytesRead == 0) {
            continue;  // No data received
        }
        
        // Deserialize message
        ProtocolMessage message = deserializeMessage(buffer.data(), bytesRead);
        
        // Filter out messages from self
        if (message.sourceNodeId == nodeId_) {
            continue;
        }
        
        // Call message callback
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (messageCallback_) {
            messageCallback_(message);
        }
        
        // Add sender to known nodes if not already known
        if (!message.sourceNodeId.empty()) {
            std::lock_guard<std::mutex> nodesLock(nodesMutex_);
            if (nodes_.find(message.sourceNodeId) == nodes_.end()) {
                // Convert IP address to string
                char addrStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(senderAddr.sin_addr), addrStr, INET_ADDRSTRLEN);
                
                // Add to known nodes
                nodes_[message.sourceNodeId] = {
                    addrStr,
                    ntohs(senderAddr.sin_port)
                };
            }
        }
    }
}

std::vector<uint8_t> UDPTransport::serializeMessage(const ProtocolMessage& message) {
    // Simple binary serialization
    std::vector<uint8_t> result;
    
    // Message type
    uint8_t type = static_cast<uint8_t>(message.type);
    result.push_back(type);
    
    // Source node ID
    uint16_t sourceIdLength = static_cast<uint16_t>(message.sourceNodeId.size());
    result.push_back(sourceIdLength & 0xFF);
    result.push_back((sourceIdLength >> 8) & 0xFF);
    result.insert(result.end(), message.sourceNodeId.begin(), message.sourceNodeId.end());
    
    // Destination node ID
    uint16_t destIdLength = static_cast<uint16_t>(message.destNodeId.size());
    result.push_back(destIdLength & 0xFF);
    result.push_back((destIdLength >> 8) & 0xFF);
    result.insert(result.end(), message.destNodeId.begin(), message.destNodeId.end());
    
    // Timestamp
    for (int i = 0; i < 8; i++) {
        result.push_back((message.timestamp >> (i * 8)) & 0xFF);
    }
    
    // Sequence number
    for (int i = 0; i < 4; i++) {
        result.push_back((message.sequenceNumber >> (i * 8)) & 0xFF);
    }
    
    // Payload
    uint32_t payloadLength = static_cast<uint32_t>(message.payload.size());
    for (int i = 0; i < 4; i++) {
        result.push_back((payloadLength >> (i * 8)) & 0xFF);
    }
    result.insert(result.end(), message.payload.begin(), message.payload.end());
    
    // Signature (placeholder for now)
    uint16_t signatureLength = static_cast<uint16_t>(message.signature.size());
    result.push_back(signatureLength & 0xFF);
    result.push_back((signatureLength >> 8) & 0xFF);
    result.insert(result.end(), message.signature.begin(), message.signature.end());
    
    return result;
}

ProtocolMessage UDPTransport::deserializeMessage(const uint8_t* data, size_t length) {
    ProtocolMessage result;
    
    if (length < 19) {
        std::cerr << "Message too short" << std::endl;
        return result;  // Message too short
    }
    
    size_t offset = 0;
    
    // Message type
    result.type = static_cast<MessageType>(data[offset++]);
    
    // Source node ID
    uint16_t sourceIdLength = data[offset] | (data[offset + 1] << 8);
    offset += 2;
    
    if (offset + sourceIdLength > length) {
        std::cerr << "Invalid source ID length" << std::endl;
        return ProtocolMessage();  // Invalid length
    }
    
    result.sourceNodeId.assign(reinterpret_cast<const char*>(data + offset), sourceIdLength);
    offset += sourceIdLength;
    
    // Destination node ID
    uint16_t destIdLength = data[offset] | (data[offset + 1] << 8);
    offset += 2;
    
    if (offset + destIdLength > length) {
        std::cerr << "Invalid destination ID length" << std::endl;
        return ProtocolMessage();  // Invalid length
    }
    
    result.destNodeId.assign(reinterpret_cast<const char*>(data + offset), destIdLength);
    offset += destIdLength;
    
    // Timestamp
    result.timestamp = 0;
    for (int i = 0; i < 8; i++) {
        result.timestamp |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }
    
    // Sequence number
    result.sequenceNumber = 0;
    for (int i = 0; i < 4; i++) {
        result.sequenceNumber |= static_cast<uint32_t>(data[offset++]) << (i * 8);
    }
    
    // Payload
    uint32_t payloadLength = 0;
    for (int i = 0; i < 4; i++) {
        payloadLength |= static_cast<uint32_t>(data[offset++]) << (i * 8);
    }
    
    if (offset + payloadLength > length) {
        std::cerr << "Invalid payload length" << std::endl;
        return ProtocolMessage();  // Invalid length
    }
    
    result.payload.assign(data + offset, data + offset + payloadLength);
    offset += payloadLength;
    
    // Signature
    if (offset + 2 > length) {
        std::cerr << "Invalid signature length" << std::endl;
        return ProtocolMessage();  // Invalid length
    }
    
    uint16_t signatureLength = data[offset] | (data[offset + 1] << 8);
    offset += 2;
    
    if (offset + signatureLength > length) {
        std::cerr << "Invalid signature data length" << std::endl;
        return ProtocolMessage();  // Invalid length
    }
    
    result.signature.assign(data + offset, data + offset + signatureLength);
    
    return result;
}

bool UDPTransport::joinMulticastGroup() {
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(multicastGroup_.c_str());
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    if (setsockopt(socketFd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&mreq, sizeof(mreq)) < 0) {
        int error = getSocketError();
        std::cerr << "Failed to join multicast group " << multicastGroup_ << ": " 
                  << getErrorMessage(error) << std::endl;
        return false;
    }
    
    return true;
}

bool UDPTransport::leaveMulticastGroup() {
    if (socketFd_ < 0) {
        return true;  // Socket not initialized
    }
    
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(multicastGroup_.c_str());
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    if (setsockopt(socketFd_, IPPROTO_IP, IP_DROP_MEMBERSHIP, (const char*)&mreq, sizeof(mreq)) < 0) {
        int error = getSocketError();
        std::cerr << "Failed to leave multicast group " << multicastGroup_ << ": " 
                  << getErrorMessage(error) << std::endl;
        return false;
    }
    
    return true;
}

std::shared_ptr<ProtocolTransport> createUDPTransport(
    uint16_t localPort,
    const std::string& multicastGroup,
    uint16_t multicastPort) {
    return std::make_shared<UDPTransport>(localPort, multicastGroup, multicastPort);
}

} // namespace time_sync
} // namespace tdoa 