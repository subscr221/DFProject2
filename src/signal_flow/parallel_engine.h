/**
 * @file parallel_engine.h
 * @brief Parallel processing engine for signal processing
 */

#pragma once

#include "signal.h"
#include "processing_component.h"
#include "resource_manager.h"
#include <memory>
#include <vector>
#include <queue>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <chrono>

namespace tdoa {
namespace signal {

/**
 * @brief Signal processing task
 */
struct SignalTask {
    std::shared_ptr<Signal> signal;                   ///< Input signal
    std::function<std::shared_ptr<Signal>()> process; ///< Processing function
    std::promise<std::shared_ptr<Signal>> promise;    ///< Promise for the result
    TaskPriority priority;                            ///< Task priority
    std::chrono::system_clock::time_point timestamp;  ///< Task creation timestamp
    std::string taskId;                               ///< Task ID
    std::string signalId;                             ///< Signal ID
    
    /**
     * @brief Default constructor
     */
    SignalTask();
    
    /**
     * @brief Constructor with parameters
     * @param sig Input signal
     * @param proc Processing function
     * @param prio Task priority
     * @param id Task ID
     */
    SignalTask(
        std::shared_ptr<Signal> sig,
        std::function<std::shared_ptr<Signal>()> proc,
        TaskPriority prio = TaskPriority::NORMAL,
        const std::string& id = ""
    );
    
    /**
     * @brief Comparison operator for priority queue
     * @param other Other task to compare with
     * @return True if this task has lower priority
     */
    bool operator<(const SignalTask& other) const;
};

/**
 * @brief Task statistics
 */
struct TaskStats {
    size_t totalProcessed;          ///< Total tasks processed
    size_t totalDropped;            ///< Total tasks dropped due to overload
    size_t currentQueueSize;        ///< Current queue size
    size_t peakQueueSize;           ///< Peak queue size
    size_t activeThreads;           ///< Number of currently active threads
    double averageProcessingTime;   ///< Average processing time in ms
    double maxProcessingTime;       ///< Maximum processing time in ms
    std::map<TaskPriority, size_t> priorityDistribution; ///< Distribution by priority
};

/**
 * @brief Backpressure policy
 */
enum class BackpressurePolicy {
    BLOCK,              ///< Block until queue has space
    DROP_OLDEST,        ///< Drop oldest task in queue
    DROP_LOWEST_PRIORITY, ///< Drop lowest priority task in queue
    DROP_NEW,           ///< Drop the new task
    EXPAND_QUEUE        ///< Expand the queue (no backpressure)
};

/**
 * @brief Thread pool based parallel processing engine
 */
class ParallelEngine {
public:
    /**
     * @brief Singleton instance accessor
     * @return Reference to the singleton instance
     */
    static ParallelEngine& getInstance();
    
    /**
     * @brief Initialize the engine
     * @param numThreads Number of worker threads (0 = auto-detect)
     * @param maxQueueSize Maximum size of the task queue
     * @return True if initialization successful
     */
    bool initialize(size_t numThreads = 0, size_t maxQueueSize = 1000);
    
    /**
     * @brief Shutdown the engine
     */
    void shutdown();
    
    /**
     * @brief Submit a signal processing task
     * @param signal Input signal
     * @param process Processing function
     * @param priority Task priority
     * @return Future for the result signal
     */
    std::future<std::shared_ptr<Signal>> submitTask(
        std::shared_ptr<Signal> signal,
        std::function<std::shared_ptr<Signal>()> process,
        TaskPriority priority = TaskPriority::NORMAL
    );
    
    /**
     * @brief Submit a component processing task
     * @param signal Input signal
     * @param component Processing component
     * @param priority Task priority
     * @return Future for the result signal
     */
    std::future<std::shared_ptr<Signal>> submitTask(
        std::shared_ptr<Signal> signal,
        std::shared_ptr<ProcessingComponent> component,
        TaskPriority priority = TaskPriority::NORMAL
    );
    
