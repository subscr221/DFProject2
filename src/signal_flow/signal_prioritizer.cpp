/**
 * @file signal_prioritizer.cpp
 * @brief Implementation of SignalPrioritizer class
 */

#include "signal_prioritizer.h"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace tdoa {
namespace signal {

// Singleton instance accessor
SignalPrioritizer& SignalPrioritizer::getInstance() {
    static SignalPrioritizer instance;
    return instance;
}

// Private constructor for singleton
SignalPrioritizer::SignalPrioritizer()
    : nextCallbackId_(0) {
    
    // Set the default policy
    setDefaultPolicy();
}

// Private destructor for singleton
SignalPrioritizer::~SignalPrioritizer() {
    // Clean up
    reset();
}

// Set the prioritization policy
void SignalPrioritizer::setPrioritizationPolicy(PrioritizationPolicy policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (policy) {
        policy_ = policy;
    } else {
        // Set default policy if null
        setDefaultPolicy();
    }
}

// Set the default policy
void SignalPrioritizer::setDefaultPolicy() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    policy_ = [this](std::shared_ptr<Signal> signal) {
        return defaultPrioritizationPolicy(signal);
    };
}

// Prioritize a signal
SignalPriority SignalPrioritizer::prioritize(std::shared_ptr<Signal> signal) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!signal) {
        std::cerr << "Error: Cannot prioritize null signal" << std::endl;
        return SignalPriority();
    }
    
    // Check if signal already has a priority
    const std::string& signalId = signal->getId();
    auto it = priorities_.find(signalId);
    if (it != priorities_.end()) {
        return it->second;
    }
    
    // Apply the policy
    SignalPriority priority = policy_(signal);
    
    // Ensure the signal ID is set
    priority.signalId = signalId;
    
    // Store the priority
    priorities_[signalId] = priority;
    
    // Notify callbacks
    notifyPriorityCallbacks(signalId, priority);
    
    return priority;
}

// Get priority for a signal
SignalPriority SignalPrioritizer::getPriority(const std::string& signalId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = priorities_.find(signalId);
    if (it != priorities_.end()) {
        return it->second;
    }
    
    // Return default priority if not found
    return SignalPriority(signalId);
}

// Get all signal priorities
std::map<std::string, SignalPriority> SignalPrioritizer::getAllPriorities() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return priorities_;
}

// Check if a signal has been prioritized
bool SignalPrioritizer::hasPriority(const std::string& signalId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return priorities_.find(signalId) != priorities_.end();
}

// Update priority for a signal
bool SignalPrioritizer::updatePriority(const std::string& signalId, TaskPriority priority, double score) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = priorities_.find(signalId);
    if (it == priorities_.end()) {
        // Create a new priority entry
        SignalPriority newPriority(signalId, priority, score);
        priorities_[signalId] = newPriority;
        
        // Notify callbacks
        notifyPriorityCallbacks(signalId, newPriority);
        
        return true;
    }
    
    // Update existing priority
    it->second.priority = priority;
    
    // Update score if provided
    if (score > 0.0) {
        it->second.priorityScore = score;
    } else {
        // Recalculate score based on factors
        recalculatePriorityScore(it->second);
    }
    
    // Update timestamp
    it->second.timestamp = std::chrono::system_clock::now();
    
    // Notify callbacks
    notifyPriorityCallbacks(signalId, it->second);
    
    return true;
}

// Add a prioritization factor
bool SignalPrioritizer::addPrioritizationFactor(const std::string& signalId, const std::string& factor, double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = priorities_.find(signalId);
    if (it == priorities_.end()) {
        // Create a new priority entry with default priority
        SignalPriority newPriority(signalId);
        newPriority.factors[factor] = value;
        
        // Calculate score based on factors
        recalculatePriorityScore(newPriority);
        
        priorities_[signalId] = newPriority;
        
        // Notify callbacks
        notifyPriorityCallbacks(signalId, newPriority);
        
        return true;
    }
    
    // Update existing factor or add new one
    it->second.factors[factor] = value;
    
    // Recalculate score based on factors
    recalculatePriorityScore(it->second);
    
    // Update timestamp
    it->second.timestamp = std::chrono::system_clock::now();
    
    // Notify callbacks
    notifyPriorityCallbacks(signalId, it->second);
    
    return true;
}

// Remove a signal from prioritization
bool SignalPrioritizer::removeSignal(const std::string& signalId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = priorities_.find(signalId);
    if (it == priorities_.end()) {
        return false;
    }
    
    // Remove the signal
    priorities_.erase(it);
    
    return true;
}

// Get the top N highest priority signals
std::vector<SignalPriority> SignalPrioritizer::getTopPriorities(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<SignalPriority> result;
    
    // Convert map to vector
    for (const auto& pair : priorities_) {
        result.push_back(pair.second);
    }
    
    // Sort by priority (highest first)
    std::sort(result.begin(), result.end(),
        [](const SignalPriority& a, const SignalPriority& b) {
            // First sort by priority level
            if (a.priority != b.priority) {
                return static_cast<int>(a.priority) > static_cast<int>(b.priority);
            }
            
            // Then sort by score
            return a.priorityScore > b.priorityScore;
        });
    
    // Limit to count if requested
    if (count > 0 && count < result.size()) {
        result.resize(count);
    }
    
    return result;
}

// Reset all priorities
void SignalPrioritizer::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    priorities_.clear();
}

