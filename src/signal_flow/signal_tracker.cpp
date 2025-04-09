#include "signal_tracker.h"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <random>

namespace tdoa {
namespace signal {

SignalTracker::SignalTracker(const TrackingConfig& config)
    : config_(config) {
}

bool SignalTracker::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        // Initialize statistics
        stats_["total_tracks"] = 0;
        stats_["active_tracks"] = 0;
        stats_["merged_tracks"] = 0;
        stats_["track_updates"] = 0;
        stats_["track_predictions"] = 0;
        stats_["processing_time"] = 0;

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error initializing SignalTracker: " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::string> SignalTracker::updateTracks(
    const std::vector<DetectedSignal>& signals) {
    
    if (signals.empty()) {
        return {};
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto startTime = std::chrono::high_resolution_clock::now();
    std::vector<std::string> updatedTracks;

    try {
        // Match signals to existing tracks
        auto matches = matchSignalsToTracks(signals);

        // Update matched tracks and create new ones
        for (size_t i = 0; i < signals.size(); ++i) {
            const auto& signal = signals[i];
            auto matchIt = matches.find(i);

            if (matchIt != matches.end()) {
                // Update existing track
                updateTrack(matchIt->second, signal);
                updatedTracks.push_back(matchIt->second);
            }
            else {
                // Create new track
                std::string trackId = createTrack(signal);
                if (!trackId.empty()) {
                    updatedTracks.push_back(trackId);
                }
            }
        }

        // Predict next points for active tracks not updated
        if (config_.enablePrediction) {
            for (auto& [trackId, track] : tracks_) {
                if (track->active && 
                    std::find(updatedTracks.begin(), updatedTracks.end(), trackId) == updatedTracks.end()) {
                    auto prediction = predictNextPoint(*track);
                    track->points.push_back(prediction);
                    stats_["track_predictions"]++;
                }
            }
        }

        // Check for track merging
        if (config_.enableMerging) {
            for (size_t i = 0; i < updatedTracks.size(); ++i) {
                for (size_t j = i + 1; j < updatedTracks.size(); ++j) {
                    const auto& track1 = tracks_[updatedTracks[i]];
                    const auto& track2 = tracks_[updatedTracks[j]];
                    
                    if (shouldMergeTracks(*track1, *track2)) {
                        auto mergedId = mergeTracks(updatedTracks[i], updatedTracks[j]);
                        // Replace the two tracks with the merged one in the updated list
                        updatedTracks[i] = mergedId;
                        updatedTracks.erase(updatedTracks.begin() + j);
                        j--; // Adjust index after removal
                    }
                }
            }
        }

        // Clean up old tracks and points
        cleanupTracks();

        // Update statistics
        auto endTime = std::chrono::high_resolution_clock::now();
        double processingTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        
        stats_["processing_time"] = processingTime;
        stats_["active_tracks"] = std::count_if(tracks_.begin(), tracks_.end(),
            [](const auto& pair) { return pair.second->active; });

        return updatedTracks;
    }
    catch (const std::exception& e) {
        std::cerr << "Error in updateTracks: " << e.what() << std::endl;
        return {};
    }
}

std::map<size_t, std::string> SignalTracker::matchSignalsToTracks(
    const std::vector<DetectedSignal>& signals) {
    
    std::map<size_t, std::string> matches;

    // For each signal, find the best matching track
    for (size_t i = 0; i < signals.size(); ++i) {
        const auto& signal = signals[i];
        std::string bestMatchId;
        double bestMatchScore = 0.0;

        for (const auto& [trackId, track] : tracks_) {
            if (!track->active || track->points.empty()) {
                continue;
            }

            const auto& lastPoint = track->points.back();

            // Calculate matching score based on frequency, bandwidth, and power
            double freqDiff = std::abs(signal.centerFrequency - lastPoint.frequency);
            double bwDiff = std::abs(signal.bandwidth - lastPoint.bandwidth) / lastPoint.bandwidth;
            double powerDiff = std::abs(signal.power - lastPoint.power);

            if (freqDiff <= config_.frequencyTolerance &&
                bwDiff <= config_.bandwidthTolerance &&
                powerDiff <= config_.powerTolerance) {
                
                // Calculate similarity score (1.0 = perfect match)
                double freqScore = 1.0 - (freqDiff / config_.frequencyTolerance);
                double bwScore = 1.0 - (bwDiff / config_.bandwidthTolerance);
                double powerScore = 1.0 - (powerDiff / config_.powerTolerance);
                
                double score = (freqScore + bwScore + powerScore) / 3.0;

                if (score > bestMatchScore) {
                    bestMatchScore = score;
                    bestMatchId = trackId;
                }
            }
        }

        if (!bestMatchId.empty()) {
            matches[i] = bestMatchId;
        }
    }

    return matches;
}

std::string SignalTracker::createTrack(const DetectedSignal& signal) {
    // Check if we're at the track limit
    if (tracks_.size() >= config_.maxTracks) {
        // Try to find an inactive track to replace
        auto oldestInactive = std::min_element(tracks_.begin(), tracks_.end(),
            [](const auto& a, const auto& b) {
                return !a.second->active && !b.second->active &&
                       a.second->lastUpdate < b.second->lastUpdate;
            });

        if (oldestInactive != tracks_.end() && !oldestInactive->second->active) {
            // Notify track end
            if (endCallback_) {
                endCallback_(*oldestInactive->second);
            }
            tracks_.erase(oldestInactive);
        }
        else {
            // No room for new track
            return "";
        }
    }

    // Create new track
    auto track = std::make_shared<Track>();
    track->id = generateTrackId();
    track->active = true;
    track->lastUpdate = std::chrono::system_clock::now();
    track->primaryClass = SignalClass::UNKNOWN;

    // Create initial track point
    TrackPoint point;
    point.timestamp = track->lastUpdate;
    point.frequency = signal.centerFrequency;
    point.bandwidth = signal.bandwidth;
    point.power = signal.power;
    point.snr = signal.snr;
    point.confidence = signal.confidence;
    point.signalClass = SignalClass::UNKNOWN;
    
    track->points.push_back(point);
    tracks_[track->id] = track;

    stats_["total_tracks"]++;
    stats_["track_updates"]++;

    // Notify track update
    if (updateCallback_) {
        updateCallback_(*track);
    }

    return track->id;
}

void SignalTracker::updateTrack(const std::string& trackId, const DetectedSignal& signal) {
    auto it = tracks_.find(trackId);
    if (it == tracks_.end()) {
        return;
    }

    auto& track = it->second;
    track->active = true;
    track->lastUpdate = std::chrono::system_clock::now();

    // Create new track point
    TrackPoint point;
    point.timestamp = track->lastUpdate;
    point.frequency = signal.centerFrequency;
    point.bandwidth = signal.bandwidth;
    point.power = signal.power;
    point.snr = signal.snr;
    point.confidence = signal.confidence;
    point.signalClass = track->primaryClass;  // Maintain current classification

    // Add point to track
    track->points.push_back(point);

    // Trim old points if beyond time window
    auto cutoffTime = track->lastUpdate - 
        std::chrono::duration<double>(config_.timeWindow);
    
    while (!track->points.empty() && 
           track->points.front().timestamp < cutoffTime) {
        track->points.pop_front();
    }

    stats_["track_updates"]++;

    // Notify track update
    if (updateCallback_) {
        updateCallback_(*track);
    }
}

TrackPoint SignalTracker::predictNextPoint(const Track& track) {
    TrackPoint prediction;
    prediction.timestamp = std::chrono::system_clock::now();

    if (track.points.size() < 2) {
        // Not enough points for prediction, copy last known point
        if (!track.points.empty()) {
            const auto& last = track.points.back();
            prediction.frequency = last.frequency;
            prediction.bandwidth = last.bandwidth;
            prediction.power = last.power;
            prediction.snr = last.snr;
            prediction.confidence = last.confidence * 0.8;  // Reduce confidence
            prediction.signalClass = last.signalClass;
        }
        return prediction;
    }

    // Use linear prediction based on last two points
    const auto& last = track.points.back();
    const auto& prev = track.points[track.points.size() - 2];

    // Calculate time difference in seconds
    double dt = std::chrono::duration<double>(last.timestamp - prev.timestamp).count();
    if (dt > 0) {
        // Calculate rates of change
        double freqRate = (last.frequency - prev.frequency) / dt;
        double bwRate = (last.bandwidth - prev.bandwidth) / dt;
        double powerRate = (last.power - prev.power) / dt;
        double snrRate = (last.snr - prev.snr) / dt;

        // Calculate prediction time difference
        double predictionDt = std::chrono::duration<double>(
            prediction.timestamp - last.timestamp).count();

        // Linear extrapolation
        prediction.frequency = last.frequency + freqRate * predictionDt;
        prediction.bandwidth = last.bandwidth + bwRate * predictionDt;
        prediction.power = last.power + powerRate * predictionDt;
        prediction.snr = last.snr + snrRate * predictionDt;
        prediction.confidence = last.confidence * 0.8;  // Reduce confidence
        prediction.signalClass = last.signalClass;
    }
    else {
        // Zero time difference, copy last point
        prediction = last;
        prediction.confidence *= 0.8;  // Reduce confidence
    }

    return prediction;
}

bool SignalTracker::shouldMergeTracks(const Track& track1, const Track& track2) {
    if (track1.points.empty() || track2.points.empty()) {
        return false;
    }

    const auto& point1 = track1.points.back();
    const auto& point2 = track2.points.back();

    // Calculate similarity metrics
    double freqDiff = std::abs(point1.frequency - point2.frequency);
    double bwDiff = std::abs(point1.bandwidth - point2.bandwidth) / 
                    std::max(point1.bandwidth, point2.bandwidth);
    double powerDiff = std::abs(point1.power - point2.power);

    // Calculate similarity score
    double freqScore = 1.0 - (freqDiff / config_.frequencyTolerance);
    double bwScore = 1.0 - (bwDiff / config_.bandwidthTolerance);
    double powerScore = 1.0 - (powerDiff / config_.powerTolerance);
    
    double similarityScore = (freqScore + bwScore + powerScore) / 3.0;

    return similarityScore >= config_.mergingThreshold;
}

std::string SignalTracker::mergeTracks(
    const std::string& track1Id, const std::string& track2Id) {
    
    auto it1 = tracks_.find(track1Id);
    auto it2 = tracks_.find(track2Id);
    
    if (it1 == tracks_.end() || it2 == tracks_.end()) {
        return "";
    }

    const auto& track1 = it1->second;
    const auto& track2 = it2->second;

    // Create new merged track
    auto mergedTrack = std::make_shared<Track>();
    mergedTrack->id = generateTrackId();
    mergedTrack->active = true;
    mergedTrack->lastUpdate = std::chrono::system_clock::now();

    // Combine points from both tracks
    std::vector<TrackPoint> allPoints;
    allPoints.insert(allPoints.end(), track1->points.begin(), track1->points.end());
    allPoints.insert(allPoints.end(), track2->points.begin(), track2->points.end());

    // Sort points by timestamp
    std::sort(allPoints.begin(), allPoints.end(),
        [](const TrackPoint& a, const TrackPoint& b) {
            return a.timestamp < b.timestamp;
        });

    // Remove duplicates and near-duplicates
    allPoints.erase(std::unique(allPoints.begin(), allPoints.end(),
        [this](const TrackPoint& a, const TrackPoint& b) {
            return std::chrono::duration<double>(b.timestamp - a.timestamp).count() < 0.1 &&
                   std::abs(a.frequency - b.frequency) < config_.frequencyTolerance &&
                   std::abs(a.bandwidth - b.bandwidth) < a.bandwidth * config_.bandwidthTolerance;
        }), allPoints.end());

    // Copy sorted points to merged track
    mergedTrack->points.insert(mergedTrack->points.end(), 
        allPoints.begin(), allPoints.end());

    // Determine primary class based on confidence
    std::map<SignalClass, double> totalConfidence;
    for (const auto& point : mergedTrack->points) {
        totalConfidence[point.signalClass] += point.confidence;
    }

    mergedTrack->primaryClass = std::max_element(
        totalConfidence.begin(), totalConfidence.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; }
    )->first;

    // Store merged track
    tracks_[mergedTrack->id] = mergedTrack;

    // Remove original tracks
    if (endCallback_) {
        endCallback_(*track1);
        endCallback_(*track2);
    }
    tracks_.erase(it1);
    tracks_.erase(it2);

    stats_["merged_tracks"]++;

    // Notify track update
    if (updateCallback_) {
        updateCallback_(*mergedTrack);
    }

    return mergedTrack->id;
}

void SignalTracker::cleanupTracks() {
    auto now = std::chrono::system_clock::now();
    std::vector<std::string> tracksToRemove;

    for (const auto& [trackId, track] : tracks_) {
        // Check if track is too old
        auto timeSinceUpdate = std::chrono::duration<double>(
            now - track->lastUpdate).count();

        if (timeSinceUpdate > config_.timeWindow) {
            track->active = false;
        }

        // Remove completely inactive tracks with no recent updates
        if (!track->active && timeSinceUpdate > config_.timeWindow * 2) {
            tracksToRemove.push_back(trackId);
        }
        else {
            // Clean up old points
            auto cutoffTime = now - 
                std::chrono::duration<double>(config_.timeWindow);
            
            while (!track->points.empty() && 
                   track->points.front().timestamp < cutoffTime) {
                track->points.pop_front();
            }
        }
    }

    // Remove old tracks
    for (const auto& trackId : tracksToRemove) {
        if (endCallback_) {
            endCallback_(*tracks_[trackId]);
        }
        tracks_.erase(trackId);
    }
}

std::string SignalTracker::generateTrackId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 35); // 0-9 and a-z

    std::stringstream ss;
    ss << "track_";
    for (int i = 0; i < 8; ++i) {
        int r = dis(gen);
        ss << static_cast<char>(r < 10 ? '0' + r : 'a' + (r - 10));
    }
    return ss.str();
}

std::shared_ptr<Track> SignalTracker::getTrack(const std::string& trackId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tracks_.find(trackId);
    return (it != tracks_.end()) ? it->second : nullptr;
}

std::vector<std::shared_ptr<Track>> SignalTracker::getActiveTracks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<Track>> activeTracks;
    
    for (const auto& [trackId, track] : tracks_) {
        if (track->active) {
            activeTracks.push_back(track);
        }
    }
    
    return activeTracks;
}

void SignalTracker::setTrackUpdateCallback(TrackUpdateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    updateCallback_ = callback;
}

void SignalTracker::setTrackEndCallback(TrackEndCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    endCallback_ = callback;
}

void SignalTracker::updateConfig(const TrackingConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

TrackingConfig SignalTracker::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

std::map<std::string, double> SignalTracker::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

} // namespace signal
} // namespace tdoa 