/**
 * @file resource_manager.cpp
 * @brief Implementation of the ResourceManager class
 */

#include "resource_manager.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>

namespace tdoa {
namespace signal {

// Convert ResourceType to string
std::string resourceTypeToString(ResourceType type) {
    switch (type) {
        case ResourceType::CPU:
            return "CPU";
        case ResourceType::MEMORY:
            return "MEMORY";
        case ResourceType::GPU:
            return "GPU";
        case ResourceType::NETWORK:
            return "NETWORK";
        case ResourceType::DISK:
            return "DISK";
        case ResourceType::CUSTOM:
            return "CUSTOM";
        default:
            return "UNKNOWN";
    }
}

// Convert TaskPriority to string
std::string taskPriorityToString(TaskPriority priority) {
    switch (priority) {
        case TaskPriority::LOW:
            return "LOW";
        case TaskPriority::NORMAL:
            return "NORMAL";
        case TaskPriority::HIGH:
            return "HIGH";
        case TaskPriority::CRITICAL:
            return "CRITICAL";
        default:
            return "UNKNOWN";
    }
}

// Convert string to TaskPriority
TaskPriority stringToTaskPriority(const std::string& priorityStr) {
    if (priorityStr == "LOW") {
        return TaskPriority::LOW;
    } else if (priorityStr == "NORMAL") {
        return TaskPriority::NORMAL;
    } else if (priorityStr == "HIGH") {
        return TaskPriority::HIGH;
    } else if (priorityStr == "CRITICAL") {
        return TaskPriority::CRITICAL;
    } else {
        throw std::invalid_argument("Invalid task priority string: " + priorityStr);
    }
}

//-----------------------------------------------------------------------------
// ResourceRequest Implementation
//-----------------------------------------------------------------------------

// Default constructor
ResourceRequest::ResourceRequest()
    : priority(TaskPriority::NORMAL)
    , timestamp(std::chrono::system_clock::now()) {
}

// Constructor with parameters
ResourceRequest::ResourceRequest(
    const std::string& id,
    const std::map<ResourceType, double>& reqs,
    TaskPriority prio,
    const std::string& client
)
    : requestId(id)
    , requirements(reqs)
    , priority(prio)
    , timestamp(std::chrono::system_clock::now())
    , clientId(client) {
}

//-----------------------------------------------------------------------------
// ResourceAllocation Implementation
//-----------------------------------------------------------------------------

// Default constructor
ResourceAllocation::ResourceAllocation()
    : success(false)
    , timestamp(std::chrono::system_clock::now()) {
}

// Constructor with parameters
ResourceAllocation::ResourceAllocation(
    const std::string& id,
    const std::string& reqId,
    const std::map<ResourceType, double>& alloc,
    bool success_,
    const std::string& client
)
    : allocationId(id)
    , requestId(reqId)
    , allocated(alloc)
    , success(success_)
    , timestamp(std::chrono::system_clock::now())
    , clientId(client) {
}

//-----------------------------------------------------------------------------
// ResourceManager Implementation
//-----------------------------------------------------------------------------

// Singleton instance accessor
ResourceManager& ResourceManager::getInstance() {
    static ResourceManager instance;
    return instance;
}

// Private constructor for singleton
ResourceManager::ResourceManager()
    : nextCallbackId_(0)
    , preemptionEnabled_(false) {
    
    // Initialize with default resources
    ResourceUsage cpuUsage = {0.0, 0.0, 0.0, 0.0, "cores"};
    ResourceUsage memoryUsage = {0.0, 0.0, 0.0, 0.0, "MB"};
    ResourceUsage gpuUsage = {0.0, 0.0, 0.0, 0.0, "MB"};
    ResourceUsage networkUsage = {0.0, 0.0, 0.0, 0.0, "MB/s"};
    ResourceUsage diskUsage = {0.0, 0.0, 0.0, 0.0, "MB/s"};
    
    resources_[ResourceType::CPU] = cpuUsage;
    resources_[ResourceType::MEMORY] = memoryUsage;
    resources_[ResourceType::GPU] = gpuUsage;
    resources_[ResourceType::NETWORK] = networkUsage;
    resources_[ResourceType::DISK] = diskUsage;
}

// Private destructor for singleton
ResourceManager::~ResourceManager() {
    // Clean up all allocations
    reset();
}

// Initialize the resource manager
bool ResourceManager::initialize(int cpuCores, double memoryMB, double gpuMemoryMB) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Auto-detect system resources if not provided
    if (cpuCores <= 0) {
        cpuCores = std::thread::hardware_concurrency();
        if (cpuCores <= 0) {
            cpuCores = 4; // Default to 4 cores if detection fails
        }
    }
    
