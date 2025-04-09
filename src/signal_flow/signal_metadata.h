/**
 * @file signal_metadata.h
 * @brief Class for storing metadata associated with signals
 */

#pragma once

#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <memory>

namespace tdoa {
namespace signal {

/**
 * @brief Class for storing metadata associated with signals
 * 
 * This class provides a structured way to store and access metadata
 * about signal processing, including source information, quality metrics,
 * processing history, and classification tags.
 */
class SignalMetadata {
public:
    /**
     * @brief Default constructor
     */
    SignalMetadata();
    
    /**
     * @brief Destructor
     */
    virtual ~SignalMetadata();
    
    /**
     * @brief Get the signal source information
     * @return Map of source information key-value pairs
     */
    const std::map<std::string, std::string>& getSourceInfo() const { return sourceInfo_; }
    
    /**
     * @brief Set a source information value
     * @param key Source info key
     * @param value Source info value
     */
    void setSourceInfo(const std::string& key, const std::string& value);
    
    /**
     * @brief Get a source information value
     * @param key Source info key
     * @return Source info value or empty string if not found
     */
    std::string getSourceInfo(const std::string& key) const;
    
    /**
     * @brief Check if source information key exists
     * @param key Source info key
     * @return True if key exists
     */
    bool hasSourceInfo(const std::string& key) const;
    
    /**
     * @brief Get the quality metrics
     * @return Map of quality metric key-value pairs
     */
    const std::map<std::string, double>& getQualityMetrics() const { return qualityMetrics_; }
    
    /**
     * @brief Set a quality metric value
     * @param key Quality metric key
     * @param value Quality metric value
     */
    void setQualityMetric(const std::string& key, double value);
    
    /**
     * @brief Get a quality metric value
     * @param key Quality metric key
     * @param defaultValue Default value to return if key not found
     * @return Quality metric value or defaultValue if not found
     */
    double getQualityMetric(const std::string& key, double defaultValue = 0.0) const;
    
    /**
     * @brief Check if quality metric key exists
     * @param key Quality metric key
     * @return True if key exists
     */
    bool hasQualityMetric(const std::string& key) const;
    
    /**
     * @brief Processing history entry structure
     */
    struct ProcessingHistoryEntry {
        std::string componentId;       ///< ID of the processing component
        std::string componentName;     ///< Name of the processing component
        std::string operation;         ///< Description of the operation performed
        std::chrono::system_clock::time_point timestamp; ///< Timestamp of the operation
        std::map<std::string, std::string> parameters;   ///< Parameters used for the operation
        
        ProcessingHistoryEntry();
        ProcessingHistoryEntry(
            const std::string& componentId,
            const std::string& componentName,
            const std::string& operation
        );
    };
    
    /**
     * @brief Add a processing history entry
     * @param entry Processing history entry
     */
    void addProcessingHistoryEntry(const ProcessingHistoryEntry& entry);
    
    /**
     * @brief Add a processing history entry with core parameters
     * @param componentId ID of the processing component
     * @param componentName Name of the processing component
     * @param operation Description of the operation performed
     * @return Reference to the newly added entry for parameter setting
     */
    ProcessingHistoryEntry& addProcessingHistoryEntry(
        const std::string& componentId,
        const std::string& componentName,
        const std::string& operation
    );
    
    /**
     * @brief Get the processing history
     * @return Vector of processing history entries
     */
    const std::vector<ProcessingHistoryEntry>& getProcessingHistory() const { return processingHistory_; }
    
    /**
     * @brief Add a classification tag
     * @param tag Classification tag string
     * @param confidence Confidence value (0.0 to 1.0)
     */
    void addClassificationTag(const std::string& tag, double confidence = 1.0);
    
    /**
     * @brief Remove a classification tag
     * @param tag Classification tag string
     * @return True if tag was removed, false if not found
     */
    bool removeClassificationTag(const std::string& tag);
    
    /**
     * @brief Get all classification tags
     * @return Map of tag names to confidence values
     */
    const std::map<std::string, double>& getClassificationTags() const { return classificationTags_; }
    
    /**
     * @brief Check if a classification tag exists
     * @param tag Classification tag string
     * @return True if tag exists
     */
    bool hasClassificationTag(const std::string& tag) const;
    
    /**
     * @brief Get the confidence value for a tag
     * @param tag Classification tag string
     * @param defaultValue Default value to return if tag not found
     * @return Confidence value or defaultValue if tag not found
     */
    double getTagConfidence(const std::string& tag, double defaultValue = 0.0) const;
    
    /**
     * @brief Merge metadata from another instance
     * @param other Other metadata instance to merge from
     * @param overwrite Whether to overwrite existing values
     */
    void merge(const SignalMetadata& other, bool overwrite = true);
    
    /**
     * @brief Clone this metadata object
     * @return Shared pointer to a new metadata object
     */
    std::shared_ptr<SignalMetadata> clone() const;
    
private:
    std::map<std::string, std::string> sourceInfo_;        ///< Source information
    std::map<std::string, double> qualityMetrics_;         ///< Quality metrics
    std::vector<ProcessingHistoryEntry> processingHistory_; ///< Processing history
    std::map<std::string, double> classificationTags_;     ///< Classification tags
};

} // namespace signal
} // namespace tdoa 