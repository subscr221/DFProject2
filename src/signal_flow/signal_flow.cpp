/**
 * @file signal_flow.cpp
 * @brief Implementation of the SignalFlow class
 */

#include "signal_flow.h"
#include <iostream>

namespace tdoa {
namespace signal {

// Singleton instance accessor
SignalFlow& SignalFlow::getInstance() {
    static SignalFlow instance;
    return instance;
}

// Private constructor for singleton
SignalFlow::SignalFlow() {
    // Initialize components will be done explicitly via initialize() method
}

// Private destructor for singleton
SignalFlow::~SignalFlow() {
    // Ensure shutdown is called
    shutdown();
}

// Initialize the signal flow architecture
bool SignalFlow::initialize(size_t numThreads, size_t maxQueueSize) {
    // Initialize resource manager
    ResourceManager& resourceManager = ResourceManager::getInstance();
    if (!resourceManager.initialize()) {
        std::cerr << "Error: Failed to initialize resource manager" << std::endl;
        return false;
    }
    
    // Enable preemption
    resourceManager.setPreemptionPolicy(true);
    
    // Initialize parallel engine
    ParallelEngine& parallelEngine = ParallelEngine::getInstance();
    if (!parallelEngine.initialize(numThreads, maxQueueSize)) {
        std::cerr << "Error: Failed to initialize parallel engine" << std::endl;
        return false;
    }
    
    // Signal prioritizer doesn't need explicit initialization
    
    return true;
}

// Shutdown the signal flow architecture
void SignalFlow::shutdown() {
    // Shutdown parallel engine
    ParallelEngine& parallelEngine = ParallelEngine::getInstance();
    parallelEngine.shutdown();
    
    // Reset resource manager
    ResourceManager& resourceManager = ResourceManager::getInstance();
    resourceManager.reset();
    
    // Reset signal prioritizer
    SignalPrioritizer& signalPrioritizer = SignalPrioritizer::getInstance();
    signalPrioritizer.reset();
}

// Create a processing chain
std::shared_ptr<ProcessingChain> SignalFlow::createChain(const std::string& name) {
    return std::make_shared<ProcessingChain>(name);
}

// Register a standard component type
void SignalFlow::registerComponentType(
    const std::string& componentType,
    std::function<std::shared_ptr<ProcessingComponent>(const std::string&, const ComponentConfig&)> creationFunc
) {
    ProcessingComponentFactory::registerComponentType(componentType, creationFunc);
}

// Create a standard component
std::shared_ptr<ProcessingComponent> SignalFlow::createComponent(
    const std::string& componentType,
    const std::string& id,
    const ComponentConfig& config
) {
    return ProcessingComponentFactory::createComponent(componentType, id, config);
}

// Get all registered component types
std::vector<std::string> SignalFlow::getRegisteredComponentTypes() {
    return ProcessingComponentFactory::getRegisteredComponentTypes();
}

// Process a signal through a chain
std::future<std::shared_ptr<Signal>> SignalFlow::processAsync(
    std::shared_ptr<Signal> signal,
    std::shared_ptr<ProcessingChain> chain,
    TaskPriority priority
) {
    // Check parameters
    if (!signal || !chain) {
        std::cerr << "Error: Invalid parameters for processAsync" << std::endl;
        std::promise<std::shared_ptr<Signal>> promise;
        promise.set_value(nullptr);
        return promise.get_future();
    }
    
    // Create a processing function for the chain
    auto processFunc = [chain, signal]() {
        return chain->process(signal);
    };
    
    // Prioritize the signal if not already prioritized
    SignalPrioritizer& prioritizer = SignalPrioritizer::getInstance();
    if (!prioritizer.hasPriority(signal->getId())) {
        prioritizer.prioritize(signal);
    }
    
    // Submit the task to the parallel engine
    ParallelEngine& parallelEngine = ParallelEngine::getInstance();
    return parallelEngine.submitTask(signal, processFunc, priority);
}

// Process a signal through a chain synchronously
std::shared_ptr<Signal> SignalFlow::processSync(
    std::shared_ptr<Signal> signal,
    std::shared_ptr<ProcessingChain> chain
) {
    // Check parameters
    if (!signal || !chain) {
        std::cerr << "Error: Invalid parameters for processSync" << std::endl;
        return nullptr;
    }
    
    // Process the signal directly
    return chain->process(signal);
}

// Process a signal through a component
std::future<std::shared_ptr<Signal>> SignalFlow::processAsync(
    std::shared_ptr<Signal> signal,
    std::shared_ptr<ProcessingComponent> component,
    TaskPriority priority
) {
    // Check parameters
    if (!signal || !component) {
        std::cerr << "Error: Invalid parameters for processAsync" << std::endl;
        std::promise<std::shared_ptr<Signal>> promise;
        promise.set_value(nullptr);
        return promise.get_future();
    }
    
    // Prioritize the signal if not already prioritized
    SignalPrioritizer& prioritizer = SignalPrioritizer::getInstance();
    if (!prioritizer.hasPriority(signal->getId())) {
        prioritizer.prioritize(signal);
    }
    
    // Submit the task to the parallel engine
    ParallelEngine& parallelEngine = ParallelEngine::getInstance();
    return parallelEngine.submitTask(signal, component, priority);
}

// Process a signal through a component synchronously
std::shared_ptr<Signal> SignalFlow::processSync(
    std::shared_ptr<Signal> signal,
    std::shared_ptr<ProcessingComponent> component
) {
    // Check parameters
    if (!signal || !component) {
        std::cerr << "Error: Invalid parameters for processSync" << std::endl;
        return nullptr;
    }
    
    // Process the signal directly
    return component->process(signal);
}

// Get the resource manager
ResourceManager& SignalFlow::getResourceManager() {
    return ResourceManager::getInstance();
}

// Get the signal prioritizer
SignalPrioritizer& SignalFlow::getSignalPrioritizer() {
    return SignalPrioritizer::getInstance();
}

// Get the parallel engine
ParallelEngine& SignalFlow::getParallelEngine() {
    return ParallelEngine::getInstance();
}

} // namespace signal
} // namespace tdoa 