    if (memoryMB <= 0.0) {
        // Default to 4GB if detection fails
        memoryMB = 4096.0;
    }
    
    // Set CPU resource
    resources_[ResourceType::CPU].total = static_cast<double>(cpuCores);
    resources_[ResourceType::CPU].available = static_cast<double>(cpuCores);
    resources_[ResourceType::CPU].unit = "cores";
    
    // Set memory resource
    resources_[ResourceType::MEMORY].total = memoryMB;
    resources_[ResourceType::MEMORY].available = memoryMB;
    resources_[ResourceType::MEMORY].unit = "MB";
    
    // Set GPU resource if provided
    if (gpuMemoryMB > 0.0) {
        resources_[ResourceType::GPU].total = gpuMemoryMB;
        resources_[ResourceType::GPU].available = gpuMemoryMB;
        resources_[ResourceType::GPU].unit = "MB";
    }
    
    // Set default network and disk resources
    resources_[ResourceType::NETWORK].total = 100.0;
    resources_[ResourceType::NETWORK].available = 100.0;
    resources_[ResourceType::NETWORK].unit = "MB/s";
    
    resources_[ResourceType::DISK].total = 100.0;
    resources_[ResourceType::DISK].available = 100.0;
    resources_[ResourceType::DISK].unit = "MB/s";
    
    return true;
}

// Set the total amount of a resource
bool ResourceManager::setResourceTotal(ResourceType type, double amount, const std::string& unit) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (amount <= 0.0) {
        std::cerr << "Error: Resource amount must be positive" << std::endl;
        return false;
    }
    
    // Get the current usage
    auto& resource = resources_[type];
    double currentUsage = resource.total - resource.available;
    
    // Update the resource
    resource.total = amount;
    resource.available = std::max(0.0, amount - currentUsage);
    
    // Update the unit if provided
    if (!unit.empty()) {
        resource.unit = unit;
    }
    
    return true;
}

// Add a custom resource
bool ResourceManager::addCustomResource(const std::string& name, double amount, const std::string& unit) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (amount <= 0.0) {
        std::cerr << "Error: Resource amount must be positive" << std::endl;
        return false;
    }
    
    // Check if the resource already exists
    if (customResources_.find(name) != customResources_.end()) {
        // Update existing resource
        auto& resource = customResources_[name];
        double currentUsage = resource.total - resource.available;
        
        resource.total = amount;
        resource.available = std::max(0.0, amount - currentUsage);
        
        // Update the unit if provided
        if (!unit.empty()) {
            resource.unit = unit;
        }
    } else {
        // Create a new resource
        ResourceUsage usage = {amount, amount, 0.0, 0.0, unit.empty() ? "units" : unit};
        customResources_[name] = usage;
    }
    
    return true;
}

// Get resource usage
ResourceUsage ResourceManager::getResourceUsage(ResourceType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = resources_.find(type);
    if (it != resources_.end()) {
        return it->second;
    }
    
    // Return default resource usage if not found
    return {0.0, 0.0, 0.0, 0.0, ""};
}

// Get custom resource usage
ResourceUsage ResourceManager::getCustomResourceUsage(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = customResources_.find(name);
    if (it != customResources_.end()) {
        return it->second;
    }
    
    // Return default resource usage if not found
    return {0.0, 0.0, 0.0, 0.0, ""};
}

