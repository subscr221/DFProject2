#include "measurement_tools.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

namespace df {
namespace ui {

// Default vertex shader source for marker and measurement rendering
const char* MEASUREMENT_VERTEX_SHADER_SRC = R"glsl(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;

uniform mat4 uProjection;
uniform mat4 uView;

out vec4 vertexColor;

void main() {
    gl_Position = uProjection * uView * vec4(aPos, 0.0, 1.0);
    vertexColor = aColor;
}
)glsl";

// Default fragment shader source for marker and measurement rendering
const char* MEASUREMENT_FRAGMENT_SHADER_SRC = R"glsl(
#version 330 core
in vec4 vertexColor;
out vec4 FragColor;

void main() {
    FragColor = vertexColor;
}
)glsl";

MeasurementTools::MeasurementTools()
    : width_(0),
      height_(0),
      markerVao_(0),
      markerVbo_(0),
      bandwidthVao_(0),
      bandwidthVbo_(0),
      nextMarkerId_(1),
      nextMeasurementId_(1) {
}

MeasurementTools::~MeasurementTools() {
    // Clean up OpenGL resources
    if (markerVao_ != 0) {
        glDeleteVertexArrays(1, &markerVao_);
        markerVao_ = 0;
    }
    
    if (markerVbo_ != 0) {
        glDeleteBuffers(1, &markerVbo_);
        markerVbo_ = 0;
    }
    
    if (bandwidthVao_ != 0) {
        glDeleteVertexArrays(1, &bandwidthVao_);
        bandwidthVao_ = 0;
    }
    
    if (bandwidthVbo_ != 0) {
        glDeleteBuffers(1, &bandwidthVbo_);
        bandwidthVbo_ = 0;
    }
}

bool MeasurementTools::initialize(int width, int height) {
    width_ = width;
    height_ = height;
    
    // Prepare shaders
    if (!prepareShaders()) {
        std::cerr << "Failed to prepare shaders for measurement tools" << std::endl;
        return false;
    }
    
    // Setup OpenGL buffers
    setupBuffers();
    
    return true;
}

bool MeasurementTools::prepareShaders() {
    // Create measurement shader program
    measurementShader_ = std::make_unique<ShaderProgram>();
    if (!measurementShader_->createFromStrings(MEASUREMENT_VERTEX_SHADER_SRC, MEASUREMENT_FRAGMENT_SHADER_SRC)) {
        std::cerr << "Failed to create measurement shader program" << std::endl;
        return false;
    }
    
    return true;
}

void MeasurementTools::setupBuffers() {
    // Create and bind VAO and VBO for markers
    glGenVertexArrays(1, &markerVao_);
    glGenBuffers(1, &markerVbo_);
    
    glBindVertexArray(markerVao_);
    glBindBuffer(GL_ARRAY_BUFFER, markerVbo_);
    
    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Create and bind VAO and VBO for bandwidth measurements
    glGenVertexArrays(1, &bandwidthVao_);
    glGenBuffers(1, &bandwidthVbo_);
    
    glBindVertexArray(bandwidthVao_);
    glBindBuffer(GL_ARRAY_BUFFER, bandwidthVbo_);
    
    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
}

