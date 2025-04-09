/**
 * @file resource_manager.h
 * @brief Resource management for signal processing
 */

#pragma once

#include <string>
#include <map>
#include <mutex>
#include <vector>
#include <atomic>
#include <thread>
#include <memory>
#include <functional>
#include <condition_variable>
#include <chrono>

namespace tdoa {
namespace signal {

/**
 * @brief Resource type enumeration
 */
enum class ResourceType {
    CPU,        ///< CPU cores
    MEMORY,     ///< Memory in MB
    GPU,        ///< GPU resources
    NETWORK,    ///< Network bandwidth
    DISK,       ///< Disk I/O
    CUSTOM      ///< Custom resource
};

/**
 * @brief Convert ResourceType to string
 * @param type ResourceType to convert
 * @return String representation
 */
std::string resourceTypeToString(ResourceType type);

/**
 * @brief Resource usage statistics
 */
struct ResourceUsage {
    double available;       ///< Available amount of resource
    double total;           ///< Total amount of resource
    double reserved;        ///< Amount currently reserved
    double peak;            ///< Peak usage
    std::string unit;       ///< Unit of measurement
    
    /**
     * @brief Calculate usage percentage
     * @return Percentage of usage (0.0 to 100.0)
     */
    double getUsagePercent() const {
        return (total > 0.0) ? ((total - available) / total * 100.0) : 0.0;
    }
    
    /**
     * @brief Calculate reserved percentage
     * @return Percentage of reserved (0.0 to 100.0)
     */
    double getReservedPercent() const {
        return (total > 0.0) ? (reserved / total * 100.0) : 0.0;
    }
};

/**
 * @brief Task priority levels
 */
enum class TaskPriority {
    LOW,        ///< Low priority
    NORMAL,     ///< Normal priority
    HIGH,       ///< High priority
    CRITICAL    ///< Critical priority
};

/**
 * @brief Convert TaskPriority to string
 * @param priority TaskPriority to convert
 * @return String representation
 */
std::string taskPriorityToString(TaskPriority priority);

/**
 * @brief Convert string to TaskPriority
 * @param priorityStr String to convert
 * @return TaskPriority value
 * @throws std::invalid_argument if string cannot be converted
 */
TaskPriority stringToTaskPriority(const std::string& priorityStr);

/**
 * @brief Resource allocation request
 */
struct ResourceRequest {
    std::string requestId;                          ///< Unique request ID
    std::map<ResourceType, double> requirements;    ///< Resource requirements by type
    TaskPriority priority;                          ///< Request priority
    std::chrono::system_clock::time_point timestamp;///< Request timestamp
    std::string clientId;                           ///< Client identifier
    
    /**
     * @brief Default constructor
     */
    ResourceRequest();
    
    /**
     * @brief Constructor with parameters
     * @param id Request ID
     * @param reqs Resource requirements
     * @param prio Priority
     * @param client Client ID
     */
    ResourceRequest(
        const std::string& id,
        const std::map<ResourceType, double>& reqs,
        TaskPriority prio = TaskPriority::NORMAL,
        const std::string& client = ""
    );
};

/**
 * @brief Resource allocation result
 */
struct ResourceAllocation {
    std::string allocationId;                       ///< Unique allocation ID
    std::string requestId;                          ///< Original request ID
    std::map<ResourceType, double> allocated;       ///< Allocated resources by type
    bool success;                                   ///< Whether allocation was successful
    std::chrono::system_clock::time_point timestamp;///< Allocation timestamp
    std::string clientId;                           ///< Client identifier
    
    /**
     * @brief Default constructor
     */
    ResourceAllocation();
    
    /**
     * @brief Constructor with parameters
     * @param id Allocation ID
     * @param reqId Request ID
     * @param alloc Allocated resources
     * @param success Success flag
     * @param client Client ID
     */
    ResourceAllocation(
        const std::string& id,
        const std::string& reqId,
        const std::map<ResourceType, double>& alloc,
        bool success = true,
        const std::string& client = ""
    );
};

/**
 * @brief Resource manager for tracking and allocating system resources
 */
class ResourceManager {
public:
    /**
     * @brief Singleton instance accessor
     * @return Reference to the singleton instance
     */
    static ResourceManager& getInstance();
    
    /**
     * @brief Initialize the resource manager
     * @param cpuCores Number of CPU cores
     * @param memoryMB Amount of memory in MB
     * @param gpuMemoryMB Amount of GPU memory in MB
     * @return True if initialization successful
     */
    bool initialize(int cpuCores = 0, double memoryMB = 0.0, double gpuMemoryMB = 0.0);
    
    /**
     * @brief Set the total amount of a resource
     * @param type Resource type
     * @param amount Total amount
     * @param unit Unit of measurement
     * @return True if resource was set
     */
    bool setResourceTotal(ResourceType type, double amount, const std::string& unit = "");
    
