/**
 * @file processing_chain.h
 * @brief Processing chain for signal processing components
 */

#pragma once

#include "processing_component.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include <mutex>

namespace tdoa {
namespace signal {

/**
 * @brief Edge connecting components in the processing chain
 */
struct ProcessingEdge {
    std::string sourceComponentId;    ///< Source component ID
    std::string targetComponentId;    ///< Target component ID
    std::string label;                ///< Optional edge label
    
    ProcessingEdge() = default;
    
    ProcessingEdge(const std::string& sourceId, const std::string& targetId, const std::string& edgeLabel = "")
        : sourceComponentId(sourceId)
        , targetComponentId(targetId)
        , label(edgeLabel) {
    }
};

/**
 * @brief Class managing the processing chain topology and execution
 */
class ProcessingChain {
public:
    /**
     * @brief Constructor with optional name
     * @param name Chain name
     */
    ProcessingChain(const std::string& name = "ProcessingChain");
    
    /**
     * @brief Destructor
     */
    virtual ~ProcessingChain();
    
    /**
     * @brief Add a component to the chain
     * @param component Component to add
     * @return True if component was added
     */
    bool addComponent(std::shared_ptr<ProcessingComponent> component);
    
    /**
     * @brief Remove a component from the chain by ID
     * @param componentId ID of the component to remove
     * @return True if component was removed
     */
    bool removeComponent(const std::string& componentId);
    
    /**
     * @brief Connect two components
     * @param sourceComponentId Source component ID
     * @param targetComponentId Target component ID
     * @param label Optional edge label
     * @return True if components were connected
     */
    bool connectComponents(
        const std::string& sourceComponentId, 
        const std::string& targetComponentId,
        const std::string& label = ""
    );
    
    /**
     * @brief Disconnect two components
     * @param sourceComponentId Source component ID
     * @param targetComponentId Target component ID
     * @return True if components were disconnected
     */
    bool disconnectComponents(
        const std::string& sourceComponentId, 
        const std::string& targetComponentId
    );
    
    /**
     * @brief Get a component by ID
     * @param componentId Component ID
     * @return Component pointer or nullptr if not found
     */
    std::shared_ptr<ProcessingComponent> getComponent(const std::string& componentId) const;
    
    /**
     * @brief Get all components
     * @return Map of component IDs to components
     */
    const std::map<std::string, std::shared_ptr<ProcessingComponent>>& getComponents() const;
    
    /**
     * @brief Get all edges
     * @return Vector of edges
     */
    const std::vector<ProcessingEdge>& getEdges() const;
    
    /**
     * @brief Get all source components (no incoming edges)
     * @return Vector of source component IDs
     */
    std::vector<std::string> getSourceComponentIds() const;
    
    /**
     * @brief Get all sink components (no outgoing edges)
     * @return Vector of sink component IDs
     */
    std::vector<std::string> getSinkComponentIds() const;
    
    /**
     * @brief Get the next components in the chain from a given component
     * @param componentId Source component ID
     * @return Vector of next component IDs
     */
    std::vector<std::string> getNextComponentIds(const std::string& componentId) const;
    
    /**
     * @brief Get the previous components in the chain from a given component
     * @param componentId Target component ID
     * @return Vector of previous component IDs
     */
    std::vector<std::string> getPreviousComponentIds(const std::string& componentId) const;
    
    /**
     * @brief Process a signal through the chain
     * @param signal Input signal
     * @param sourceComponentId Optional source component ID to start processing from
     * @return Output signal or nullptr if processing failed
     */
    std::shared_ptr<Signal> process(
        std::shared_ptr<Signal> signal,
        const std::string& sourceComponentId = ""
    );
    
    /**
     * @brief Set a callback for signal processing
     * @param callback Callback function
     */
    void setProcessingCallback(ProcessingCallback callback);
    
    /**
     * @brief Reset the processing chain
     */
    void reset();
    
    /**
     * @brief Validate the chain topology
     * @param errorMsg Error message if validation fails
     * @return True if chain is valid
     */
    bool validate(std::string& errorMsg) const;
    
    /**
     * @brief Check if the chain has cycles
     * @return True if the chain has cycles
     */
    bool hasCycles() const;
    
    /**
     * @brief Get the chain name
     * @return Chain name
     */
    const std::string& getName() const { return name_; }
    
    /**
     * @brief Set the chain name
     * @param name Chain name
     */
    void setName(const std::string& name) { name_ = name; }
    
private:
    /**
     * @brief Find cycles in the graph using DFS
     * @param componentId Current component ID
     * @param visited Set of visited components
     * @param recursionStack Set of components in the current recursion stack
     * @return True if a cycle is found
     */
    bool findCycleDFS(
        const std::string& componentId,
        std::set<std::string>& visited,
        std::set<std::string>& recursionStack
    ) const;
    
    /**
     * @brief Process a signal through a specific component and its successors
     * @param signal Input signal
     * @param componentId Component ID to process
     * @param visited Set of already visited components to avoid cycles
     * @return Output signal or nullptr if processing failed
     */
    std::shared_ptr<Signal> processComponent(
        std::shared_ptr<Signal> signal,
        const std::string& componentId,
        std::set<std::string>& visited
    );
    
    std::string name_;                                                  ///< Chain name
    std::map<std::string, std::shared_ptr<ProcessingComponent>> components_; ///< Components by ID
    std::vector<ProcessingEdge> edges_;                                 ///< Edges between components
    ProcessingCallback processingCallback_;                             ///< Callback for signal processing
    mutable std::mutex mutex_;                                          ///< Mutex for thread safety
};

/**
 * @brief Factory to create standard processing components
 */
class ProcessingComponentFactory {
public:
    /**
     * @brief Register a component creation function
     * @param componentType Component type name
     * @param creationFunc Component creation function
     */
    static void registerComponentType(
        const std::string& componentType,
        std::function<std::shared_ptr<ProcessingComponent>(const std::string&, const ComponentConfig&)> creationFunc
    );
    
    /**
     * @brief Create a component by type
     * @param componentType Component type name
     * @param id Component ID
     * @param config Component configuration
     * @return Created component or nullptr if type not found
     */
    static std::shared_ptr<ProcessingComponent> createComponent(
        const std::string& componentType,
        const std::string& id,
        const ComponentConfig& config
    );
    
    /**
     * @brief Get all registered component types
     * @return Vector of component type names
     */
    static std::vector<std::string> getRegisteredComponentTypes();
    
private:
    /**
     * @brief Get the registry of component creation functions
     * @return Registry map
     */
    static std::map<std::string, std::function<std::shared_ptr<ProcessingComponent>(const std::string&, const ComponentConfig&)>>& getRegistry();
};

} // namespace signal
} // namespace tdoa 