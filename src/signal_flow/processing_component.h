/**
 * @file processing_component.h
 * @brief Abstract base class for signal processing components
 */

#pragma once

#include "signal.h"
#include "signal_metadata.h"
#include "processing_state.h"
#include <memory>
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>

namespace tdoa {
namespace signal {

/**
 * @brief Configuration for a processing component
 */
class ComponentConfig {
public:
    /**
     * @brief Default constructor
     */
    ComponentConfig() = default;
    
    /**
     * @brief Destructor
     */
    virtual ~ComponentConfig() = default;
    
    /**
     * @brief Set a string parameter
     * @param key Parameter key
     * @param value Parameter value
     */
    void setParameter(const std::string& key, const std::string& value);
    
    /**
     * @brief Set an integer parameter
     * @param key Parameter key
     * @param value Parameter value
     */
    void setParameter(const std::string& key, int value);
    
    /**
     * @brief Set a double parameter
     * @param key Parameter key
     * @param value Parameter value
     */
    void setParameter(const std::string& key, double value);
    
    /**
     * @brief Set a boolean parameter
     * @param key Parameter key
     * @param value Parameter value
     */
    void setParameter(const std::string& key, bool value);
    
    /**
     * @brief Get a string parameter
     * @param key Parameter key
     * @param defaultValue Default value if parameter not found
     * @return Parameter value or defaultValue if not found
     */
    std::string getStringParameter(const std::string& key, const std::string& defaultValue = "") const;
    
    /**
     * @brief Get an integer parameter
     * @param key Parameter key
     * @param defaultValue Default value if parameter not found
     * @return Parameter value or defaultValue if not found
     */
    int getIntParameter(const std::string& key, int defaultValue = 0) const;
    
    /**
     * @brief Get a double parameter
     * @param key Parameter key
     * @param defaultValue Default value if parameter not found
     * @return Parameter value or defaultValue if not found
     */
    double getDoubleParameter(const std::string& key, double defaultValue = 0.0) const;
    
    /**
     * @brief Get a boolean parameter
     * @param key Parameter key
     * @param defaultValue Default value if parameter not found
     * @return Parameter value or defaultValue if not found
     */
    bool getBoolParameter(const std::string& key, bool defaultValue = false) const;
    
    /**
     * @brief Check if a parameter exists
     * @param key Parameter key
     * @return True if parameter exists
     */
    bool hasParameter(const std::string& key) const;
    
    /**
     * @brief Get all parameters
     * @return Map of parameter keys to values
     */
    const std::map<std::string, std::string>& getAllParameters() const;
    
private:
    std::map<std::string, std::string> parameters_;   ///< Parameter storage
};

/**
 * @brief Abstract base class for signal processing components
 */
class ProcessingComponent {
public:
    /**
     * @brief Constructor with component ID and name
     * @param id Component ID
     * @param name Component name
     */
    ProcessingComponent(const std::string& id, const std::string& name);
    
    /**
     * @brief Virtual destructor
     */
    virtual ~ProcessingComponent() = default;
    
    /**
     * @brief Initialize the component
     * @param config Component configuration
     * @return True if initialization successful
     */
    virtual bool initialize(const ComponentConfig& config);
    
    /**
     * @brief Process a signal
     * @param signal Input signal
     * @return Output signal or nullptr if processing failed
     */
    virtual std::shared_ptr<Signal> process(std::shared_ptr<Signal> signal) = 0;
    
    /**
     * @brief Reset the component state
     */
    virtual void reset();
    
    /**
     * @brief Get the component ID
     * @return Component ID
     */
    const std::string& getId() const { return id_; }
    
    /**
     * @brief Get the component name
     * @return Component name
     */
    const std::string& getName() const { return name_; }
    
    /**
     * @brief Get the component configuration
     * @return Component configuration
     */
    const ComponentConfig& getConfig() const { return config_; }
    
    /**
     * @brief Get the component configuration (non-const)
     * @return Component configuration
     */
    ComponentConfig& getConfig() { return config_; }
    
    /**
     * @brief Get the processing state
     * @return Processing state
     */
    const ProcessingState& getState() const { return state_; }
    
    /**
     * @brief Get the processing state (non-const)
     * @return Processing state
     */
    ProcessingState& getState() { return state_; }
    
    /**
     * @brief Set a logger function
     * @param logger Logger function that takes a log level and message
     */
    void setLogger(std::function<void(const std::string&, const std::string&)> logger);
    
    /**
     * @brief Log a message
     * @param level Log level (e.g., "INFO", "WARNING", "ERROR")
     * @param message Log message
     */
    void log(const std::string& level, const std::string& message) const;
    
    /**
     * @brief Set whether this component is enabled
     * @param enabled Enabled flag
     */
    void setEnabled(bool enabled);
    
    /**
     * @brief Check if this component is enabled
     * @return True if enabled
     */
    bool isEnabled() const;
    
    /**
     * @brief Clone this component
     * @return Cloned component
     */
    virtual std::shared_ptr<ProcessingComponent> clone() const = 0;
    
protected:
    /**
     * @brief Validate input signal
     * @param signal Input signal
     * @return True if signal is valid for this component
     */
    virtual bool validateInput(std::shared_ptr<Signal> signal) const;
    
    /**
     * @brief Update processing state
     * @param signal Input signal
     * @param stage Current processing stage
     */
    void updateState(std::shared_ptr<Signal> signal, const std::string& stage);
    
    /**
     * @brief Add processing history to output signal
     * @param input Input signal
     * @param output Output signal
     * @param operation Operation description
     */
    void addProcessingHistory(std::shared_ptr<Signal> input, 
                             std::shared_ptr<Signal> output,
                             const std::string& operation);
    
    std::string id_;               ///< Component ID
    std::string name_;             ///< Component name
    ComponentConfig config_;       ///< Component configuration
    ProcessingState state_;        ///< Processing state
    std::atomic<bool> enabled_;    ///< Enabled flag
    
private:
    std::function<void(const std::string&, const std::string&)> logger_; ///< Logger function
    mutable std::mutex mutex_;     ///< Mutex for thread safety
};

/**
 * @brief Callback function type for signal processing
 */
using ProcessingCallback = std::function<void(std::shared_ptr<Signal>, const ProcessingState&)>;

} // namespace signal
} // namespace tdoa 