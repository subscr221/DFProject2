/**
 * @file parallel_engine.cpp
 * @brief Implementation of ParallelEngine class
 */

#include "parallel_engine.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>

namespace tdoa {
namespace signal {

//-----------------------------------------------------------------------------
// SignalTask Implementation
//-----------------------------------------------------------------------------

// Default constructor
SignalTask::SignalTask()
    : priority(TaskPriority::NORMAL)
    , timestamp(std::chrono::system_clock::now()) {
}

// Constructor with parameters
SignalTask::SignalTask(
    std::shared_ptr<Signal> sig,
    std::function<std::shared_ptr<Signal>()> proc,
    TaskPriority prio,
    const std::string& id
)
    : signal(sig)
    , process(proc)
    , priority(prio)
    , timestamp(std::chrono::system_clock::now())
    , taskId(id) {
    
    // Set signal ID if signal is valid
    if (signal) {
        signalId = signal->getId();
    }
}

// Comparison operator for priority queue
bool SignalTask::operator<(const SignalTask& other) const {
    // First compare priority level (higher priority = higher precedence)
    if (priority != other.priority) {
        return static_cast<int>(priority) < static_cast<int>(other.priority);
    }
    
    // Then compare by timestamp (earlier timestamp = higher precedence)
    return timestamp > other.timestamp;
}

//-----------------------------------------------------------------------------
// ParallelEngine Implementation
//-----------------------------------------------------------------------------

// Singleton instance accessor
ParallelEngine& ParallelEngine::getInstance() {
    static ParallelEngine instance;
    return instance;
}

// Private constructor for singleton
ParallelEngine::ParallelEngine()
    : running_(false)
    , activeThreads_(0)
    , maxQueueSize_(1000)
    , backpressurePolicy_(BackpressurePolicy::BLOCK)
    , totalProcessed_(0)
    , totalDropped_(0)
    , peakQueueSize_(0)
    , totalProcessingTime_(0.0)
    , maxProcessingTime_(0.0) {
    
    // Initialize priority stats
    priorityStats_[TaskPriority::LOW] = 0;
    priorityStats_[TaskPriority::NORMAL] = 0;
    priorityStats_[TaskPriority::HIGH] = 0;
    priorityStats_[TaskPriority::CRITICAL] = 0;
}

// Private destructor for singleton
ParallelEngine::~ParallelEngine() {
    // Ensure shutdown is called
    shutdown();
}

// Initialize the engine
bool ParallelEngine::initialize(size_t numThreads, size_t maxQueueSize) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if already running
    if (running_) {
        std::cerr << "Error: ParallelEngine is already running" << std::endl;
        return false;
    }
    
    // Set running flag
    running_ = true;
    
    // Set max queue size
    maxQueueSize_ = maxQueueSize;
    
    // Auto-detect number of threads if not specified
    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) {
            numThreads = 4; // Default to 4 threads if detection fails
        }
    }
    
    // Create worker threads
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back(&ParallelEngine::workerFunction, this);
    }
    
    // Reserve space for task queue
    tasks_.reserve(maxQueueSize);
    
    return true;
}

// Shutdown the engine
void ParallelEngine::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check if already stopped
        if (!running_) {
            return;
        }
        
        // Set running flag to false
        running_ = false;
    }
    
    // Notify all worker threads
    condition_.notify_all();
    
    // Wait for all worker threads to finish
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    // Clear worker threads
    workers_.clear();
    
    // Clear tasks and reject all pending tasks
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& task : tasks_) {
            task.promise.set_value(nullptr);
        }
        
        tasks_.clear();
    }
}

// Submit a signal processing task
std::future<std::shared_ptr<Signal>> ParallelEngine::submitTask(
    std::shared_ptr<Signal> signal,
    std::function<std::shared_ptr<Signal>()> process,
    TaskPriority priority
) {
    // Check if engine is running
    if (!running_) {
        std::cerr << "Error: ParallelEngine is not running" << std::endl;
        std::promise<std::shared_ptr<Signal>> promise;
        promise.set_value(nullptr);
        return promise.get_future();
    }
    
    // Create a new task
    SignalTask task(signal, process, priority, generateTaskId());
    
    // Get the future before task is moved
    std::future<std::shared_ptr<Signal>> future = task.promise.get_future();
    
    // Add task to queue
    {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Handle backpressure if queue is full
        if (tasks_.size() >= maxQueueSize_) {
            if (!handleBackpressure(task)) {
                // Task was dropped or rejected
                task.promise.set_value(nullptr);
                return future;
            }
        }
        
        // Add task to queue
        tasks_.push_back(std::move(task));
        
        // Update peak queue size
        if (tasks_.size() > peakQueueSize_) {
            peakQueueSize_ = tasks_.size();
        }
    }
    
    // Notify a worker thread
    condition_.notify_one();
    
    return future;
}

