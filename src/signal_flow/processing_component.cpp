/**
 * @file processing_component.cpp
 * @brief Implementation of ProcessingComponent and ComponentConfig classes
 */

#include "processing_component.h"
#include <iostream>
#include <sstream>

namespace tdoa {
namespace signal {

//-----------------------------------------------------------------------------
// ComponentConfig Implementation
//-----------------------------------------------------------------------------

// Set a string parameter
void ComponentConfig::setParameter(const std::string& key, const std::string& value) {
    parameters_[key] = value;
}

// Set an integer parameter
void ComponentConfig::setParameter(const std::string& key, int value) {
    parameters_[key] = std::to_string(value);
}

// Set a double parameter
void ComponentConfig::setParameter(const std::string& key, double value) {
    parameters_[key] = std::to_string(value);
}

// Set a boolean parameter
void ComponentConfig::setParameter(const std::string& key, bool value) {
    parameters_[key] = value ? "true" : "false";
}

// Get a string parameter
std::string ComponentConfig::getStringParameter(const std::string& key, const std::string& defaultValue) const {
    auto it = parameters_.find(key);
    if (it != parameters_.end()) {
        return it->second;
    }
    return defaultValue;
}

// Get an integer parameter
int ComponentConfig::getIntParameter(const std::string& key, int defaultValue) const {
    auto it = parameters_.find(key);
    if (it != parameters_.end()) {
        try {
            return std::stoi(it->second);
        } catch (const std::exception& e) {
            // Return default if conversion fails
            return defaultValue;
        }
    }
    return defaultValue;
}

// Get a double parameter
double ComponentConfig::getDoubleParameter(const std::string& key, double defaultValue) const {
    auto it = parameters_.find(key);
    if (it != parameters_.end()) {
        try {
            return std::stod(it->second);
        } catch (const std::exception& e) {
            // Return default if conversion fails
            return defaultValue;
        }
    }
    return defaultValue;
}

// Get a boolean parameter
bool ComponentConfig::getBoolParameter(const std::string& key, bool defaultValue) const {
    auto it = parameters_.find(key);
    if (it != parameters_.end()) {
        std::string value = it->second;
        // Convert to lowercase for comparison
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        
        if (value == "true" || value == "1" || value == "yes" || value == "y") {
            return true;
        } else if (value == "false" || value == "0" || value == "no" || value == "n") {
            return false;
        }
    }
    return defaultValue;
}

// Check if a parameter exists
bool ComponentConfig::hasParameter(const std::string& key) const {
    return parameters_.find(key) != parameters_.end();
}

// Get all parameters
const std::map<std::string, std::string>& ComponentConfig::getAllParameters() const {
    return parameters_;
}

//-----------------------------------------------------------------------------
// ProcessingComponent Implementation
//-----------------------------------------------------------------------------

// Constructor with component ID and name
ProcessingComponent::ProcessingComponent(const std::string& id, const std::string& name)
    : id_(id)
    , name_(name)
    , enabled_(true) {
    
    // Set initial state
    state_.setCurrentStage("Initialized");
    state_.setStatus(ProcessingStatus::PENDING);
    
    // Default logger just outputs to std::cerr
    logger_ = [](const std::string& level, const std::string& message) {
        std::cerr << "[" << level << "] " << message << std::endl;
    };
}

// Initialize the component
bool ProcessingComponent::initialize(const ComponentConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Copy configuration
    config_ = config;
    
    // Reset state
    state_.reset();
    state_.setCurrentStage("Initialized");
    
    // Log initialization
    log("INFO", "Component initialized: " + id_ + " (" + name_ + ")");
    
    return true;
}

// Reset the component state
void ProcessingComponent::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Reset state
    state_.reset();
    state_.setCurrentStage("Reset");
    
    // Log reset
    log("INFO", "Component reset: " + id_ + " (" + name_ + ")");
}

// Set a logger function
void ProcessingComponent::setLogger(std::function<void(const std::string&, const std::string&)> logger) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (logger) {
        logger_ = logger;
    } else {
        // Reset to default logger
        logger_ = [](const std::string& level, const std::string& message) {
            std::cerr << "[" << level << "] " << message << std::endl;
        };
    }
}

// Log a message
void ProcessingComponent::log(const std::string& level, const std::string& message) const {
    // No need to lock, as logger_ is only accessed here and in setLogger (which has its own lock)
    if (logger_) {
        logger_(level, id_ + ": " + message);
    }
}

// Set whether this component is enabled
void ProcessingComponent::setEnabled(bool enabled) {
    enabled_ = enabled;
    
    log("INFO", std::string(enabled ? "Enabled" : "Disabled") + " component " + id_);
}

// Check if this component is enabled
bool ProcessingComponent::isEnabled() const {
    return enabled_;
}

// Validate input signal
bool ProcessingComponent::validateInput(std::shared_ptr<Signal> signal) const {
    // Base implementation just checks if signal is not null
    if (!signal) {
        log("ERROR", "Null input signal");
        return false;
    }
    
    return true;
}

// Update processing state
void ProcessingComponent::updateState(std::shared_ptr<Signal> signal, const std::string& stage) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Update stage
    state_.setCurrentStage(stage);
    
    // Add checkpoint
    state_.addCheckpoint(stage, "Processing " + stage);
    
    // Add signal info to resource utilization
    if (signal) {
        state_.setResourceUtilization("sampleCount", signal->getSampleCount(), "samples");
        state_.setResourceUtilization("sampleRate", signal->getSampleRate(), "Hz");
        state_.setResourceUtilization("duration", signal->getDuration(), "s");
    }
}

// Add processing history to output signal
void ProcessingComponent::addProcessingHistory(
    std::shared_ptr<Signal> input,
    std::shared_ptr<Signal> output,
    const std::string& operation
) {
    if (!input || !output) {
        return;
    }
    
    // Add metadata to track processing history
    if (output->hasMetadata("metadata")) {
        // Get existing metadata
        std::string metadataStr = output->getMetadata("metadata");
        std::shared_ptr<SignalMetadata> metadata;
        
        // TODO: Deserialize metadata from string
        
        // Add processing history entry
        auto& entry = metadata->addProcessingHistoryEntry(id_, name_, operation);
        
        // Add parameters from config
        for (const auto& param : config_.getAllParameters()) {
            entry.parameters[param.first] = param.second;
        }
        
        // TODO: Serialize metadata back to string
        // output->setMetadata("metadata", serializedStr);
    } else {
        // Create new metadata if none exists
        auto metadata = std::make_shared<SignalMetadata>();
        
        // Add processing history entry
        auto& entry = metadata->addProcessingHistoryEntry(id_, name_, operation);
        
        // Add parameters from config
        for (const auto& param : config_.getAllParameters()) {
            entry.parameters[param.first] = param.second;
        }
        
        // TODO: Serialize metadata to string
        // output->setMetadata("metadata", serializedStr);
    }
    
    // Add component ID to processing chain
    std::string processingChain = input->getMetadata("processing_chain");
    if (!processingChain.empty()) {
        processingChain += "," + id_;
    } else {
        processingChain = id_;
    }
    output->setMetadata("processing_chain", processingChain);
}

} // namespace signal
} // namespace tdoa 