// Get all resource usage
std::map<ResourceType, ResourceUsage> ResourceManager::getAllResourceUsage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return resources_;
}

// Get all custom resource usage
std::map<std::string, ResourceUsage> ResourceManager::getAllCustomResourceUsage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return customResources_;
}

// Request resource allocation
ResourceAllocation ResourceManager::requestAllocation(const ResourceRequest& request) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if we can allocate the resources
    if (canAllocate(request)) {
        // Allocate the resources
        return allocateResources(request);
    } else if (preemptionEnabled_) {
        // Try to preempt lower priority allocations
        if (tryPreemption(request)) {
            // Retry allocation after preemption
            return allocateResources(request);
        }
    }
    
    // If we can't allocate, add to pending requests
    pendingRequests_.push_back(request);
    
    // Sort pending requests by priority (highest first)
    std::sort(pendingRequests_.begin(), pendingRequests_.end(),
        [](const ResourceRequest& a, const ResourceRequest& b) {
            return static_cast<int>(a.priority) > static_cast<int>(b.priority);
        });
    
    // Return failed allocation
    ResourceAllocation allocation;
    allocation.requestId = request.requestId;
    allocation.clientId = request.clientId;
    allocation.success = false;
    
    return allocation;
}

// Release allocated resources
bool ResourceManager::releaseAllocation(const std::string& allocationId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Find the allocation
    auto it = activeAllocations_.find(allocationId);
    if (it == activeAllocations_.end()) {
        std::cerr << "Error: Allocation with ID '" << allocationId << "' not found" << std::endl;
        return false;
    }
    
    // Get the allocation
    const auto& allocation = it->second;
    
    // Release the resources
    for (const auto& pair : allocation.allocated) {
        ResourceType type = pair.first;
        double amount = pair.second;
        
        // Update the resource
        auto& resource = resources_[type];
        resource.available += amount;
        resource.reserved -= amount;
        
        // Ensure we don't exceed the total
        if (resource.available > resource.total) {
            resource.available = resource.total;
        }
    }
    
    // Remove the allocation
    activeAllocations_.erase(it);
    
    // Notify waiters
    resourceAvailableCV_.notify_all();
    
    return true;
}

// Check if an allocation can be made
bool ResourceManager::canAllocate(const ResourceRequest& request) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check each required resource
    for (const auto& pair : request.requirements) {
        ResourceType type = pair.first;
        double amount = pair.second;
        
        // Check if we have this resource
        auto it = resources_.find(type);
        if (it == resources_.end()) {
            return false;
        }
        
        // Check if we have enough available
        if (it->second.available < amount) {
            return false;
        }
    }
    
    return true;
}

// Wait for resources to become available
bool ResourceManager::waitForResources(const ResourceRequest& request, int timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // If resources are already available, return immediately
    if (canAllocate(request)) {
        return true;
    }
    
    // Wait for resources to become available
    if (timeout_ms > 0) {
        // Wait with timeout
        return resourceAvailableCV_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
            [this, &request]() { return canAllocate(request); });
    } else {
        // Wait indefinitely
        resourceAvailableCV_.wait(lock,
            [this, &request]() { return canAllocate(request); });
        return true;
    }
}

// Get current active allocations
std::map<std::string, ResourceAllocation> ResourceManager::getActiveAllocations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return activeAllocations_;
}

// Get pending requests
std::vector<ResourceRequest> ResourceManager::getPendingRequests() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pendingRequests_;
}

// Register a callback for allocation events
int ResourceManager::registerAllocationCallback(std::function<void(const ResourceAllocation&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!callback) {
        std::cerr << "Error: Cannot register null callback" << std::endl;
        return -1;
    }
    
    int id = nextCallbackId_++;
    allocationCallbacks_[id] = callback;
    
    return id;
}