// Submit a component processing task
std::future<std::shared_ptr<Signal>> ParallelEngine::submitTask(
    std::shared_ptr<Signal> signal,
    std::shared_ptr<ProcessingComponent> component,
    TaskPriority priority
) {
    // Check parameters
    if (!signal || !component) {
        std::cerr << "Error: Invalid parameters for submitTask" << std::endl;
        std::promise<std::shared_ptr<Signal>> promise;
        promise.set_value(nullptr);
        return promise.get_future();
    }
    
    // Create a processing function for the component
    auto process = [component, signal]() {
        return component->process(signal);
    };
    
    // Submit the task
    return submitTask(signal, process, priority);
}

// Process a signal synchronously
std::shared_ptr<Signal> ParallelEngine::processSync(
    std::shared_ptr<Signal> signal,
    std::function<std::shared_ptr<Signal>()> process
) {
    if (!signal || !process) {
        std::cerr << "Error: Invalid parameters for processSync" << std::endl;
        return nullptr;
    }
    
    // Process the signal synchronously
    auto startTime = std::chrono::high_resolution_clock::now();
    auto result = process();
    auto endTime = std::chrono::high_resolution_clock::now();
    
    // Calculate processing time in ms
    double processingTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    
    // Update statistics
    updateStats(processingTime, TaskPriority::NORMAL);
    
    return result;
}

// Get task statistics
TaskStats ParallelEngine::getStats() const {
    TaskStats stats;
    
    // Get the current queue size
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats.currentQueueSize = tasks_.size();
    }
    
    // Get atomic values
    stats.totalProcessed = totalProcessed_;
    stats.totalDropped = totalDropped_;
    stats.peakQueueSize = peakQueueSize_;
    stats.activeThreads = activeThreads_;
    
    // Calculate average processing time
    double totalTime = totalProcessingTime_;
    size_t totalTasks = totalProcessed_;
    stats.averageProcessingTime = (totalTasks > 0) ? (totalTime / totalTasks) : 0.0;
    
    // Get maximum processing time
    stats.maxProcessingTime = maxProcessingTime_;
    
    // Get priority distribution
    stats.priorityDistribution[TaskPriority::LOW] = priorityStats_[TaskPriority::LOW];
    stats.priorityDistribution[TaskPriority::NORMAL] = priorityStats_[TaskPriority::NORMAL];
    stats.priorityDistribution[TaskPriority::HIGH] = priorityStats_[TaskPriority::HIGH];
    stats.priorityDistribution[TaskPriority::CRITICAL] = priorityStats_[TaskPriority::CRITICAL];
    
    return stats;
}

// Reset task statistics
void ParallelEngine::resetStats() {
    totalProcessed_ = 0;
    totalDropped_ = 0;
    peakQueueSize_ = 0;
    totalProcessingTime_ = 0.0;
    maxProcessingTime_ = 0.0;
    
    priorityStats_[TaskPriority::LOW] = 0;
    priorityStats_[TaskPriority::NORMAL] = 0;
    priorityStats_[TaskPriority::HIGH] = 0;
    priorityStats_[TaskPriority::CRITICAL] = 0;
}

// Set the backpressure policy
void ParallelEngine::setBackpressurePolicy(BackpressurePolicy policy) {
    backpressurePolicy_ = policy;
}

// Get the backpressure policy
BackpressurePolicy ParallelEngine::getBackpressurePolicy() const {
    return backpressurePolicy_;
}

// Set the maximum queue size
void ParallelEngine::setMaxQueueSize(size_t size) {
    if (size == 0) {
        std::cerr << "Error: Maximum queue size must be greater than 0" << std::endl;
        return;
    }
    
    maxQueueSize_ = size;
    
    // Reserve space for task queue
    std::lock_guard<std::mutex> lock(mutex_);
    tasks_.reserve(size);
}

// Get the maximum queue size
size_t ParallelEngine::getMaxQueueSize() const {
    return maxQueueSize_;
}

// Set the number of worker threads
bool ParallelEngine::setNumThreads(size_t numThreads) {
    // Check if engine is running
    if (!running_) {
        std::cerr << "Error: ParallelEngine is not running" << std::endl;
        return false;
    }
    
    // Auto-detect number of threads if not specified
    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) {
            numThreads = 4; // Default to 4 threads if detection fails
        }
    }
    
    // Check if number of threads is the same
    if (numThreads == workers_.size()) {
        return true;
    }
    
    // Shutdown and reinitialize with new thread count
    shutdown();
    return initialize(numThreads, maxQueueSize_);
}

// Get the number of worker threads
size_t ParallelEngine::getNumThreads() const {
    return workers_.size();
}

// Generate a unique task ID
std::string ParallelEngine::generateTaskId() const {
    // Generate a random ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "task-";
    
    // Generate a UUID-like string
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << dis(gen);
    }
    
    return ss.str();
}

// Cancel a task by ID
bool ParallelEngine::cancelTask(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Find the task in the queue
    auto it = findTaskById(taskId);
    if (it != tasks_.end()) {
        // Set the promise to null
        it->promise.set_value(nullptr);
        
        // Remove the task from the queue
        tasks_.erase(it);
        
        // Increment dropped count
        ++totalDropped_;
        
        return true;
    }
    
    return false;
}

// Check if the engine is running
bool ParallelEngine::isRunning() const {
    return running_;
}