    /**
     * @brief Process a signal synchronously
     * @param signal Input signal
     * @param process Processing function
     * @return Result signal
     */
    std::shared_ptr<Signal> processSync(
        std::shared_ptr<Signal> signal,
        std::function<std::shared_ptr<Signal>()> process
    );
    
    /**
     * @brief Get task statistics
     * @return Task statistics
     */
    TaskStats getStats() const;
    
    /**
     * @brief Reset task statistics
     */
    void resetStats();
    
    /**
     * @brief Set the backpressure policy
     * @param policy Backpressure policy
     */
    void setBackpressurePolicy(BackpressurePolicy policy);
    
    /**
     * @brief Get the backpressure policy
     * @return Current backpressure policy
     */
    BackpressurePolicy getBackpressurePolicy() const;
    
    /**
     * @brief Set the maximum queue size
     * @param size Maximum queue size
     */
    void setMaxQueueSize(size_t size);
    
    /**
     * @brief Get the maximum queue size
     * @return Current maximum queue size
     */
    size_t getMaxQueueSize() const;
    
    /**
     * @brief Set the number of worker threads
     * @param numThreads Number of worker threads (0 = auto-detect)
     * @return True if successful
     */
    bool setNumThreads(size_t numThreads);
    
    /**
     * @brief Get the number of worker threads
     * @return Number of worker threads
     */
    size_t getNumThreads() const;
    
    /**
     * @brief Generate a unique task ID
     * @return Unique ID string
     */
    std::string generateTaskId() const;
    
    /**
     * @brief Cancel a task by ID
     * @param taskId Task ID to cancel
     * @return True if task was cancelled
     */
    bool cancelTask(const std::string& taskId);
    
    /**
     * @brief Check if the engine is running
     * @return True if the engine is running
     */
    bool isRunning() const;
    
private:
    /**
     * @brief Private constructor for singleton
     */
    ParallelEngine();
    
    /**
     * @brief Private destructor for singleton
     */
    ~ParallelEngine();
    
    /**
     * @brief Worker thread function
     */
    void workerFunction();
    
    /**
     * @brief Handle backpressure
     * @param task New task to add
     * @return True if task was added to queue
     */
    bool handleBackpressure(const SignalTask& task);
    
    /**
     * @brief Find lowest priority task in queue
     * @return Iterator to lowest priority task
     */
    std::vector<SignalTask>::iterator findLowestPriorityTask();
    
    /**
     * @brief Find oldest task in queue
     * @return Iterator to oldest task
     */
    std::vector<SignalTask>::iterator findOldestTask();
    
    /**
     * @brief Find task by ID
     * @param taskId Task ID to find
     * @return Iterator to the task or tasks_.end() if not found
     */
    std::vector<SignalTask>::iterator findTaskById(const std::string& taskId);
    
    /**
     * @brief Update task statistics
     * @param processingTime Processing time in milliseconds
     * @param priority Task priority
     */
    void updateStats(double processingTime, TaskPriority priority);
    
    std::vector<std::thread> workers_;                   ///< Worker threads
    std::vector<SignalTask> tasks_;                      ///< Task queue
    std::atomic<bool> running_;                          ///< Running flag
    std::atomic<size_t> activeThreads_;                  ///< Number of active threads
    std::atomic<size_t> maxQueueSize_;                   ///< Maximum queue size
    std::atomic<BackpressurePolicy> backpressurePolicy_; ///< Backpressure policy
    
    // Statistics
    std::atomic<size_t> totalProcessed_;                 ///< Total tasks processed
    std::atomic<size_t> totalDropped_;                   ///< Total tasks dropped
    std::atomic<size_t> peakQueueSize_;                  ///< Peak queue size
    std::atomic<double> totalProcessingTime_;            ///< Total processing time
    std::atomic<double> maxProcessingTime_;              ///< Maximum processing time
    std::map<TaskPriority, std::atomic<size_t>> priorityStats_; ///< Priority statistics
    
    mutable std::mutex mutex_;                          ///< Mutex for thread safety
    std::condition_variable condition_;                 ///< Condition variable for worker threads
    std::condition_variable queueSpace_;                ///< Condition variable for queue space
};

} // namespace signal
} // namespace tdoa 