// Unregister an allocation callback
bool ResourceManager::unregisterAllocationCallback(int callbackId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = allocationCallbacks_.find(callbackId);
    if (it == allocationCallbacks_.end()) {
        std::cerr << "Error: Callback with ID " << callbackId << " not found" << std::endl;
        return false;
    }
    
    allocationCallbacks_.erase(it);
    return true;
}

// Set resource preemption policy
void ResourceManager::setPreemptionPolicy(bool enablePreemption) {
    preemptionEnabled_ = enablePreemption;
}

// Check if preemption is enabled
bool ResourceManager::isPreemptionEnabled() const {
    return preemptionEnabled_;
}

// Adjust resource allocation based on system load
bool ResourceManager::adjustAllocation(ResourceType type, double factor) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (factor <= 0.0) {
        std::cerr << "Error: Adjustment factor must be positive" << std::endl;
        return false;
    }
    
    // Check if we have this resource
    auto it = resources_.find(type);
    if (it == resources_.end()) {
        std::cerr << "Error: Resource type not found" << std::endl;
        return false;
    }
    
    // Get the current usage
    auto& resource = it->second;
    double currentUsage = resource.total - resource.available;
    
    // Adjust the total
    double newTotal = resource.total * factor;
    resource.total = newTotal;
    
    // Adjust the available
    resource.available = std::max(0.0, newTotal - currentUsage);
    
    return true;
}

// Generate a unique allocation ID
std::string ResourceManager::generateAllocationId() const {
    // Generate a random ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "alloc-";
    
    // Generate a UUID-like string
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            ss << "-";
        }
        ss << std::hex << dis(gen);
    }
    
    return ss.str();
}

// Reset all allocations and resource usage
void ResourceManager::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Reset all resources
    for (auto& pair : resources_) {
        pair.second.available = pair.second.total;
        pair.second.reserved = 0.0;
        pair.second.peak = 0.0;
    }
    
    // Reset all custom resources
    for (auto& pair : customResources_) {
        pair.second.available = pair.second.total;
        pair.second.reserved = 0.0;
        pair.second.peak = 0.0;
    }
    
    // Clear allocations and pending requests
    activeAllocations_.clear();
    pendingRequests_.clear();
    
    // Notify waiters
    resourceAvailableCV_.notify_all();
}

// Allocate resources for a request
ResourceAllocation ResourceManager::allocateResources(const ResourceRequest& request) {
    // Generate a unique allocation ID
    std::string allocationId = generateAllocationId();
    
    // Create the allocation
    ResourceAllocation allocation(
        allocationId,
        request.requestId,
        request.requirements,
        true,
        request.clientId
    );
    
    // Update resources
    for (const auto& pair : request.requirements) {
        ResourceType type = pair.first;
        double amount = pair.second;
        
        // Update the resource
        auto& resource = resources_[type];
        resource.available -= amount;
        resource.reserved += amount;
        
        // Update peak usage
        double currentUsage = resource.total - resource.available;
        if (currentUsage > resource.peak) {
            resource.peak = currentUsage;
        }
    }
    
    // Add to active allocations
    activeAllocations_[allocationId] = allocation;
    
    // Notify callbacks
    notifyAllocationCallbacks(allocation);
    
    return allocation;
}

