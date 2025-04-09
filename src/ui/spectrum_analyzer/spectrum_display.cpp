#include "spectrum_display.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

namespace df {
namespace ui {

// Default vertex shader source for spectrum rendering
const char* SPECTRUM_VERTEX_SHADER_SRC = R"glsl(
#version 330 core
layout (location = 0) in vec2 aPos;

uniform mat4 uProjection;
uniform mat4 uView;

void main() {
    gl_Position = uProjection * uView * vec4(aPos.x, aPos.y, 0.0, 1.0);
}
)glsl";

// Default fragment shader source for spectrum rendering
const char* SPECTRUM_FRAGMENT_SHADER_SRC = R"glsl(
#version 330 core
out vec4 FragColor;

uniform vec4 uColor;

void main() {
    FragColor = uColor;
}
)glsl";

// Default vertex shader source for grid rendering
const char* GRID_VERTEX_SHADER_SRC = R"glsl(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;

uniform mat4 uProjection;
uniform mat4 uView;

out vec4 vertexColor;

void main() {
    gl_Position = uProjection * uView * vec4(aPos.x, aPos.y, 0.0, 1.0);
    vertexColor = aColor;
}
)glsl";

// Default fragment shader source for grid rendering
const char* GRID_FRAGMENT_SHADER_SRC = R"glsl(
#version 330 core
in vec4 vertexColor;
out vec4 FragColor;

void main() {
    FragColor = vertexColor;
}
)glsl";

SpectrumDisplay::SpectrumDisplay()
    : width_(0),
      height_(0),
      startFreq_(0.0),
      endFreq_(0.0),
      minAmplitude_(-100.0f),
      maxAmplitude_(0.0f),
      peakDetectionEnabled_(false),
      autoScaleEnabled_(true),
      vao_(0),
      vbo_(0),
      gridVao_(0),
      gridVbo_(0) {
}

SpectrumDisplay::~SpectrumDisplay() {
    // Clean up OpenGL resources
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    
    if (gridVao_ != 0) {
        glDeleteVertexArrays(1, &gridVao_);
        gridVao_ = 0;
    }
    
    if (gridVbo_ != 0) {
        glDeleteBuffers(1, &gridVbo_);
        gridVbo_ = 0;
    }
}

bool SpectrumDisplay::initialize(int width, int height) {
    width_ = width;
    height_ = height;
    
    // Initialize default frequency range (100MHz to 6GHz)
    startFreq_ = 100.0e6;
    endFreq_ = 6.0e9;
    
    // Prepare shaders
    if (!prepareShaders()) {
        std::cerr << "Failed to prepare shaders for spectrum display" << std::endl;
        return false;
    }
    
    // Setup OpenGL buffers
    setupBuffers();
    
    // Update grid lines
    updateGridLines();
    
    return true;
}

bool SpectrumDisplay::prepareShaders() {
    // Create spectrum shader program
    spectrumShader_ = std::make_unique<ShaderProgram>();
    if (!spectrumShader_->createFromStrings(SPECTRUM_VERTEX_SHADER_SRC, SPECTRUM_FRAGMENT_SHADER_SRC)) {
        std::cerr << "Failed to create spectrum shader program" << std::endl;
        return false;
    }
    
    // Create grid shader program
    gridShader_ = std::make_unique<ShaderProgram>();
    if (!gridShader_->createFromStrings(GRID_VERTEX_SHADER_SRC, GRID_FRAGMENT_SHADER_SRC)) {
        std::cerr << "Failed to create grid shader program" << std::endl;
        return false;
    }
    
    return true;
}

void SpectrumDisplay::setupBuffers() {
    // Create and bind VAO and VBO for spectrum
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW); // We'll update this later
    
    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
    
    // Create and bind VAO and VBO for grid
    glGenVertexArrays(1, &gridVao_);
    glGenBuffers(1, &gridVbo_);
    
    glBindVertexArray(gridVao_);
    glBindBuffer(GL_ARRAY_BUFFER, gridVbo_);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW); // We'll update this later
    
    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Color attribute
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
}