void MeasurementTools::render(const std::vector<float>& frequencyData, double startFreq, double endFreq,
                              float minAmplitude, float maxAmplitude) {
    // Set up view and projection matrices
    glm::mat4 projection = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f);
    glm::mat4 view = glm::mat4(1.0f);
    
    measurementShader_->use();
    measurementShader_->setMat4("uProjection", projection);
    measurementShader_->setMat4("uView", view);
    
    // Update marker values based on current spectrum data
    updateMarkerValues(frequencyData, startFreq, endFreq);
    
    // Draw markers
    for (const auto& marker : markers_) {
        if (!marker.active) continue;
        
        double freqRange = endFreq - startFreq;
        float normalizedX = static_cast<float>((marker.frequency - startFreq) / freqRange);
        float normalizedY = (marker.amplitude - minAmplitude) / (maxAmplitude - minAmplitude);
        
        // Ensure within bounds
        normalizedX = std::max(0.0f, std::min(normalizedX, 1.0f));
        normalizedY = std::max(0.0f, std::min(normalizedY, 1.0f));
        
        drawMarker(marker, normalizedX, normalizedY);
    }
    
    // Draw bandwidth measurements
    for (const auto& bwm : bwMeasurements_) {
        if (!bwm.active) continue;
        
        double freqRange = endFreq - startFreq;
        float startX = static_cast<float>((bwm.startFreq - startFreq) / freqRange);
        float endX = static_cast<float>((bwm.endFreq - startFreq) / freqRange);
        
        // Ensure within bounds
        startX = std::max(0.0f, std::min(startX, 1.0f));
        endX = std::max(0.0f, std::min(endX, 1.0f));
        
        // Calculate Y positions
        float referenceY = (bwm.referencedB - minAmplitude) / (maxAmplitude - minAmplitude);
        float offsetY = (bwm.referencedB + bwm.offsetdB - minAmplitude) / (maxAmplitude - minAmplitude);
        
        // Ensure within bounds
        referenceY = std::max(0.0f, std::min(referenceY, 1.0f));
        offsetY = std::max(0.0f, std::min(offsetY, 1.0f));
        
        drawBandwidthMeasurement(bwm, startX, endX, referenceY, offsetY);
    }
}

void MeasurementTools::drawMarker(const Marker& marker, float screenX, float screenY) {
    // Marker is a cross with a line to the x-axis
    std::vector<float> vertices;
    
    float crossSize = 0.01f;
    
    // Cross horizontal line
    vertices.push_back(screenX - crossSize);         // x
    vertices.push_back(screenY);                     // y
    vertices.push_back(marker.color.r);              // r
    vertices.push_back(marker.color.g);              // g
    vertices.push_back(marker.color.b);              // b
    vertices.push_back(marker.color.a);              // a
    
    vertices.push_back(screenX + crossSize);         // x
    vertices.push_back(screenY);                     // y
    vertices.push_back(marker.color.r);              // r
    vertices.push_back(marker.color.g);              // g
    vertices.push_back(marker.color.b);              // b
    vertices.push_back(marker.color.a);              // a
    
    // Cross vertical line
    vertices.push_back(screenX);                     // x
    vertices.push_back(screenY - crossSize);         // y
    vertices.push_back(marker.color.r);              // r
    vertices.push_back(marker.color.g);              // g
    vertices.push_back(marker.color.b);              // b
    vertices.push_back(marker.color.a);              // a
    
    vertices.push_back(screenX);                     // x
    vertices.push_back(screenY + crossSize);         // y
    vertices.push_back(marker.color.r);              // r
    vertices.push_back(marker.color.g);              // g
    vertices.push_back(marker.color.b);              // b
    vertices.push_back(marker.color.a);              // a
    
    // Vertical line to X-axis
    vertices.push_back(screenX);                     // x
    vertices.push_back(0.0f);                        // y (x-axis)
    vertices.push_back(marker.color.r);              // r
    vertices.push_back(marker.color.g);              // g
    vertices.push_back(marker.color.b);              // b
    vertices.push_back(marker.color.a * 0.6f);       // a (less visible)
    
    vertices.push_back(screenX);                     // x
    vertices.push_back(screenY);                     // y
    vertices.push_back(marker.color.r);              // r
    vertices.push_back(marker.color.g);              // g
    vertices.push_back(marker.color.b);              // b
    vertices.push_back(marker.color.a * 0.6f);       // a (less visible)
    
    // Update the buffer
    glBindVertexArray(markerVao_);
    glBindBuffer(GL_ARRAY_BUFFER, markerVbo_);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    // Draw the marker
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, vertices.size() / 6);
    
    glBindVertexArray(0);
}

