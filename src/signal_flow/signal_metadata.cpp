/**
 * @file signal_metadata.cpp
 * @brief Implementation of the SignalMetadata class
 */

#include "signal_metadata.h"
#include <stdexcept>

namespace tdoa {
namespace signal {

// Default constructor
SignalMetadata::SignalMetadata() {
    // Initialization handled by member initializers
}

// Destructor
SignalMetadata::~SignalMetadata() {
    // Nothing to explicitly clean up
}

// Set a source information value
void SignalMetadata::setSourceInfo(const std::string& key, const std::string& value) {
    sourceInfo_[key] = value;
}

// Get a source information value
std::string SignalMetadata::getSourceInfo(const std::string& key) const {
    auto it = sourceInfo_.find(key);
    if (it != sourceInfo_.end()) {
        return it->second;
    }
    return "";
}

// Check if source information key exists
bool SignalMetadata::hasSourceInfo(const std::string& key) const {
    return sourceInfo_.find(key) != sourceInfo_.end();
}

// Set a quality metric value
void SignalMetadata::setQualityMetric(const std::string& key, double value) {
    qualityMetrics_[key] = value;
}

// Get a quality metric value
double SignalMetadata::getQualityMetric(const std::string& key, double defaultValue) const {
    auto it = qualityMetrics_.find(key);
    if (it != qualityMetrics_.end()) {
        return it->second;
    }
    return defaultValue;
}

// Check if quality metric key exists
bool SignalMetadata::hasQualityMetric(const std::string& key) const {
    return qualityMetrics_.find(key) != qualityMetrics_.end();
}

// ProcessingHistoryEntry default constructor
SignalMetadata::ProcessingHistoryEntry::ProcessingHistoryEntry()
    : timestamp(std::chrono::system_clock::now()) {
}

// ProcessingHistoryEntry parameterized constructor
SignalMetadata::ProcessingHistoryEntry::ProcessingHistoryEntry(
    const std::string& componentId,
    const std::string& componentName,
    const std::string& operation
)
    : componentId(componentId)
    , componentName(componentName)
    , operation(operation)
    , timestamp(std::chrono::system_clock::now()) {
}

// Add a processing history entry
void SignalMetadata::addProcessingHistoryEntry(const ProcessingHistoryEntry& entry) {
    processingHistory_.push_back(entry);
}

// Add a processing history entry with core parameters
SignalMetadata::ProcessingHistoryEntry& SignalMetadata::addProcessingHistoryEntry(
    const std::string& componentId,
    const std::string& componentName,
    const std::string& operation
) {
    processingHistory_.emplace_back(componentId, componentName, operation);
    return processingHistory_.back();
}

// Add a classification tag
void SignalMetadata::addClassificationTag(const std::string& tag, double confidence) {
    // Ensure confidence is in valid range
    if (confidence < 0.0 || confidence > 1.0) {
        throw std::out_of_range("Classification tag confidence must be between 0.0 and 1.0");
    }
    classificationTags_[tag] = confidence;
}

// Remove a classification tag
bool SignalMetadata::removeClassificationTag(const std::string& tag) {
    auto it = classificationTags_.find(tag);
    if (it != classificationTags_.end()) {
        classificationTags_.erase(it);
        return true;
    }
    return false;
}

// Check if a classification tag exists
bool SignalMetadata::hasClassificationTag(const std::string& tag) const {
    return classificationTags_.find(tag) != classificationTags_.end();
}

// Get the confidence value for a tag
double SignalMetadata::getTagConfidence(const std::string& tag, double defaultValue) const {
    auto it = classificationTags_.find(tag);
    if (it != classificationTags_.end()) {
        return it->second;
    }
    return defaultValue;
}

// Merge metadata from another instance
void SignalMetadata::merge(const SignalMetadata& other, bool overwrite) {
    // Merge source information
    for (const auto& pair : other.sourceInfo_) {
        if (overwrite || !hasSourceInfo(pair.first)) {
            sourceInfo_[pair.first] = pair.second;
        }
    }
    
    // Merge quality metrics
    for (const auto& pair : other.qualityMetrics_) {
        if (overwrite || !hasQualityMetric(pair.first)) {
            qualityMetrics_[pair.first] = pair.second;
        }
    }
    
    // Append processing history (always append all entries)
    for (const auto& entry : other.processingHistory_) {
        processingHistory_.push_back(entry);
    }
    
    // Merge classification tags
    for (const auto& pair : other.classificationTags_) {
        if (overwrite || !hasClassificationTag(pair.first)) {
            classificationTags_[pair.first] = pair.second;
        }
    }
}

// Clone this metadata object
std::shared_ptr<SignalMetadata> SignalMetadata::clone() const {
    auto result = std::make_shared<SignalMetadata>();
    
    // Copy all data
    result->sourceInfo_ = sourceInfo_;
    result->qualityMetrics_ = qualityMetrics_;
    result->processingHistory_ = processingHistory_;
    result->classificationTags_ = classificationTags_;
    
    return result;
}

} // namespace signal
} // namespace tdoa 