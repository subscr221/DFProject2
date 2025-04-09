#pragma once

#include <string>
#include <memory>
#include <optional>
#include <cmath>

namespace df {
namespace ui {

/**
 * @struct SignalInfo
 * @brief Information about a detected signal
 */
struct SignalInfo {
    double latitude;                    ///< Signal latitude
    double longitude;                   ///< Signal longitude
    double frequency;                   ///< Signal frequency in Hz
    double power;                       ///< Signal power in dBm
    double confidenceLevel;             ///< Confidence level (0.0-1.0)
    std::optional<double> semiMajorAxis; ///< Semi-major axis of confidence ellipse (meters)
    std::optional<double> semiMinorAxis; ///< Semi-minor axis of confidence ellipse (meters)
    std::optional<double> orientation;   ///< Orientation of confidence ellipse (radians)
    std::string label;                  ///< Optional label for the signal
};

/**
 * @class SignalMarker
 * @brief Represents a signal detection on the map with confidence visualization
 */
class SignalMarker {
public:
    /**
     * @brief Constructor
     * @param info Signal information
     */
    explicit SignalMarker(const SignalInfo& info);

    /**
     * @brief Destructor
     */
    ~SignalMarker();

    /**
     * @brief Get the marker color based on confidence level
     * @return Hex color string
     */
    std::string getColor() const;

    /**
     * @brief Get the marker opacity based on confidence level
     * @return Opacity value (0.0-1.0)
     */
    double getOpacity() const;

    /**
     * @brief Get formatted tooltip content
     * @return HTML-formatted tooltip string
     */
    std::string getTooltipContent() const;

    /**
     * @brief Get signal information
     * @return Const reference to signal info
     */
    const SignalInfo& getInfo() const { return info_; }

    /**
     * @brief Update signal information
     * @param newInfo Updated signal information
     */
    void update(const SignalInfo& newInfo);

private:
    SignalInfo info_;
    
    // Helper functions
    std::string formatFrequency(double freq) const;
    std::string formatPower(double power) const;
    std::string interpolateColor(double confidence) const;
};

} // namespace ui
} // namespace df 