void MeasurementTools::drawBandwidthMeasurement(const BandwidthMeasurement& bwMeasurement,
                                               float startX, float endX, float levelY, float offsetY) {
    // Bandwidth measurement consists of:
    // 1. Horizontal line at reference level
    // 2. Horizontal line at offset level
    // 3. Vertical lines connecting the two levels at start and end frequencies
    std::vector<float> vertices;
    
    // Reference level horizontal line
    vertices.push_back(startX);                       // x
    vertices.push_back(levelY);                       // y
    vertices.push_back(bwMeasurement.color.r);        // r
    vertices.push_back(bwMeasurement.color.g);        // g
    vertices.push_back(bwMeasurement.color.b);        // b
    vertices.push_back(bwMeasurement.color.a);        // a
    
    vertices.push_back(endX);                         // x
    vertices.push_back(levelY);                       // y
    vertices.push_back(bwMeasurement.color.r);        // r
    vertices.push_back(bwMeasurement.color.g);        // g
    vertices.push_back(bwMeasurement.color.b);        // b
    vertices.push_back(bwMeasurement.color.a);        // a
    
    // Offset level horizontal line
    vertices.push_back(startX);                       // x
    vertices.push_back(offsetY);                      // y
    vertices.push_back(bwMeasurement.color.r);        // r
    vertices.push_back(bwMeasurement.color.g);        // g
    vertices.push_back(bwMeasurement.color.b);        // b
    vertices.push_back(bwMeasurement.color.a);        // a
    
    vertices.push_back(endX);                         // x
    vertices.push_back(offsetY);                      // y
    vertices.push_back(bwMeasurement.color.r);        // r
    vertices.push_back(bwMeasurement.color.g);        // g
    vertices.push_back(bwMeasurement.color.b);        // b
    vertices.push_back(bwMeasurement.color.a);        // a
    
    // Left vertical line
    vertices.push_back(startX);                       // x
    vertices.push_back(levelY);                       // y
    vertices.push_back(bwMeasurement.color.r);        // r
    vertices.push_back(bwMeasurement.color.g);        // g
    vertices.push_back(bwMeasurement.color.b);        // b
    vertices.push_back(bwMeasurement.color.a);        // a
    
    vertices.push_back(startX);                       // x
    vertices.push_back(offsetY);                      // y
    vertices.push_back(bwMeasurement.color.r);        // r
    vertices.push_back(bwMeasurement.color.g);        // g
    vertices.push_back(bwMeasurement.color.b);        // b
    vertices.push_back(bwMeasurement.color.a);        // a
    
    // Right vertical line
    vertices.push_back(endX);                         // x
    vertices.push_back(levelY);                       // y
    vertices.push_back(bwMeasurement.color.r);        // r
    vertices.push_back(bwMeasurement.color.g);        // g
    vertices.push_back(bwMeasurement.color.b);        // b
    vertices.push_back(bwMeasurement.color.a);        // a
    
    vertices.push_back(endX);                         // x
    vertices.push_back(offsetY);                      // y
    vertices.push_back(bwMeasurement.color.r);        // r
    vertices.push_back(bwMeasurement.color.g);        // g
    vertices.push_back(bwMeasurement.color.b);        // b
    vertices.push_back(bwMeasurement.color.a);        // a
    
    // Update the buffer
    glBindVertexArray(bandwidthVao_);
    glBindBuffer(GL_ARRAY_BUFFER, bandwidthVbo_);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    // Draw the bandwidth measurement
    glLineWidth(1.5f);
    glDrawArrays(GL_LINES, 0, vertices.size() / 6);
    
    glBindVertexArray(0);
}

void MeasurementTools::resize(int width, int height) {
    width_ = width;
    height_ = height;
}

int MeasurementTools::addMarker(double frequency, float amplitude, bool isDelta, int referenceId) {
    // Create new marker
    Marker marker;
    marker.frequency = frequency;
    marker.amplitude = amplitude;
    marker.active = true;
    marker.isDelta = isDelta;
    marker.referenceId = referenceId;
    marker.id = nextMarkerId_++;
    
    // Set marker color
    if (isDelta) {
        marker.color = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f); // Orange for delta markers
    } else {
        marker.color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); // Red for regular markers
    }
    
    // Add to list
    markers_.push_back(marker);
    
    return marker.id;
}