// Try to preempt lower priority allocations
bool ResourceManager::tryPreemption(const ResourceRequest& request) {
    // Check if preemption is enabled
    if (!preemptionEnabled_) {
        return false;
    }
    
    // Get requested resources by type
    std::map<ResourceType, double> neededResources;
    for (const auto& pair : request.requirements) {
        ResourceType type = pair.first;
        double amount = pair.second;
        
        // Calculate how much more we need
        auto it = resources_.find(type);
        if (it != resources_.end()) {
            double available = it->second.available;
            if (amount > available) {
                neededResources[type] = amount - available;
            }
        } else {
            // If resource type not found, preemption can't help
            return false;
        }
    }
    
    // If we don't need any more resources, we're good
    if (neededResources.empty()) {
        return true;
    }
    
    // Get all active allocations sorted by priority (lowest first)
    std::vector<std::pair<std::string, ResourceAllocation>> allocations;
    for (const auto& pair : activeAllocations_) {
        allocations.push_back(pair);
    }
    
    // Sort by priority (need to extract priority from allocation)
    std::sort(allocations.begin(), allocations.end(),
        [&](const auto& a, const auto& b) {
            // Get the request IDs
            const std::string& reqIdA = a.second.requestId;
            const std::string& reqIdB = b.second.requestId;
            
            // Find the corresponding requests
            TaskPriority prioA = TaskPriority::NORMAL;
            TaskPriority prioB = TaskPriority::NORMAL;
            
            for (const auto& req : pendingRequests_) {
                if (req.requestId == reqIdA) {
                    prioA = req.priority;
                }
                if (req.requestId == reqIdB) {
                    prioB = req.priority;
                }
            }
            
            return static_cast<int>(prioA) < static_cast<int>(prioB);
        });
    
    // Keep track of resources we've preempted
    std::map<ResourceType, double> preemptedResources;
    for (const auto& type : neededResources) {
        preemptedResources[type.first] = 0.0;
    }
    
    // Try to preempt allocations (lowest priority first)
    std::vector<std::string> allocationsToRelease;
    
    for (const auto& pair : allocations) {
        const std::string& allocationId = pair.first;
        const ResourceAllocation& allocation = pair.second;
        
        // Skip allocations with higher or equal priority
        TaskPriority allocationPriority = TaskPriority::NORMAL;
        for (const auto& req : pendingRequests_) {
            if (req.requestId == allocation.requestId) {
                allocationPriority = req.priority;
                break;
            }
        }
        
        if (static_cast<int>(allocationPriority) >= static_cast<int>(request.priority)) {
            continue;
        }
        
        // Check if this allocation has any resources we need
        bool useful = false;
        for (const auto& resourcePair : allocation.allocated) {
            ResourceType type = resourcePair.first;
            double amount = resourcePair.second;
            
            if (neededResources.find(type) != neededResources.end()) {
                useful = true;
                break;
            }
        }
        
        if (!useful) {
            continue;
        }
        
        // This allocation can be preempted
        allocationsToRelease.push_back(allocationId);
        
        // Update preempted resources
        for (const auto& resourcePair : allocation.allocated) {
            ResourceType type = resourcePair.first;
            double amount = resourcePair.second;
            
            if (neededResources.find(type) != neededResources.end()) {
                preemptedResources[type] += amount;
            }
        }
        
        // Check if we have enough preempted resources
        bool enough = true;
        for (const auto& pair : neededResources) {
            ResourceType type = pair.first;
            double needed = pair.second;
            
            if (preemptedResources[type] < needed) {
                enough = false;
                break;
            }
        }
        
        if (enough) {
            break;
        }
    }
    
    // Check if we have enough preempted resources
    for (const auto& pair : neededResources) {
        ResourceType type = pair.first;
        double needed = pair.second;
        
        if (preemptedResources[type] < needed) {
            // Not enough resources can be preempted
            return false;
        }
    }
    
    // Release the preempted allocations
    for (const auto& allocationId : allocationsToRelease) {
        releaseAllocation(allocationId);
    }
    
    return true;
}

// Update resource usage after allocation or release
void ResourceManager::updateResourceUsage() {
    // Nothing to do here for now
}

// Notify allocation callbacks
void ResourceManager::notifyAllocationCallbacks(const ResourceAllocation& allocation) {
    // Copy the callbacks to avoid mutex deadlock
    std::map<int, std::function<void(const ResourceAllocation&)>> callbacks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks = allocationCallbacks_;
    }
    
    // Call the callbacks
    for (const auto& pair : callbacks) {
        try {
            pair.second(allocation);
        } catch (const std::exception& e) {
            std::cerr << "Error in allocation callback: " << e.what() << std::endl;
        }
    }
}

} // namespace signal
} // namespace tdoa 