void SpectrumDisplay::updateData(const std::vector<float>& frequencyData, double startFreq, double endFreq) {
    frequencyData_ = frequencyData;
    startFreq_ = startFreq;
    endFreq_ = endFreq;
    
    if (autoScaleEnabled_) {
        // Find min/max amplitude for auto-scaling
        auto [minIt, maxIt] = std::minmax_element(frequencyData_.begin(), frequencyData_.end());
        float dataMin = *minIt;
        float dataMax = *maxIt;
        
        // Add some padding
        minAmplitude_ = std::floor(dataMin - 10.0f);
        maxAmplitude_ = std::ceil(dataMax + 5.0f);
    }
    
    // Update the spectrum vertex data
    if (frequencyData_.empty()) {
        return;
    }
    
    size_t numPoints = frequencyData_.size();
    std::vector<float> vertices(numPoints * 2); // (x, y) for each point
    
    double freqStep = (endFreq_ - startFreq_) / (numPoints - 1);
    
    for (size_t i = 0; i < numPoints; ++i) {
        double freq = startFreq_ + i * freqStep;
        float amp = frequencyData_[i];
        
        // Normalize to 0-1 range
        float x = static_cast<float>(i) / (numPoints - 1);
        float y = (amp - minAmplitude_) / (maxAmplitude_ - minAmplitude_);
        
        // Clamp y to [0, 1]
        y = std::max(0.0f, std::min(1.0f, y));
        
        vertices[i * 2] = x;
        vertices[i * 2 + 1] = y;
    }
    
    // Update the VBO
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
    
    // If peak detection is enabled, find peaks
    if (peakDetectionEnabled_) {
        findPeaks();
    }
}

void SpectrumDisplay::findPeaks() {
    peaks_.clear();
    
    if (frequencyData_.size() < 3) {
        return;
    }
    
    // Simple peak detection algorithm
    double freqStep = (endFreq_ - startFreq_) / (frequencyData_.size() - 1);
    
    for (size_t i = 1; i < frequencyData_.size() - 1; ++i) {
        if (frequencyData_[i] > frequencyData_[i - 1] && 
            frequencyData_[i] > frequencyData_[i + 1] &&
            frequencyData_[i] > (minAmplitude_ + (maxAmplitude_ - minAmplitude_) * 0.7f)) {
            
            double peakFreq = startFreq_ + i * freqStep;
            float peakAmp = frequencyData_[i];
            
            peaks_.emplace_back(peakFreq, peakAmp);
        }
    }
    
    // Sort peaks by amplitude (descending)
    std::sort(peaks_.begin(), peaks_.end(), 
        [](const auto& a, const auto& b) {
            return a.second > b.second;
        });
    
    // Keep only the top N peaks
    constexpr size_t MAX_PEAKS = 5;
    if (peaks_.size() > MAX_PEAKS) {
        peaks_.resize(MAX_PEAKS);
    }
}

void SpectrumDisplay::render() {
    // Set up view and projection matrices
    glm::mat4 projection = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f);
    glm::mat4 view = glm::mat4(1.0f);
    
    // Draw grid lines first
    drawGrid();
    
    // Draw spectrum
    if (!frequencyData_.empty()) {
        spectrumShader_->use();
        spectrumShader_->setMat4("uProjection", projection);
        spectrumShader_->setMat4("uView", view);
        spectrumShader_->setVec4("uColor", glm::vec4(0.0f, 0.8f, 0.0f, 1.0f)); // Green color
        
        glBindVertexArray(vao_);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(frequencyData_.size()));
        
        // Draw peak markers if peak detection is enabled
        if (peakDetectionEnabled_ && !peaks_.empty()) {
            // Change color for peaks
            spectrumShader_->setVec4("uColor", glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)); // Red color
            
            for (const auto& peak : peaks_) {
                double normalizedX = (peak.first - startFreq_) / (endFreq_ - startFreq_);
                float normalizedY = (peak.second - minAmplitude_) / (maxAmplitude_ - minAmplitude_);
                
                // Ensure within bounds
                normalizedX = std::max(0.0, std::min(1.0, normalizedX));
                normalizedY = std::max(0.0f, std::min(1.0f, normalizedY));
                
                // Draw point at peak
                glPointSize(8.0f);
                
                float peakVertex[] = { static_cast<float>(normalizedX), normalizedY };
                glBindBuffer(GL_ARRAY_BUFFER, vbo_);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(peakVertex), peakVertex);
                
                glDrawArrays(GL_POINTS, 0, 1);
            }
        }
    }
}