// Register a callback for priority changes
int SignalPrioritizer::registerPriorityCallback(std::function<void(const std::string&, const SignalPriority&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!callback) {
        std::cerr << "Error: Cannot register null callback" << std::endl;
        return -1;
    }
    
    int id = nextCallbackId_++;
    priorityCallbacks_[id] = callback;
    
    return id;
}

// Unregister a priority callback
bool SignalPrioritizer::unregisterPriorityCallback(int callbackId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = priorityCallbacks_.find(callbackId);
    if (it == priorityCallbacks_.end()) {
        std::cerr << "Error: Callback with ID " << callbackId << " not found" << std::endl;
        return false;
    }
    
    priorityCallbacks_.erase(it);
    return true;
}

// Preempt lower priority signals based on resource needs
bool SignalPrioritizer::preemptForSignal(
    std::shared_ptr<Signal> signal,
    const std::map<ResourceType, double>& resourceRequirements
) {
    if (!signal) {
        std::cerr << "Error: Cannot preempt for null signal" << std::endl;
        return false;
    }
    
    // Prioritize the signal if not already prioritized
    const std::string& signalId = signal->getId();
    if (!hasPriority(signalId)) {
        prioritize(signal);
    }
    
    // Get the signal priority
    SignalPriority priority = getPriority(signalId);
    
    // Create a resource request
    ResourceRequest request;
    request.requestId = signalId;
    request.requirements = resourceRequirements;
    request.priority = priority.priority;
    request.clientId = "SignalPrioritizer";
    
    // Request allocation from resource manager
    ResourceManager& resourceManager = ResourceManager::getInstance();
    
    // Enable preemption
    bool preemptionWasEnabled = resourceManager.isPreemptionEnabled();
    resourceManager.setPreemptionPolicy(true);
    
    // Request allocation
    ResourceAllocation allocation = resourceManager.requestAllocation(request);
    
    // Restore preemption setting
    resourceManager.setPreemptionPolicy(preemptionWasEnabled);
    
    // Return success
    return allocation.success;
}

// Default prioritization policy
SignalPriority SignalPrioritizer::defaultPrioritizationPolicy(std::shared_ptr<Signal> signal) {
    SignalPriority priority(signal->getId());
    
    // Set default priority based on simple heuristics
    double bandwidth = signal->getBandwidth();
    double sampleRate = signal->getSampleRate();
    double duration = signal->getDuration();
    
    // Add factors based on signal properties
    
    // Higher bandwidth signals are more important
    if (bandwidth > 0.0) {
        double bandwidthFactor = std::log10(1.0 + bandwidth / 1000.0); // Normalize in kHz
        priority.factors["bandwidth"] = bandwidthFactor;
    }
    
    // Higher sample rate signals are more important
    if (sampleRate > 0.0) {
        double sampleRateFactor = std::log10(1.0 + sampleRate / 1000.0); // Normalize in kHz
        priority.factors["sample_rate"] = sampleRateFactor;
    }
    
    // Longer signals are less important (to prioritize quick processing)
    if (duration > 0.0) {
        double durationFactor = 1.0 / (1.0 + duration);
        priority.factors["duration"] = durationFactor;
    }
    
    // Check metadata for priority hints
    if (signal->hasMetadata("priority")) {
        std::string priorityStr = signal->getMetadata("priority");
        try {
            TaskPriority prio = stringToTaskPriority(priorityStr);
            priority.priority = prio;
            priority.factors["explicit_priority"] = static_cast<double>(static_cast<int>(prio)) + 1.0;
        } catch (const std::exception& e) {
            // Ignore invalid priority string
        }
    }
    
    // Check for signal type in metadata
    if (signal->hasMetadata("signal_type")) {
        std::string signalType = signal->getMetadata("signal_type");
        
        // Assign priority based on signal type
        if (signalType == "emergency") {
            priority.priority = TaskPriority::CRITICAL;
            priority.factors["emergency"] = 10.0;
        } else if (signalType == "control") {
            priority.priority = TaskPriority::HIGH;
            priority.factors["control"] = 5.0;
        } else if (signalType == "telemetry") {
            priority.priority = TaskPriority::NORMAL;
            priority.factors["telemetry"] = 3.0;
        } else if (signalType == "background") {
            priority.priority = TaskPriority::LOW;
            priority.factors["background"] = 1.0;
        }
    }
    
    // Recalculate score based on factors
    recalculatePriorityScore(priority);
    
    return priority;
}

// Notify priority callbacks
void SignalPrioritizer::notifyPriorityCallbacks(const std::string& signalId, const SignalPriority& priority) {
    // Copy the callbacks to avoid mutex deadlock
    std::map<int, std::function<void(const std::string&, const SignalPriority&)>> callbacks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks = priorityCallbacks_;
    }
    
    // Call the callbacks
    for (const auto& pair : callbacks) {
        try {
            pair.second(signalId, priority);
        } catch (const std::exception& e) {
            std::cerr << "Error in priority callback: " << e.what() << std::endl;
        }
    }
}

// Recalculate priority score based on factors
void SignalPrioritizer::recalculatePriorityScore(SignalPriority& priority) {
    // Calculate the priority score as a weighted sum of factors
    double score = 0.0;
    
    // Add priority level base score (0-3)
    score += static_cast<double>(static_cast<int>(priority.priority));
    
    // Add factor scores
    for (const auto& factor : priority.factors) {
        score += factor.second;
    }
    
    priority.priorityScore = score;
}

} // namespace signal
} // namespace tdoa 