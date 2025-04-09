/**
 * @file processing_state.h
 * @brief Tracking state of signal processing operations
 */

#pragma once

#include <string>
#include <map>
#include <memory>
#include <chrono>
#include <vector>

namespace tdoa {
namespace signal {

/**
 * @brief Enum representing the status of processing
 */
enum class ProcessingStatus {
    PENDING,        ///< Processing has not started
    IN_PROGRESS,    ///< Processing is currently running
    COMPLETED,      ///< Processing completed successfully
    FAILED,         ///< Processing failed
    CANCELLED       ///< Processing was cancelled
};

/**
 * @brief Convert ProcessingStatus to string
 * @param status Status to convert
 * @return String representation
 */
std::string processingStatusToString(ProcessingStatus status);

/**
 * @brief Convert string to ProcessingStatus
 * @param statusStr String to convert
 * @return ProcessingStatus value
 * @throws std::invalid_argument if string cannot be converted
 */
ProcessingStatus stringToProcessingStatus(const std::string& statusStr);

/**
 * @brief Class to track the state of signal processing
 */
class ProcessingState {
public:
    /**
     * @brief Default constructor, initializes state to PENDING
     */
    ProcessingState();
    
    /**
     * @brief Constructor with initial processing stage and status
     * @param stage Current processing stage name
     * @param status Current processing status
     */
    ProcessingState(const std::string& stage, ProcessingStatus status = ProcessingStatus::PENDING);
    
    /**
     * @brief Get the current processing stage
     * @return Current stage name
     */
    const std::string& getCurrentStage() const { return currentStage_; }
    
    /**
     * @brief Set the current processing stage
     * @param stage Current stage name
     */
    void setCurrentStage(const std::string& stage);
    
    /**
     * @brief Get the current processing status
     * @return Current status
     */
    ProcessingStatus getStatus() const { return status_; }
    
    /**
     * @brief Set the current processing status
     * @param status Current status
     * @param updateTimestamp Whether to update the last status change timestamp
     */
    void setStatus(ProcessingStatus status, bool updateTimestamp = true);
    
    /**
     * @brief Check if processing is completed successfully
     * @return True if status is COMPLETED
     */
    bool isCompleted() const { return status_ == ProcessingStatus::COMPLETED; }
    
    /**
     * @brief Check if processing failed
     * @return True if status is FAILED
     */
    bool isFailed() const { return status_ == ProcessingStatus::FAILED; }
    
    /**
     * @brief Get the error message if any
     * @return Error message or empty string if no error
     */
    const std::string& getErrorMessage() const { return errorMessage_; }
    
    /**
     * @brief Set the error message
     * @param message Error message
     * @param updateStatus Whether to update status to FAILED
     */
    void setErrorMessage(const std::string& message, bool updateStatus = true);
    
    /**
     * @brief Get the start time
     * @return Processing start time
     */
    const std::chrono::system_clock::time_point& getStartTime() const { return startTime_; }
    
    /**
     * @brief Get the end time
     * @return Processing end time (or max time_point if not finished)
     */
    const std::chrono::system_clock::time_point& getEndTime() const { return endTime_; }
    
    /**
     * @brief Get the last status change time
     * @return Last status change time
     */
    const std::chrono::system_clock::time_point& getLastStatusChangeTime() const { return lastStatusChangeTime_; }
    
    /**
     * @brief Mark the processing as started
     * @param stage Optional stage name to set
     */
    void markStarted(const std::string& stage = "");
    
    /**
     * @brief Mark the processing as completed
     * @param stage Optional stage name to set
     */
    void markCompleted(const std::string& stage = "");
    
    /**
     * @brief Mark the processing as failed
     * @param errorMessage Error message describing the failure
     * @param stage Optional stage name to set
     */
    void markFailed(const std::string& errorMessage, const std::string& stage = "");
    
    /**
     * @brief Mark the processing as cancelled
     * @param reason Reason for cancellation
     * @param stage Optional stage name to set
     */
    void markCancelled(const std::string& reason = "", const std::string& stage = "");
    
    /**
     * @brief Set a resource utilization metric
     * @param resource Resource name
     * @param value Utilization value
     * @param unit Optional unit of measurement
     */
    void setResourceUtilization(const std::string& resource, double value, const std::string& unit = "");
    
    /**
     * @brief Get a resource utilization metric
     * @param resource Resource name
     * @param defaultValue Default value to return if resource not found
     * @return Resource utilization value or defaultValue if not found
     */
    double getResourceUtilization(const std::string& resource, double defaultValue = 0.0) const;
    
    /**
     * @brief Check if a resource utilization metric exists
     * @param resource Resource name
     * @return True if resource metric exists
     */
    bool hasResourceUtilization(const std::string& resource) const;
    
    /**
     * @brief Get the unit for a resource utilization metric
     * @param resource Resource name
     * @return Unit string or empty string if not found
     */
    std::string getResourceUtilizationUnit(const std::string& resource) const;
    
    /**
     * @brief Get all resource utilization metrics
     * @return Map of resource names to utilization values
     */
    const std::map<std::string, double>& getResourceUtilizations() const { return resourceUtilization_; }
    
    /**
     * @brief Get all resource utilization units
     * @return Map of resource names to unit strings
     */
    const std::map<std::string, std::string>& getResourceUtilizationUnits() const { return resourceUtilizationUnits_; }
    
    /**
     * @brief Add a processing checkpoint with timestamp
     * @param name Checkpoint name
     * @param description Optional description
     * @return Timestamp when checkpoint was added
     */
    std::chrono::system_clock::time_point addCheckpoint(const std::string& name, const std::string& description = "");
    
    /**
     * @brief Get all processing checkpoints
     * @return Map of checkpoint names to timestamps
     */
    const std::map<std::string, std::chrono::system_clock::time_point>& getCheckpoints() const { return checkpoints_; }
    
    /**
     * @brief Get checkpoint descriptions
     * @return Map of checkpoint names to descriptions
     */
    const std::map<std::string, std::string>& getCheckpointDescriptions() const { return checkpointDescriptions_; }
    
    /**
     * @brief Reset the state to initial values
     */
    void reset();
    
    /**
     * @brief Clone this processing state
     * @return Shared pointer to a new state object
     */
    std::shared_ptr<ProcessingState> clone() const;
    
    /**
     * @brief Calculate the processing duration
     * @return Duration in milliseconds
     */
    int64_t getDurationMs() const;
    
    /**
     * @brief Calculate the time elapsed since processing started
     * @return Duration in milliseconds
     */
    int64_t getElapsedMs() const;
    
    /**
     * @brief Calculate the time elapsed since the last status change
     * @return Duration in milliseconds
     */
    int64_t getTimeSinceLastStatusChangeMs() const;
    
private:
    std::string currentStage_;                                  ///< Current processing stage
    ProcessingStatus status_;                                   ///< Current processing status
    std::string errorMessage_;                                  ///< Error message if failed
    std::chrono::system_clock::time_point startTime_;           ///< Processing start time
    std::chrono::system_clock::time_point endTime_;             ///< Processing end time
    std::chrono::system_clock::time_point lastStatusChangeTime_; ///< Last status change time
    
    std::map<std::string, double> resourceUtilization_;         ///< Resource utilization metrics
    std::map<std::string, std::string> resourceUtilizationUnits_; ///< Units for resource metrics
    std::map<std::string, std::chrono::system_clock::time_point> checkpoints_; ///< Processing checkpoints
    std::map<std::string, std::string> checkpointDescriptions_; ///< Descriptions for checkpoints
};

} // namespace signal
} // namespace tdoa 