#include "signal_marker.h"
#include <sstream>
#include <iomanip>

namespace df {
namespace ui {

SignalMarker::SignalMarker(const SignalInfo& info) : info_(info) {}

SignalMarker::~SignalMarker() = default;

std::string SignalMarker::getColor() const {
    return interpolateColor(info_.confidenceLevel);
}

double SignalMarker::getOpacity() const {
    // Scale opacity between 0.3 and 1.0 based on confidence
    return 0.3 + (info_.confidenceLevel * 0.7);
}

std::string SignalMarker::getTooltipContent() const {
    std::stringstream ss;
    ss << "<div class='signal-tooltip'>";
    
    if (!info_.label.empty()) {
        ss << "<strong>" << info_.label << "</strong><br>";
    }
    
    ss << "Frequency: " << formatFrequency(info_.frequency) << "<br>"
       << "Power: " << formatPower(info_.power) << "<br>"
       << "Confidence: " << std::fixed << std::setprecision(1) 
       << (info_.confidenceLevel * 100.0) << "%";
    
    ss << "</div>";
    return ss.str();
}

void SignalMarker::update(const SignalInfo& newInfo) {
    info_ = newInfo;
}

std::string SignalMarker::formatFrequency(double freq) const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(3);
    
    if (freq >= 1e9) {
        ss << (freq / 1e9) << " GHz";
    } else if (freq >= 1e6) {
        ss << (freq / 1e6) << " MHz";
    } else if (freq >= 1e3) {
        ss << (freq / 1e3) << " kHz";
    } else {
        ss << freq << " Hz";
    }
    
    return ss.str();
}

std::string SignalMarker::formatPower(double power) const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << power << " dBm";
    return ss.str();
}

std::string SignalMarker::interpolateColor(double confidence) const {
    // Interpolate between red (low confidence) and green (high confidence)
    // through yellow (medium confidence)
    int r, g;
    
    if (confidence < 0.5) {
        // Red to Yellow
        r = 255;
        g = static_cast<int>(510 * confidence);
    } else {
        // Yellow to Green
        r = static_cast<int>(510 * (1.0 - confidence));
        g = 255;
    }
    
    // Ensure values are in valid range
    r = std::min(255, std::max(0, r));
    g = std::min(255, std::max(0, g));
    
    std::stringstream ss;
    ss << "#" << std::hex << std::setfill('0')
       << std::setw(2) << r
       << std::setw(2) << g
       << "00";  // Blue component is always 0
    
    return ss.str();
}

} // namespace ui
} // namespace df 