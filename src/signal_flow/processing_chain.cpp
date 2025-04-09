/**
 * @file processing_chain.cpp
 * @brief Implementation of ProcessingChain and ProcessingComponentFactory classes
 */

#include "processing_chain.h"
#include <iostream>
#include <algorithm>
#include <set>
#include <stdexcept>

namespace tdoa {
namespace signal {

//-----------------------------------------------------------------------------
// ProcessingChain Implementation
//-----------------------------------------------------------------------------

// Constructor with optional name
ProcessingChain::ProcessingChain(const std::string& name)
    : name_(name) {
}

// Destructor
ProcessingChain::~ProcessingChain() {
    // Clear components and edges
    components_.clear();
    edges_.clear();
}

// Add a component to the chain
bool ProcessingChain::addComponent(std::shared_ptr<ProcessingComponent> component) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!component) {
        std::cerr << "Error: Cannot add null component to processing chain" << std::endl;
        return false;
    }
    
    // Check if component with this ID already exists
    if (components_.find(component->getId()) != components_.end()) {
        std::cerr << "Error: Component with ID '" << component->getId() 
                  << "' already exists in processing chain" << std::endl;
        return false;
    }
    
    // Add component
    components_[component->getId()] = component;
    
    return true;
}

// Remove a component from the chain by ID
bool ProcessingChain::removeComponent(const std::string& componentId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if component exists
    if (components_.find(componentId) == components_.end()) {
        std::cerr << "Error: Component with ID '" << componentId 
                  << "' not found in processing chain" << std::endl;
        return false;
    }
    
    // Remove all edges associated with this component
    auto it = edges_.begin();
    while (it != edges_.end()) {
        if (it->sourceComponentId == componentId || it->targetComponentId == componentId) {
            it = edges_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Remove component
    components_.erase(componentId);
    
    return true;
}

// Connect two components
bool ProcessingChain::connectComponents(
    const std::string& sourceComponentId, 
    const std::string& targetComponentId,
    const std::string& label
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if components exist
    if (components_.find(sourceComponentId) == components_.end()) {
        std::cerr << "Error: Source component with ID '" << sourceComponentId 
                  << "' not found in processing chain" << std::endl;
        return false;
    }
    
    if (components_.find(targetComponentId) == components_.end()) {
        std::cerr << "Error: Target component with ID '" << targetComponentId 
                  << "' not found in processing chain" << std::endl;
        return false;
    }
    
    // Check if edge already exists
    for (const auto& edge : edges_) {
        if (edge.sourceComponentId == sourceComponentId && 
            edge.targetComponentId == targetComponentId) {
            std::cerr << "Error: Edge from '" << sourceComponentId 
                      << "' to '" << targetComponentId 
                      << "' already exists in processing chain" << std::endl;
            return false;
        }
    }
    
    // Add edge
    edges_.emplace_back(sourceComponentId, targetComponentId, label);
    
    // Check for cycles
    if (hasCycles()) {
        // Remove the edge we just added
        edges_.pop_back();
        std::cerr << "Error: Adding edge from '" << sourceComponentId 
                  << "' to '" << targetComponentId 
                  << "' would create a cycle in the processing chain" << std::endl;
        return false;
    }
    
    return true;
}

// Disconnect two components
bool ProcessingChain::disconnectComponents(
    const std::string& sourceComponentId, 
    const std::string& targetComponentId
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Find and remove the edge
    auto it = std::find_if(edges_.begin(), edges_.end(), 
        [&](const ProcessingEdge& edge) {
            return edge.sourceComponentId == sourceComponentId && 
                   edge.targetComponentId == targetComponentId;
        });
    
    if (it != edges_.end()) {
        edges_.erase(it);
        return true;
    }
    
    std::cerr << "Error: Edge from '" << sourceComponentId 
              << "' to '" << targetComponentId 
              << "' not found in processing chain" << std::endl;
    return false;
}

// Get a component by ID
std::shared_ptr<ProcessingComponent> ProcessingChain::getComponent(const std::string& componentId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = components_.find(componentId);
    if (it != components_.end()) {
        return it->second;
    }
    
    return nullptr;
}

// Get all components
const std::map<std::string, std::shared_ptr<ProcessingComponent>>& ProcessingChain::getComponents() const {
    return components_;
}

// Get all edges
const std::vector<ProcessingEdge>& ProcessingChain::getEdges() const {
    return edges_;
}

// Get all source components (no incoming edges)
std::vector<std::string> ProcessingChain::getSourceComponentIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> sources;
    std::set<std::string> targets;
    
    // Collect all target component IDs
    for (const auto& edge : edges_) {
        targets.insert(edge.targetComponentId);
    }
    
    // Components that are not targets are sources
    for (const auto& component : components_) {
        if (targets.find(component.first) == targets.end()) {
            sources.push_back(component.first);
        }
    }
    
    return sources;
}

// Get all sink components (no outgoing edges)
std::vector<std::string> ProcessingChain::getSinkComponentIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> sinks;
    std::set<std::string> sources;
    
    // Collect all source component IDs
    for (const auto& edge : edges_) {
        sources.insert(edge.sourceComponentId);
    }
    
    // Components that are not sources are sinks
    for (const auto& component : components_) {
        if (sources.find(component.first) == sources.end()) {
            sinks.push_back(component.first);
        }
    }
    
    return sinks;
}