bool MeasurementTools::removeMarker(int markerId) {
    auto it = std::find_if(markers_.begin(), markers_.end(),
        [markerId](const Marker& m) { return m.id == markerId; });
    
    if (it != markers_.end()) {
        // Update any delta markers referencing this marker
        for (auto& marker : markers_) {
            if (marker.isDelta && marker.referenceId == markerId) {
                marker.active = false; // Deactivate the delta marker
            }
        }
        
        markers_.erase(it);
        return true;
    }
    
    return false;
}

bool MeasurementTools::moveMarker(int markerId, double frequency) {
    auto it = std::find_if(markers_.begin(), markers_.end(),
        [markerId](const Marker& m) { return m.id == markerId; });
    
    if (it != markers_.end()) {
        it->frequency = frequency;
        return true;
    }
    
    return false;
}

int MeasurementTools::addBandwidthMeasurement(double startFreq, double endFreq, float offsetdB) {
    // Create new bandwidth measurement
    BandwidthMeasurement measurement;
    measurement.startFreq = startFreq;
    measurement.endFreq = endFreq;
    measurement.referencedB = 0.0f; // Will be updated later
    measurement.offsetdB = offsetdB;
    measurement.active = true;
    measurement.id = nextMeasurementId_++;
    measurement.color = glm::vec4(0.0f, 0.7f, 1.0f, 1.0f); // Cyan for bandwidth measurements
    
    // Add to list
    bwMeasurements_.push_back(measurement);
    
    return measurement.id;
}

bool MeasurementTools::removeBandwidthMeasurement(int measurementId) {
    auto it = std::find_if(bwMeasurements_.begin(), bwMeasurements_.end(),
        [measurementId](const BandwidthMeasurement& m) { return m.id == measurementId; });
    
    if (it != bwMeasurements_.end()) {
        bwMeasurements_.erase(it);
        return true;
    }
    
    return false;
}

int MeasurementTools::addPeakMarker(const std::vector<float>& frequencyData, double startFreq, double endFreq) {
    if (frequencyData.empty()) {
        return -1;
    }
    
    // Find peak
    auto peakIt = std::max_element(frequencyData.begin(), frequencyData.end());
    size_t peakIndex = std::distance(frequencyData.begin(), peakIt);
    
    // Calculate frequency
    double freqStep = (endFreq - startFreq) / (frequencyData.size() - 1);
    double peakFreq = startFreq + peakIndex * freqStep;
    float peakAmp = *peakIt;
    
    // Add marker at peak
    return addMarker(peakFreq, peakAmp);
}

void MeasurementTools::updateMarkerValues(const std::vector<float>& frequencyData, double startFreq, double endFreq) {
    if (frequencyData.empty()) {
        return;
    }
    
    // Update regular markers
    for (auto& marker : markers_) {
        if (!marker.active || marker.isDelta) continue;
        
        marker.amplitude = interpolateAmplitude(frequencyData, startFreq, endFreq, marker.frequency);
    }
    
    // Update delta markers
    for (auto& marker : markers_) {
        if (!marker.active || !marker.isDelta) continue;
        
        // Find reference marker
        auto refIt = std::find_if(markers_.begin(), markers_.end(),
            [&marker](const Marker& m) { return m.id == marker.referenceId; });
        
        if (refIt != markers_.end() && refIt->active) {
            marker.amplitude = interpolateAmplitude(frequencyData, startFreq, endFreq, marker.frequency);
        } else {
            marker.active = false; // Deactivate if reference marker is gone
        }
    }
    
    // Update bandwidth measurements
    for (auto& bwm : bwMeasurements_) {
        if (!bwm.active) continue;
        
        // Find highest point in the range
        double centerFreq = (bwm.startFreq + bwm.endFreq) / 2.0;
        bwm.referencedB = interpolateAmplitude(frequencyData, startFreq, endFreq, centerFreq);
    }
}