void SpectrumDisplay::updateGridLines() {
    // Create grid lines for frequency (x-axis) and amplitude (y-axis)
    std::vector<float> gridVertices;
    
    // Horizontal grid lines (amplitude)
    int numHorizontalLines = 5;
    float stepY = 1.0f / numHorizontalLines;
    
    for (int i = 0; i <= numHorizontalLines; ++i) {
        float y = i * stepY;
        
        // Line vertices: (x, y, r, g, b, a)
        float alpha = (i == 0 || i == numHorizontalLines) ? 1.0f : 0.5f;
        
        // Start point
        gridVertices.push_back(0.0f);          // x
        gridVertices.push_back(y);             // y
        gridVertices.push_back(0.7f);          // r
        gridVertices.push_back(0.7f);          // g
        gridVertices.push_back(0.7f);          // b
        gridVertices.push_back(alpha);         // a
        
        // End point
        gridVertices.push_back(1.0f);          // x
        gridVertices.push_back(y);             // y
        gridVertices.push_back(0.7f);          // r
        gridVertices.push_back(0.7f);          // g
        gridVertices.push_back(0.7f);          // b
        gridVertices.push_back(alpha);         // a
    }
    
    // Vertical grid lines (frequency)
    int numVerticalLines = 5;
    float stepX = 1.0f / numVerticalLines;
    
    for (int i = 0; i <= numVerticalLines; ++i) {
        float x = i * stepX;
        
        // Line vertices: (x, y, r, g, b, a)
        float alpha = (i == 0 || i == numVerticalLines) ? 1.0f : 0.5f;
        
        // Start point
        gridVertices.push_back(x);             // x
        gridVertices.push_back(0.0f);          // y
        gridVertices.push_back(0.7f);          // r
        gridVertices.push_back(0.7f);          // g
        gridVertices.push_back(0.7f);          // b
        gridVertices.push_back(alpha);         // a
        
        // End point
        gridVertices.push_back(x);             // x
        gridVertices.push_back(1.0f);          // y
        gridVertices.push_back(0.7f);          // r
        gridVertices.push_back(0.7f);          // g
        gridVertices.push_back(0.7f);          // b
        gridVertices.push_back(alpha);         // a
    }
    
    // Update grid buffer
    glBindBuffer(GL_ARRAY_BUFFER, gridVbo_);
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_STATIC_DRAW);
}

void SpectrumDisplay::drawGrid() {
    // Set up view and projection matrices
    glm::mat4 projection = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f);
    glm::mat4 view = glm::mat4(1.0f);
    
    gridShader_->use();
    gridShader_->setMat4("uProjection", projection);
    gridShader_->setMat4("uView", view);
    
    glBindVertexArray(gridVao_);
    glLineWidth(1.0f);
    
    // Number of horizontal and vertical lines
    int numHorizontalLines = 5;
    int numVerticalLines = 5;
    
    // Draw all grid lines (2 vertices per line)
    glDrawArrays(GL_LINES, 0, (numHorizontalLines + 1) * 2 + (numVerticalLines + 1) * 2);
}

void SpectrumDisplay::resize(int width, int height) {
    width_ = width;
    height_ = height;
}

void SpectrumDisplay::setFrequencyRange(double startFreq, double endFreq) {
    if (startFreq >= endFreq) {
        std::cerr << "Warning: Invalid frequency range (start >= end)" << std::endl;
        return;
    }
    
    startFreq_ = startFreq;
    endFreq_ = endFreq;
}

void SpectrumDisplay::setAmplitudeRange(float minAmplitude, float maxAmplitude) {
    if (minAmplitude >= maxAmplitude) {
        std::cerr << "Warning: Invalid amplitude range (min >= max)" << std::endl;
        return;
    }
    
    minAmplitude_ = minAmplitude;
    maxAmplitude_ = maxAmplitude;
    autoScaleEnabled_ = false;
}

void SpectrumDisplay::enablePeakDetection(bool enable) {
    peakDetectionEnabled_ = enable;
}

void SpectrumDisplay::enableAutoScale(bool enable) {
    autoScaleEnabled_ = enable;
}

std::pair<double, float> SpectrumDisplay::getPeakValue() const {
    if (peaks_.empty()) {
        return {0.0, 0.0f};
    }
    
    // Return the highest peak (first in the sorted list)
    return peaks_.front();
}

void SpectrumDisplay::pan(double deltaX) {
    double freqRange = endFreq_ - startFreq_;
    double delta = deltaX * freqRange;
    
    startFreq_ += delta;
    endFreq_ += delta;
}

void SpectrumDisplay::zoom(float factor, double centerFreq) {
    double freqRange = endFreq_ - startFreq_;
    
    // Calculate the proportion of the center frequency within the current range
    double centerProportion = (centerFreq - startFreq_) / freqRange;
    
    // Calculate new range
    double newRange = freqRange / factor;
    
    // Calculate new start and end frequencies, keeping the center frequency at the same proportion
    double newStartFreq = centerFreq - centerProportion * newRange;
    double newEndFreq = newStartFreq + newRange;
    
    startFreq_ = newStartFreq;
    endFreq_ = newEndFreq;
}

} // namespace ui
} // namespace df 