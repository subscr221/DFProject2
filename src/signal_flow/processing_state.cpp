/**
 * @file processing_state.cpp
 * @brief Implementation of ProcessingState class
 */

#include "processing_state.h"
#include <stdexcept>
#include <limits>

namespace tdoa {
namespace signal {

// Convert ProcessingStatus to string
std::string processingStatusToString(ProcessingStatus status) {
    switch (status) {
        case ProcessingStatus::PENDING:
            return "PENDING";
        case ProcessingStatus::IN_PROGRESS:
            return "IN_PROGRESS";
        case ProcessingStatus::COMPLETED:
            return "COMPLETED";
        case ProcessingStatus::FAILED:
            return "FAILED";
        case ProcessingStatus::CANCELLED:
            return "CANCELLED";
        default:
            return "UNKNOWN";
    }
}

// Convert string to ProcessingStatus
ProcessingStatus stringToProcessingStatus(const std::string& statusStr) {
    if (statusStr == "PENDING") {
        return ProcessingStatus::PENDING;
    } else if (statusStr == "IN_PROGRESS") {
        return ProcessingStatus::IN_PROGRESS;
    } else if (statusStr == "COMPLETED") {
        return ProcessingStatus::COMPLETED;
    } else if (statusStr == "FAILED") {
        return ProcessingStatus::FAILED;
    } else if (statusStr == "CANCELLED") {
        return ProcessingStatus::CANCELLED;
    } else {
        throw std::invalid_argument("Invalid processing status string: " + statusStr);
    }
}

// Default constructor
ProcessingState::ProcessingState()
    : currentStage_("")
    , status_(ProcessingStatus::PENDING)
    , errorMessage_("")
    , startTime_(std::chrono::system_clock::now())
    , endTime_(std::chrono::system_clock::time_point::max())
    , lastStatusChangeTime_(startTime_) {
}

// Constructor with initial processing stage and status
ProcessingState::ProcessingState(const std::string& stage, ProcessingStatus status)
    : currentStage_(stage)
    , status_(status)
    , errorMessage_("")
    , startTime_(std::chrono::system_clock::now())
    , endTime_(std::chrono::system_clock::time_point::max())
    , lastStatusChangeTime_(startTime_) {
}

// Set the current processing stage
void ProcessingState::setCurrentStage(const std::string& stage) {
    currentStage_ = stage;
}

// Set the current processing status
void ProcessingState::setStatus(ProcessingStatus status, bool updateTimestamp) {
    // Store the old status to check for transitions
    ProcessingStatus oldStatus = status_;
    
    // Update the status
    status_ = status;
    
    // Update the timestamp if requested
    if (updateTimestamp) {
        lastStatusChangeTime_ = std::chrono::system_clock::now();
    }
    
    // If transitioning to a terminal state, update the end time
    if ((status_ == ProcessingStatus::COMPLETED || 
         status_ == ProcessingStatus::FAILED || 
         status_ == ProcessingStatus::CANCELLED) && 
        (oldStatus == ProcessingStatus::PENDING || 
         oldStatus == ProcessingStatus::IN_PROGRESS)) {
        endTime_ = lastStatusChangeTime_;
    }
    
    // If restarting, reset the end time
    if ((status_ == ProcessingStatus::PENDING || 
         status_ == ProcessingStatus::IN_PROGRESS) && 
        (oldStatus == ProcessingStatus::COMPLETED || 
         oldStatus == ProcessingStatus::FAILED || 
         oldStatus == ProcessingStatus::CANCELLED)) {
        endTime_ = std::chrono::system_clock::time_point::max();
    }
}

// Set the error message
void ProcessingState::setErrorMessage(const std::string& message, bool updateStatus) {
    errorMessage_ = message;
    
    // Update status to FAILED if requested and not already failed
    if (updateStatus && status_ != ProcessingStatus::FAILED) {
        setStatus(ProcessingStatus::FAILED);
    }
}

// Mark the processing as started
void ProcessingState::markStarted(const std::string& stage) {
    // Update the stage if provided
    if (!stage.empty()) {
        currentStage_ = stage;
    }
    
    // Set status to IN_PROGRESS
    setStatus(ProcessingStatus::IN_PROGRESS);
    
    // Reset start and end times
    startTime_ = lastStatusChangeTime_;
    endTime_ = std::chrono::system_clock::time_point::max();
    
    // Add a checkpoint
    addCheckpoint("start", "Processing started");
}

// Mark the processing as completed
void ProcessingState::markCompleted(const std::string& stage) {
    // Update the stage if provided
    if (!stage.empty()) {
        currentStage_ = stage;
    }
    
    // Set status to COMPLETED
    setStatus(ProcessingStatus::COMPLETED);
    
    // Add a checkpoint
    addCheckpoint("complete", "Processing completed successfully");
}

// Mark the processing as failed
void ProcessingState::markFailed(const std::string& errorMessage, const std::string& stage) {
    // Update the stage if provided
    if (!stage.empty()) {
        currentStage_ = stage;
    }
    
    // Set error message and status to FAILED
    setErrorMessage(errorMessage, false);
    setStatus(ProcessingStatus::FAILED);
    
    // Add a checkpoint
    addCheckpoint("failed", "Processing failed: " + errorMessage);
}

// Mark the processing as cancelled
void ProcessingState::markCancelled(const std::string& reason, const std::string& stage) {
    // Update the stage if provided
    if (!stage.empty()) {
        currentStage_ = stage;
    }
    
    // Set status to CANCELLED
    setStatus(ProcessingStatus::CANCELLED);
    
    // Set error message with reason if provided
    if (!reason.empty()) {
        errorMessage_ = "Cancelled: " + reason;
    } else {
        errorMessage_ = "Cancelled";
    }
    
    // Add a checkpoint
    addCheckpoint("cancelled", errorMessage_);
}

// Set a resource utilization metric
void ProcessingState::setResourceUtilization(const std::string& resource, double value, const std::string& unit) {
    resourceUtilization_[resource] = value;
    
    // Update unit if provided
    if (!unit.empty()) {
        resourceUtilizationUnits_[resource] = unit;
    }
}

// Get a resource utilization metric
double ProcessingState::getResourceUtilization(const std::string& resource, double defaultValue) const {
    auto it = resourceUtilization_.find(resource);
    if (it != resourceUtilization_.end()) {
        return it->second;
    }
    return defaultValue;
}

// Check if a resource utilization metric exists
bool ProcessingState::hasResourceUtilization(const std::string& resource) const {
    return resourceUtilization_.find(resource) != resourceUtilization_.end();
}

// Get the unit for a resource utilization metric
std::string ProcessingState::getResourceUtilizationUnit(const std::string& resource) const {
    auto it = resourceUtilizationUnits_.find(resource);
    if (it != resourceUtilizationUnits_.end()) {
        return it->second;
    }
    return "";
}

// Add a processing checkpoint with timestamp
std::chrono::system_clock::time_point ProcessingState::addCheckpoint(const std::string& name, const std::string& description) {
    auto now = std::chrono::system_clock::now();
    checkpoints_[name] = now;
    
    // Store description if provided
    if (!description.empty()) {
        checkpointDescriptions_[name] = description;
    }
    
    return now;
}

// Reset the state to initial values
void ProcessingState::reset() {
    currentStage_ = "";
    status_ = ProcessingStatus::PENDING;
    errorMessage_ = "";
    
    auto now = std::chrono::system_clock::now();
    startTime_ = now;
    endTime_ = std::chrono::system_clock::time_point::max();
    lastStatusChangeTime_ = now;
    
    resourceUtilization_.clear();
    resourceUtilizationUnits_.clear();
    checkpoints_.clear();
    checkpointDescriptions_.clear();
}

// Clone this processing state
std::shared_ptr<ProcessingState> ProcessingState::clone() const {
    auto result = std::make_shared<ProcessingState>();
    
    // Copy basic properties
    result->currentStage_ = currentStage_;
    result->status_ = status_;
    result->errorMessage_ = errorMessage_;
    result->startTime_ = startTime_;
    result->endTime_ = endTime_;
    result->lastStatusChangeTime_ = lastStatusChangeTime_;
    
    // Copy resource utilization
    result->resourceUtilization_ = resourceUtilization_;
    result->resourceUtilizationUnits_ = resourceUtilizationUnits_;
    
    // Copy checkpoints
    result->checkpoints_ = checkpoints_;
    result->checkpointDescriptions_ = checkpointDescriptions_;
    
    return result;
}

// Calculate the processing duration
int64_t ProcessingState::getDurationMs() const {
    // If processing hasn't ended, calculate from now
    auto end = (endTime_ == std::chrono::system_clock::time_point::max()) ? 
               std::chrono::system_clock::now() : endTime_;
    
    // Calculate duration
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - startTime_).count();
}

// Calculate the time elapsed since processing started
int64_t ProcessingState::getElapsedMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - startTime_).count();
}

// Calculate the time elapsed since the last status change
int64_t ProcessingState::getTimeSinceLastStatusChangeMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - lastStatusChangeTime_).count();
}

} // namespace signal
} // namespace tdoa 