// Worker thread function
void ParallelEngine::workerFunction() {
    while (running_) {
        SignalTask task;
        bool hasTask = false;
        
        // Get a task from the queue
        {
            std::unique_lock<std::mutex> lock(mutex_);
            
            // Wait for a task or shutdown
            condition_.wait(lock, [this] {
                return !running_ || !tasks_.empty();
            });
            
            // Check if shutting down
            if (!running_ && tasks_.empty()) {
                break;
            }
            
            // Check if the queue is empty
            if (tasks_.empty()) {
                continue;
            }
            
            // Get the highest priority task
            auto highestPriorityIt = std::max_element(tasks_.begin(), tasks_.end());
            task = std::move(*highestPriorityIt);
            tasks_.erase(highestPriorityIt);
            
            hasTask = true;
            
            // Notify queue space available
            queueSpace_.notify_one();
        }
        
        // Process the task if we have one
        if (hasTask) {
            // Increment active threads
            ++activeThreads_;
            
            // Process the task and time it
            auto startTime = std::chrono::high_resolution_clock::now();
            std::shared_ptr<Signal> result;
            
            try {
                result = task.process();
            } catch (const std::exception& e) {
                std::cerr << "Error processing task " << task.taskId 
                          << ": " << e.what() << std::endl;
                result = nullptr;
            }
            
            auto endTime = std::chrono::high_resolution_clock::now();
            
            // Calculate processing time in ms
            double processingTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
            
            // Update statistics
            updateStats(processingTime, task.priority);
            
            // Set the promise value
            task.promise.set_value(result);
            
            // Decrement active threads
            --activeThreads_;
        }
    }
}

// Handle backpressure
bool ParallelEngine::handleBackpressure(const SignalTask& task) {
    // Get the current policy
    BackpressurePolicy policy = backpressurePolicy_;
    
    switch (policy) {
        case BackpressurePolicy::BLOCK:
        {
            // Wait for space in the queue
            queueSpace_.wait(mutex_, [this] {
                return tasks_.size() < maxQueueSize_ || !running_;
            });
            
            // Check if shutting down
            if (!running_) {
                return false;
            }
            
            // Now there is space in the queue
            return true;
        }
        
        case BackpressurePolicy::DROP_OLDEST:
        {
            // Find and remove the oldest task
            auto oldestIt = findOldestTask();
            if (oldestIt != tasks_.end()) {
                // Set the promise to null
                oldestIt->promise.set_value(nullptr);
                
                // Remove the task from the queue
                tasks_.erase(oldestIt);
                
                // Increment dropped count
                ++totalDropped_;
                
                return true;
            }
            
            // Could not find a task to drop
            return false;
        }
        
        case BackpressurePolicy::DROP_LOWEST_PRIORITY:
        {
            // Find and remove the lowest priority task
            auto lowestIt = findLowestPriorityTask();
            if (lowestIt != tasks_.end()) {
                // Set the promise to null
                lowestIt->promise.set_value(nullptr);
                
                // Remove the task from the queue
                tasks_.erase(lowestIt);
                
                // Increment dropped count
                ++totalDropped_;
                
                return true;
            }
            
            // Could not find a task to drop
            return false;
        }
        
        case BackpressurePolicy::DROP_NEW:
        {
            // Drop the new task
            ++totalDropped_;
            return false;
        }
        
        case BackpressurePolicy::EXPAND_QUEUE:
        {
            // Expand the queue (no backpressure)
            return true;
        }
        
        default:
            // Unknown policy
            std::cerr << "Error: Unknown backpressure policy" << std::endl;
            return false;
    }
}

// Find lowest priority task in queue
std::vector<SignalTask>::iterator ParallelEngine::findLowestPriorityTask() {
    if (tasks_.empty()) {
        return tasks_.end();
    }
    
    // Find the lowest priority task
    auto lowestIt = std::min_element(tasks_.begin(), tasks_.end());
    return lowestIt;
}

// Find oldest task in queue
std::vector<SignalTask>::iterator ParallelEngine::findOldestTask() {
    if (tasks_.empty()) {
        return tasks_.end();
    }
    
    // Find the oldest task
    auto oldestIt = std::min_element(tasks_.begin(), tasks_.end(),
        [](const SignalTask& a, const SignalTask& b) {
            return a.timestamp < b.timestamp;
        });
    
    return oldestIt;
}

// Find task by ID
std::vector<SignalTask>::iterator ParallelEngine::findTaskById(const std::string& taskId) {
    auto it = std::find_if(tasks_.begin(), tasks_.end(),
        [&](const SignalTask& task) {
            return task.taskId == taskId;
        });
    
    return it;
}

// Update task statistics
void ParallelEngine::updateStats(double processingTime, TaskPriority priority) {
    // Increment processed count
    ++totalProcessed_;
    
    // Add to total processing time
    totalProcessingTime_ += processingTime;
    
    // Update maximum processing time
    double currentMax = maxProcessingTime_;
    while (processingTime > currentMax) {
        if (maxProcessingTime_.compare_exchange_weak(currentMax, processingTime)) {
            break;
        }
    }
    
    // Increment priority count
    ++priorityStats_[priority];
}

} // namespace signal
} // namespace tdoa 