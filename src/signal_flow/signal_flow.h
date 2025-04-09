/**
 * @file signal_flow.h
 * @brief Signal flow architecture core components
 */

#pragma once

#include "signal.h"
#include "signal_metadata.h"
#include "processing_state.h"
#include "processing_component.h"
#include "processing_chain.h"
#include "resource_manager.h"
#include "signal_prioritizer.h"
#include "parallel_engine.h"
#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace tdoa {
namespace signal {

/**
 * @brief Signal flow architecture class
 * 
 * This class provides a high-level API for working with the signal flow architecture.
 * It brings together all the core components: signals, processing components, chains,
 * resource management, prioritization, and parallel processing.
 */
class SignalFlow {
public:
    /**
     * @brief Singleton instance accessor
     * @return Reference to the singleton instance
     */
    static SignalFlow& getInstance();
    
    /**
     * @brief Initialize the signal flow architecture
     * @param numThreads Number of worker threads (0 = auto-detect)
     * @param maxQueueSize Maximum size of the task queue
     * @return True if initialization successful
     */
    bool initialize(size_t numThreads = 0, size_t maxQueueSize = 1000);
    
    /**
     * @brief Shutdown the signal flow architecture
     */
    void shutdown();
    
    /**
     * @brief Create a processing chain
     * @param name Chain name
     * @return Shared pointer to the created chain
     */
    std::shared_ptr<ProcessingChain> createChain(const std::string& name = "ProcessingChain");
    
    /**
     * @brief Register a standard component type
     * @param componentType Component type name
     * @param creationFunc Component creation function
     */
    void registerComponentType(
        const std::string& componentType,
        std::function<std::shared_ptr<ProcessingComponent>(const std::string&, const ComponentConfig&)> creationFunc
    );
    
    /**
     * @brief Create a standard component
     * @param componentType Component type name
     * @param id Component ID
     * @param config Component configuration
     * @return Created component or nullptr if type not found
     */
    std::shared_ptr<ProcessingComponent> createComponent(
        const std::string& componentType,
        const std::string& id,
        const ComponentConfig& config
    );
    
    /**
     * @brief Get all registered component types
     * @return Vector of component type names
     */
    std::vector<std::string> getRegisteredComponentTypes();
    
    /**
     * @brief Process a signal through a chain
     * @param signal Input signal
     * @param chain Processing chain
     * @param priority Task priority
     * @return Future for the result signal
     */
    std::future<std::shared_ptr<Signal>> processAsync(
        std::shared_ptr<Signal> signal,
        std::shared_ptr<ProcessingChain> chain,
        TaskPriority priority = TaskPriority::NORMAL
    );
    
    /**
     * @brief Process a signal through a chain synchronously
     * @param signal Input signal
     * @param chain Processing chain
     * @return Result signal
     */
    std::shared_ptr<Signal> processSync(
        std::shared_ptr<Signal> signal,
        std::shared_ptr<ProcessingChain> chain
    );
    
    /**
     * @brief Process a signal through a component
     * @param signal Input signal
     * @param component Processing component
     * @param priority Task priority
     * @return Future for the result signal
     */
    std::future<std::shared_ptr<Signal>> processAsync(
        std::shared_ptr<Signal> signal,
        std::shared_ptr<ProcessingComponent> component,
        TaskPriority priority = TaskPriority::NORMAL
    );
    
    /**
     * @brief Process a signal through a component synchronously
     * @param signal Input signal
     * @param component Processing component
     * @return Result signal
     */
    std::shared_ptr<Signal> processSync(
        std::shared_ptr<Signal> signal,
        std::shared_ptr<ProcessingComponent> component
    );
    
    /**
     * @brief Get the resource manager
     * @return Reference to the resource manager
     */
    ResourceManager& getResourceManager();
    
    /**
     * @brief Get the signal prioritizer
     * @return Reference to the signal prioritizer
     */
    SignalPrioritizer& getSignalPrioritizer();
    
    /**
     * @brief Get the parallel engine
     * @return Reference to the parallel engine
     */
    ParallelEngine& getParallelEngine();
    
private:
    /**
     * @brief Private constructor for singleton
     */
    SignalFlow();
    
    /**
     * @brief Private destructor for singleton
     */
    ~SignalFlow();
};

} // namespace signal
} // namespace tdoa 