// Get the next components in the chain from a given component
std::vector<std::string> ProcessingChain::getNextComponentIds(const std::string& componentId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> nextIds;
    
    for (const auto& edge : edges_) {
        if (edge.sourceComponentId == componentId) {
            nextIds.push_back(edge.targetComponentId);
        }
    }
    
    return nextIds;
}

// Get the previous components in the chain from a given component
std::vector<std::string> ProcessingChain::getPreviousComponentIds(const std::string& componentId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> prevIds;
    
    for (const auto& edge : edges_) {
        if (edge.targetComponentId == componentId) {
            prevIds.push_back(edge.sourceComponentId);
        }
    }
    
    return prevIds;
}

// Process a signal through the chain
std::shared_ptr<Signal> ProcessingChain::process(
    std::shared_ptr<Signal> signal,
    const std::string& sourceComponentId
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!signal) {
        std::cerr << "Error: Cannot process null signal" << std::endl;
        return nullptr;
    }
    
    // Check if the chain is empty
    if (components_.empty()) {
        std::cerr << "Error: Processing chain is empty" << std::endl;
        return signal;
    }
    
    // Determine the source component(s) to start from
    std::vector<std::string> startComponentIds;
    
    if (!sourceComponentId.empty()) {
        // Check if the specified source component exists
        if (components_.find(sourceComponentId) == components_.end()) {
            std::cerr << "Error: Source component with ID '" << sourceComponentId 
                    << "' not found in processing chain" << std::endl;
            return nullptr;
        }
        
        startComponentIds.push_back(sourceComponentId);
    } else {
        // Use all source components (no incoming edges)
        startComponentIds = getSourceComponentIds();
        
        if (startComponentIds.empty()) {
            // If there are no source components, the graph might have cycles
            // Just use the first component as a starting point
            if (!components_.empty()) {
                startComponentIds.push_back(components_.begin()->first);
            } else {
                return signal;
            }
        }
    }
    
    // Process through each starting component
    std::shared_ptr<Signal> result = signal;
    
    for (const auto& componentId : startComponentIds) {
        std::set<std::string> visited;
        result = processComponent(result, componentId, visited);
        
        if (!result) {
            std::cerr << "Error: Processing failed at component '" << componentId << "'" << std::endl;
            return nullptr;
        }
    }
    
    return result;
}

// Set a callback for signal processing
void ProcessingChain::setProcessingCallback(ProcessingCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    processingCallback_ = callback;
}

// Reset the processing chain
void ProcessingChain::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Reset all components
    for (auto& pair : components_) {
        pair.second->reset();
    }
}

// Validate the chain topology
bool ProcessingChain::validate(std::string& errorMsg) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if all components in edges exist
    for (const auto& edge : edges_) {
        if (components_.find(edge.sourceComponentId) == components_.end()) {
            errorMsg = "Error: Source component with ID '" + edge.sourceComponentId 
                     + "' in edge not found in processing chain";
            return false;
        }
        
        if (components_.find(edge.targetComponentId) == components_.end()) {
            errorMsg = "Error: Target component with ID '" + edge.targetComponentId 
                     + "' in edge not found in processing chain";
            return false;
        }
    }
    
    // Check for cycles
    if (hasCycles()) {
        errorMsg = "Error: Processing chain contains cycles";
        return false;
    }
    
    return true;
}

// Check if the chain has cycles
bool ProcessingChain::hasCycles() const {
    std::set<std::string> visited;
    std::set<std::string> recursionStack;
    
    // Check for cycles starting from each component
    for (const auto& component : components_) {
        if (findCycleDFS(component.first, visited, recursionStack)) {
            return true;
        }
    }
    
    return false;
}

