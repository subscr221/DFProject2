/**
 * @file signal_prioritizer.h
 * @brief Signal prioritizer for resource allocation
 */

#pragma once

#include "signal.h"
#include "resource_manager.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include <mutex>

namespace tdoa {
namespace signal {

/**
 * @brief Signal priority information
 */
struct SignalPriority {
    std::string signalId;                    ///< Signal ID
    TaskPriority priority;                   ///< Priority level
    double priorityScore;                    ///< Numeric priority score (higher = more priority)
    std::chrono::system_clock::time_point timestamp; ///< When the priority was assigned
    std::map<std::string, double> factors;   ///< Factors that influenced priority
    
    /**
     * @brief Default constructor
     */
    SignalPriority()
        : priority(TaskPriority::NORMAL)
        , priorityScore(0.0)
        , timestamp(std::chrono::system_clock::now()) {
    }
    
    /**
     * @brief Constructor with parameters
     * @param id Signal ID
     * @param prio Priority level
     * @param score Priority score
     */
    SignalPriority(
        const std::string& id,
        TaskPriority prio = TaskPriority::NORMAL,
        double score = 0.0
    )
        : signalId(id)
        , priority(prio)
        , priorityScore(score)
        , timestamp(std::chrono::system_clock::now()) {
    }
};

/**
 * @brief Prioritization policy type
 */
using PrioritizationPolicy = std::function<SignalPriority(std::shared_ptr<Signal>)>;

/**
 * @brief Signal prioritizer for resource allocation
 */
class SignalPrioritizer {
public:
    /**
     * @brief Singleton instance accessor
     * @return Reference to the singleton instance
     */
    static SignalPrioritizer& getInstance();
    
    /**
     * @brief Set the prioritization policy
     * @param policy Function that assigns priority to signals
     */
    void setPrioritizationPolicy(PrioritizationPolicy policy);
    
    /**
     * @brief Set the default policy
     */
    void setDefaultPolicy();
    
    /**
     * @brief Prioritize a signal
     * @param signal Signal to prioritize
     * @return Assigned priority information
     */
    SignalPriority prioritize(std::shared_ptr<Signal> signal);
    
    /**
     * @brief Get priority for a signal
     * @param signalId Signal ID
     * @return Priority information or default if not found
     */
    SignalPriority getPriority(const std::string& signalId) const;
    
    /**
     * @brief Get all signal priorities
     * @return Map of signal IDs to priority information
     */
    std::map<std::string, SignalPriority> getAllPriorities() const;
    
    /**
     * @brief Check if a signal has been prioritized
     * @param signalId Signal ID
     * @return True if the signal has been prioritized
     */
    bool hasPriority(const std::string& signalId) const;
    
    /**
     * @brief Update priority for a signal
     * @param signalId Signal ID
     * @param priority Priority level
     * @param score Priority score
     * @return True if priority was updated
     */
    bool updatePriority(const std::string& signalId, TaskPriority priority, double score = 0.0);
    
    /**
     * @brief Add a prioritization factor
     * @param signalId Signal ID
     * @param factor Factor name
     * @param value Factor value (higher = more priority)
     * @return True if factor was added
     */
    bool addPrioritizationFactor(const std::string& signalId, const std::string& factor, double value);
    
    /**
     * @brief Remove a signal from prioritization
     * @param signalId Signal ID
     * @return True if signal was removed
     */
    bool removeSignal(const std::string& signalId);
    
    /**
     * @brief Get the top N highest priority signals
     * @param count Number of signals to return (0 = all)
     * @return Vector of priority information sorted by priority (highest first)
     */
    std::vector<SignalPriority> getTopPriorities(size_t count = 0) const;
    
    /**
     * @brief Reset all priorities
     */
    void reset();
    
    /**
     * @brief Register a callback for priority changes
     * @param callback Callback function that takes signal ID and priority
     * @return Callback ID
     */
    int registerPriorityCallback(std::function<void(const std::string&, const SignalPriority&)> callback);
    
    /**
     * @brief Unregister a priority callback
     * @param callbackId Callback ID to unregister
     * @return True if callback was unregistered
     */
    bool unregisterPriorityCallback(int callbackId);
    
    /**
     * @brief Preempt lower priority signals based on resource needs
     * @param signal High priority signal to allocate resources for
     * @param resourceRequirements Resource requirements
     * @return True if preemption was successful
     */
    bool preemptForSignal(std::shared_ptr<Signal> signal, const std::map<ResourceType, double>& resourceRequirements);
    
private:
    /**
     * @brief Private constructor for singleton
     */
    SignalPrioritizer();
    
    /**
     * @brief Private destructor for singleton
     */
    ~SignalPrioritizer();
    
    /**
     * @brief Default prioritization policy
     * @param signal Signal to prioritize
     * @return Priority information
     */
    SignalPriority defaultPrioritizationPolicy(std::shared_ptr<Signal> signal);
    
    /**
     * @brief Notify priority callbacks
     * @param signalId Signal ID
     * @param priority Priority information
     */
    void notifyPriorityCallbacks(const std::string& signalId, const SignalPriority& priority);
    
    /**
     * @brief Recalculate priority score based on factors
     * @param priority Priority information to update
     */
    void recalculatePriorityScore(SignalPriority& priority);
    
    PrioritizationPolicy policy_;                                          ///< Prioritization policy
    std::map<std::string, SignalPriority> priorities_;                     ///< Signal priorities
    std::map<int, std::function<void(const std::string&, const SignalPriority&)>> priorityCallbacks_; ///< Priority callbacks
    
    std::atomic<int> nextCallbackId_;                                     ///< Next callback ID
    mutable std::mutex mutex_;                                            ///< Mutex for thread safety
};

} // namespace signal
} // namespace tdoa 