float MeasurementTools::interpolateAmplitude(const std::vector<float>& frequencyData,
                                           double startFreq, double endFreq, double frequency) {
    if (frequencyData.empty() || frequency < startFreq || frequency > endFreq) {
        return 0.0f;
    }
    
    double freqStep = (endFreq - startFreq) / (frequencyData.size() - 1);
    double index = (frequency - startFreq) / freqStep;
    
    // Simple linear interpolation
    size_t index1 = static_cast<size_t>(index);
    size_t index2 = std::min(index1 + 1, frequencyData.size() - 1);
    double fraction = index - index1;
    
    return static_cast<float>(frequencyData[index1] * (1.0 - fraction) + frequencyData[index2] * fraction);
}

double MeasurementTools::calculateBandwidth(const std::vector<float>& frequencyData, double startFreq, double endFreq,
                                           double centerFreq, float offsetdB) {
    if (frequencyData.empty() || centerFreq < startFreq || centerFreq > endFreq) {
        return 0.0;
    }
    
    // Find amplitude at center frequency
    float centerAmp = interpolateAmplitude(frequencyData, startFreq, endFreq, centerFreq);
    
    // Calculate target amplitude (center - offset)
    float targetAmp = centerAmp + offsetdB;
    
    // Find start and end points where amplitude crosses the target value
    double freqStep = (endFreq - startFreq) / (frequencyData.size() - 1);
    
    // Start from center and go left to find lower frequency
    double lowerFreq = centerFreq;
    for (double freq = centerFreq; freq >= startFreq; freq -= freqStep / 10.0) { // Use smaller steps for better accuracy
        float amp = interpolateAmplitude(frequencyData, startFreq, endFreq, freq);
        if (amp <= targetAmp) {
            lowerFreq = freq;
            break;
        }
    }
    
    // Start from center and go right to find upper frequency
    double upperFreq = centerFreq;
    for (double freq = centerFreq; freq <= endFreq; freq += freqStep / 10.0) { // Use smaller steps for better accuracy
        float amp = interpolateAmplitude(frequencyData, startFreq, endFreq, freq);
        if (amp <= targetAmp) {
            upperFreq = freq;
            break;
        }
    }
    
    // Calculate bandwidth
    return upperFreq - lowerFreq;
}

float MeasurementTools::calculateIntegratedPower(const std::vector<float>& frequencyData, double startFreq, double endFreq,
                                              double rangeStartFreq, double rangeEndFreq) {
    if (frequencyData.empty() || rangeStartFreq < startFreq || rangeEndFreq > endFreq || rangeStartFreq >= rangeEndFreq) {
        return 0.0f;
    }
    
    // Convert dB values to linear power for integration
    double totalPower = 0.0;
    double freqStep = (endFreq - startFreq) / (frequencyData.size() - 1);
    
    // Number of points in range
    double startIndex = (rangeStartFreq - startFreq) / freqStep;
    double endIndex = (rangeEndFreq - startFreq) / freqStep;
    
    int startIdx = static_cast<int>(std::ceil(startIndex));
    int endIdx = static_cast<int>(std::floor(endIndex));
    
    // Handle partial first and last segments
    if (startIdx > 0 && startIndex < startIdx) {
        float amplitude = interpolateAmplitude(frequencyData, startFreq, endFreq, rangeStartFreq);
        double linearPower = std::pow(10.0, amplitude / 10.0);
        totalPower += linearPower * (startIdx - startIndex) * freqStep;
    }
    
    // Sum complete segments
    for (int i = startIdx; i <= endIdx; ++i) {
        if (i >= 0 && i < static_cast<int>(frequencyData.size())) {
            double linearPower = std::pow(10.0, frequencyData[i] / 10.0);
            totalPower += linearPower * freqStep;
        }
    }
    
    // Handle partial last segment
    if (endIdx < static_cast<int>(frequencyData.size()) - 1 && endIndex > endIdx) {
        float amplitude = interpolateAmplitude(frequencyData, startFreq, endFreq, rangeEndFreq);
        double linearPower = std::pow(10.0, amplitude / 10.0);
        totalPower += linearPower * (endIndex - endIdx) * freqStep;
    }
    
    // Convert back to dBm
    return static_cast<float>(10.0 * std::log10(totalPower));
}

} // namespace ui
} // namespace df 