    /**
     * @brief Add a custom resource
     * @param name Resource name
     * @param amount Total amount
     * @param unit Unit of measurement
     * @return True if resource was added
     */
    bool addCustomResource(const std::string& name, double amount, const std::string& unit = "");
    
    /**
     * @brief Get resource usage
     * @param type Resource type
     * @return Resource usage statistics
     */
    ResourceUsage getResourceUsage(ResourceType type) const;
    
    /**
     * @brief Get custom resource usage
     * @param name Resource name
     * @return Resource usage statistics
     */
    ResourceUsage getCustomResourceUsage(const std::string& name) const;
    
    /**
     * @brief Get all resource usage
     * @return Map of resource types to usage statistics
     */
    std::map<ResourceType, ResourceUsage> getAllResourceUsage() const;
    
    /**
     * @brief Get all custom resource usage
     * @return Map of resource names to usage statistics
     */
    std::map<std::string, ResourceUsage> getAllCustomResourceUsage() const;
    
    /**
     * @brief Request resource allocation
     * @param request Resource request
     * @return Allocation result
     */
    ResourceAllocation requestAllocation(const ResourceRequest& request);
    
    /**
     * @brief Release allocated resources
     * @param allocationId Allocation ID to release
     * @return True if resources were released
     */
    bool releaseAllocation(const std::string& allocationId);
    
    /**
     * @brief Check if an allocation can be made
     * @param request Resource request
     * @return True if allocation is possible
     */
    bool canAllocate(const ResourceRequest& request) const;
    
    /**
     * @brief Wait for resources to become available
     * @param request Resource request
     * @param timeout_ms Timeout in milliseconds (0 = wait indefinitely)
     * @return True if resources are available within the timeout
     */
    bool waitForResources(const ResourceRequest& request, int timeout_ms = 0);
    
    /**
     * @brief Get current active allocations
     * @return Map of allocation IDs to allocations
     */
    std::map<std::string, ResourceAllocation> getActiveAllocations() const;
    
    /**
     * @brief Get pending requests
     * @return Vector of pending requests
     */
    std::vector<ResourceRequest> getPendingRequests() const;
    
    /**
     * @brief Register a callback for allocation events
     * @param callback Callback function that takes an allocation
     * @return Callback ID
     */
    int registerAllocationCallback(std::function<void(const ResourceAllocation&)> callback);
    
    /**
     * @brief Unregister an allocation callback
     * @param callbackId Callback ID to unregister
     * @return True if callback was unregistered
     */
    bool unregisterAllocationCallback(int callbackId);
    
    /**
     * @brief Set resource preemption policy
     * @param enablePreemption Whether to enable preemption
     */
    void setPreemptionPolicy(bool enablePreemption);
    
    /**
     * @brief Check if preemption is enabled
     * @return True if preemption is enabled
     */
    bool isPreemptionEnabled() const;
    
    /**
     * @brief Adjust resource allocation based on system load
     * @param type Resource type
     * @param factor Adjustment factor (1.0 = no change)
     * @return True if adjustment was applied
     */
    bool adjustAllocation(ResourceType type, double factor);
    
    /**
     * @brief Generate a unique allocation ID
     * @return Unique ID string
     */
    std::string generateAllocationId() const;
    
    /**
     * @brief Reset all allocations and resource usage
     */
    void reset();
    
private:
    /**
     * @brief Private constructor for singleton
     */
    ResourceManager();
    
    /**
     * @brief Private destructor for singleton
     */
    ~ResourceManager();
    
    /**
     * @brief Allocate resources for a request
     * @param request Resource request
     * @return Allocation result
     */
    ResourceAllocation allocateResources(const ResourceRequest& request);
    
    /**
     * @brief Try to preempt lower priority allocations
     * @param request Resource request
     * @return True if preemption successful
     */
    bool tryPreemption(const ResourceRequest& request);
    
    /**
     * @brief Update resource usage after allocation or release
     */
    void updateResourceUsage();
    
    /**
     * @brief Notify allocation callbacks
     * @param allocation Allocation to notify about
     */
    void notifyAllocationCallbacks(const ResourceAllocation& allocation);
    
    std::map<ResourceType, ResourceUsage> resources_;                  ///< Standard resources
    std::map<std::string, ResourceUsage> customResources_;             ///< Custom resources
    std::map<std::string, ResourceAllocation> activeAllocations_;      ///< Active allocations
    std::vector<ResourceRequest> pendingRequests_;                     ///< Pending requests
    std::map<int, std::function<void(const ResourceAllocation&)>> allocationCallbacks_; ///< Allocation callbacks
    
    std::atomic<int> nextCallbackId_;                               ///< Next callback ID
    std::atomic<bool> preemptionEnabled_;                           ///< Preemption flag
    
    mutable std::mutex mutex_;                                      ///< Mutex for thread safety
    std::condition_variable resourceAvailableCV_;                   ///< Condition variable for resource availability
};

} // namespace signal
} // namespace tdoa 