// Find cycles in the graph using DFS
bool ProcessingChain::findCycleDFS(
    const std::string& componentId,
    std::set<std::string>& visited,
    std::set<std::string>& recursionStack
) const {
    // If not visited yet
    if (visited.find(componentId) == visited.end()) {
        // Mark as visited and add to recursion stack
        visited.insert(componentId);
        recursionStack.insert(componentId);
        
        // Visit all adjacent components
        for (const auto& edge : edges_) {
            if (edge.sourceComponentId == componentId) {
                // If the adjacent component is in the recursion stack, we found a cycle
                if (recursionStack.find(edge.targetComponentId) != recursionStack.end()) {
                    return true;
                }
                
                // If not visited yet and we find a cycle in the subtree
                if (visited.find(edge.targetComponentId) == visited.end() &&
                    findCycleDFS(edge.targetComponentId, visited, recursionStack)) {
                    return true;
                }
            }
        }
    }
    
    // Remove from recursion stack when done with this component
    recursionStack.erase(componentId);
    
    return false;
}

// Process a signal through a specific component and its successors
std::shared_ptr<Signal> ProcessingChain::processComponent(
    std::shared_ptr<Signal> signal,
    const std::string& componentId,
    std::set<std::string>& visited
) {
    // Avoid processing the same component multiple times (could happen in DAGs with multiple paths)
    if (visited.find(componentId) != visited.end()) {
        return signal;
    }
    
    visited.insert(componentId);
    
    // Get the component
    auto component = components_[componentId];
    
    // Skip disabled components
    if (!component->isEnabled()) {
        // Get the next components
        auto nextIds = getNextComponentIds(componentId);
        
        // If there are no next components, return the signal as is
        if (nextIds.empty()) {
            return signal;
        }
        
        // Process through each next component
        std::shared_ptr<Signal> result = signal;
        
        for (const auto& nextId : nextIds) {
            result = processComponent(result, nextId, visited);
            
            if (!result) {
                return nullptr;
            }
        }
        
        return result;
    }
    
    // Process the signal
    auto result = component->process(signal);
    
    // Call the processing callback if set
    if (processingCallback_) {
        processingCallback_(result, component->getState());
    }
    
    // If processing failed, return null
    if (!result) {
        return nullptr;
    }
    
    // Get the next components
    auto nextIds = getNextComponentIds(componentId);
    
    // If there are no next components, return the processed signal
    if (nextIds.empty()) {
        return result;
    }
    
    // Process through each next component
    for (const auto& nextId : nextIds) {
        result = processComponent(result, nextId, visited);
        
        if (!result) {
            return nullptr;
        }
    }
    
    return result;
}

//-----------------------------------------------------------------------------
// ProcessingComponentFactory Implementation
//-----------------------------------------------------------------------------

// Get the registry of component creation functions
std::map<std::string, std::function<std::shared_ptr<ProcessingComponent>(const std::string&, const ComponentConfig&)>>& 
ProcessingComponentFactory::getRegistry() {
    static std::map<std::string, std::function<std::shared_ptr<ProcessingComponent>(const std::string&, const ComponentConfig&)>> registry;
    return registry;
}

// Register a component creation function
void ProcessingComponentFactory::registerComponentType(
    const std::string& componentType,
    std::function<std::shared_ptr<ProcessingComponent>(const std::string&, const ComponentConfig&)> creationFunc
) {
    if (!creationFunc) {
        std::cerr << "Error: Cannot register null creation function for component type '" 
                  << componentType << "'" << std::endl;
        return;
    }
    
    getRegistry()[componentType] = creationFunc;
}

// Create a component by type
std::shared_ptr<ProcessingComponent> ProcessingComponentFactory::createComponent(
    const std::string& componentType,
    const std::string& id,
    const ComponentConfig& config
) {
    const auto& registry = getRegistry();
    auto it = registry.find(componentType);
    
    if (it != registry.end()) {
        return it->second(id, config);
    }
    
    std::cerr << "Error: Unknown component type '" << componentType << "'" << std::endl;
    return nullptr;
}

// Get all registered component types
std::vector<std::string> ProcessingComponentFactory::getRegisteredComponentTypes() {
    std::vector<std::string> types;
    const auto& registry = getRegistry();
    
    for (const auto& entry : registry) {
        types.push_back(entry.first);
    }
    
    return types;
}

} // namespace signal
} // namespace tdoa 