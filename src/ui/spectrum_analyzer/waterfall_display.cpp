#include "waterfall_display.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

namespace df {
namespace ui {

// Default vertex shader source for waterfall rendering
const char* WATERFALL_VERTEX_SHADER_SRC = R"glsl(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

uniform mat4 uProjection;
uniform mat4 uView;

out vec2 TexCoord;

void main() {
    gl_Position = uProjection * uView * vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)glsl";

// Default fragment shader source for waterfall rendering
const char* WATERFALL_FRAGMENT_SHADER_SRC = R"glsl(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D uWaterfallTexture;
uniform sampler2D uColorMapTexture;

void main() {
    // Sample the waterfall texture to get intensity value
    float intensity = texture(uWaterfallTexture, TexCoord).r;
    
    // Use the intensity to look up the color from color map
    vec4 color = texture(uColorMapTexture, vec2(intensity, 0.5));
    
    FragColor = color;
}
)glsl";

WaterfallDisplay::WaterfallDisplay()
    : width_(0),
      height_(0),
      startFreq_(0.0),
      endFreq_(0.0),
      minAmplitude_(-100.0f),
      maxAmplitude_(0.0f),
      timeScale_(10.0f),
      paused_(false),
      scrollOffset_(0),
      autoScaleEnabled_(true),
      colorMapIndex_(0),
      historySize_(256),
      vao_(0),
      vbo_(0),
      texture_(0),
      colorMapTexture_(0) {
}

WaterfallDisplay::~WaterfallDisplay() {
    // Clean up OpenGL resources
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    
    if (texture_ != 0) {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }
    
    if (colorMapTexture_ != 0) {
        glDeleteTextures(1, &colorMapTexture_);
        colorMapTexture_ = 0;
    }
}

bool WaterfallDisplay::initialize(int width, int height, size_t historySize) {
    width_ = width;
    height_ = height;
    historySize_ = historySize;
    
    // Initialize default frequency range (100MHz to 6GHz)
    startFreq_ = 100.0e6;
    endFreq_ = 6.0e9;
    
    // Prepare shaders
    if (!prepareShaders()) {
        std::cerr << "Failed to prepare shaders for waterfall display" << std::endl;
        return false;
    }
    
    // Generate color maps
    generateColorMaps();
    
    // Setup OpenGL buffers and textures
    setupBuffers();
    
    // Reserve space for texture data
    textureData_.resize(width_ * historySize_, 0);
    
    return true;
}

bool WaterfallDisplay::prepareShaders() {
    // Create waterfall shader program
    waterfallShader_ = std::make_unique<ShaderProgram>();
    if (!waterfallShader_->createFromStrings(WATERFALL_VERTEX_SHADER_SRC, WATERFALL_FRAGMENT_SHADER_SRC)) {
        std::cerr << "Failed to create waterfall shader program" << std::endl;
        return false;
    }
    
    return true;
}

void WaterfallDisplay::setupBuffers() {
    // Create and bind VAO and VBO
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    
    // Quad vertices with texture coordinates (x, y, tx, ty)
    float vertices[] = {
        // positions        // texture coords
        1.0f, 1.0f,         1.0f, 0.0f,   // top right
        1.0f, 0.0f,         1.0f, 1.0f,   // bottom right
        0.0f, 0.0f,         0.0f, 1.0f,   // bottom left
        0.0f, 1.0f,         0.0f, 0.0f    // top left
    };
    
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Texture coordinates attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Generate and bind waterfall texture
    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Initialize with empty texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width_, historySize_, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    
    // Generate and bind color map texture
    glGenTextures(1, &colorMapTexture_);
    glBindTexture(GL_TEXTURE_2D, colorMapTexture_);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Upload color map data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, colorMapData_.data());
    
    glBindVertexArray(0);
}

void WaterfallDisplay::generateColorMaps() {
    // Create three different color maps: thermal, rainbow, grayscale
    colorMapData_.resize(256 * 3); // 256 colors, RGB
    
    // Generate thermal color map by default
    for (int i = 0; i < 256; ++i) {
        float t = i / 255.0f;
        
        // Thermal color map (black -> red -> yellow -> white)
        uint8_t r = 0, g = 0, b = 0;
        
        if (t < 0.33f) {
            // Black to red
            r = static_cast<uint8_t>((t / 0.33f) * 255);
        } else if (t < 0.66f) {
            // Red to yellow
            r = 255;
            g = static_cast<uint8_t>(((t - 0.33f) / 0.33f) * 255);
        } else {
            // Yellow to white
            r = 255;
            g = 255;
            b = static_cast<uint8_t>(((t - 0.66f) / 0.34f) * 255);
        }
        
        colorMapData_[i * 3] = r;
        colorMapData_[i * 3 + 1] = g;
        colorMapData_[i * 3 + 2] = b;
    }
}

void WaterfallDisplay::setColorMap(int colorMapIndex) {
    if (colorMapIndex < 0 || colorMapIndex > 2) {
        std::cerr << "Invalid color map index: " << colorMapIndex << std::endl;
        return;
    }
    
    colorMapIndex_ = colorMapIndex;
    
    // Regenerate color map data
    for (int i = 0; i < 256; ++i) {
        float t = i / 255.0f;
        uint8_t r = 0, g = 0, b = 0;
        
        switch (colorMapIndex_) {
            case 0: // Thermal (black -> red -> yellow -> white)
                if (t < 0.33f) {
                    r = static_cast<uint8_t>((t / 0.33f) * 255);
                } else if (t < 0.66f) {
                    r = 255;
                    g = static_cast<uint8_t>(((t - 0.33f) / 0.33f) * 255);
                } else {
                    r = 255;
                    g = 255;
                    b = static_cast<uint8_t>(((t - 0.66f) / 0.34f) * 255);
                }
                break;
                
            case 1: // Rainbow (blue -> cyan -> green -> yellow -> red)
                if (t < 0.25f) {
                    b = 255;
                    g = static_cast<uint8_t>((t / 0.25f) * 255);
                } else if (t < 0.5f) {
                    g = 255;
                    b = static_cast<uint8_t>(255 - ((t - 0.25f) / 0.25f) * 255);
                } else if (t < 0.75f) {
                    g = 255;
                    r = static_cast<uint8_t>(((t - 0.5f) / 0.25f) * 255);
                } else {
                    r = 255;
                    g = static_cast<uint8_t>(255 - ((t - 0.75f) / 0.25f) * 255);
                }
                break;
                
            case 2: // Grayscale
                r = g = b = static_cast<uint8_t>(t * 255);
                break;
        }
        
        colorMapData_[i * 3] = r;
        colorMapData_[i * 3 + 1] = g;
        colorMapData_[i * 3 + 2] = b;
    }
    
    // Update the color map texture
    glBindTexture(GL_TEXTURE_2D, colorMapTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, colorMapData_.data());
}

void WaterfallDisplay::addTrace(const std::vector<float>& frequencyData, double startFreq, double endFreq) {
    if (paused_) {
        return;
    }
    
    if (frequencyData.empty()) {
        return;
    }
    
    startFreq_ = startFreq;
    endFreq_ = endFreq;
    
    // Add to history
    historyData_.push_front(frequencyData);
    if (historyData_.size() > historySize_) {
        historyData_.pop_back();
    }
    
    // Update amplitude range if auto-scaling is enabled
    if (autoScaleEnabled_) {
        findAmplitudeRange();
    }
    
    // Update texture
    updateTexture();
}

void WaterfallDisplay::findAmplitudeRange() {
    if (historyData_.empty()) {
        return;
    }
    
    float minValue = std::numeric_limits<float>::max();
    float maxValue = std::numeric_limits<float>::lowest();
    
    // Find min/max across all history data
    for (const auto& trace : historyData_) {
        auto [minIt, maxIt] = std::minmax_element(trace.begin(), trace.end());
        minValue = std::min(minValue, *minIt);
        maxValue = std::max(maxValue, *maxIt);
    }
    
    // Add some padding
    minAmplitude_ = std::floor(minValue - 10.0f);
    maxAmplitude_ = std::ceil(maxValue + 5.0f);
}

void WaterfallDisplay::updateTexture() {
    if (historyData_.empty()) {
        return;
    }
    
    // Create a new texture data buffer
    std::vector<uint8_t> newTextureData(width_ * historySize_, 0);
    
    // Fill texture data from history
    size_t historyIndex = 0;
    for (const auto& trace : historyData_) {
        if (historyIndex >= historySize_) {
            break;
        }
        
        // Resample the trace data to fit the texture width
        size_t inputSize = trace.size();
        
        for (int x = 0; x < width_; ++x) {
            // Map texture x coordinate to trace index
            double traceIndex = (static_cast<double>(x) / (width_ - 1)) * (inputSize - 1);
            
            // Simple linear interpolation
            size_t index1 = static_cast<size_t>(traceIndex);
            size_t index2 = std::min(index1 + 1, inputSize - 1);
            double fraction = traceIndex - index1;
            
            float value = trace[index1] * (1.0 - fraction) + trace[index2] * fraction;
            
            // Normalize to [0, 1]
            float normalized = normalize(value, minAmplitude_, maxAmplitude_);
            
            // Convert to byte for texture
            uint8_t byteValue = static_cast<uint8_t>(std::min(std::max(normalized * 255.0f, 0.0f), 255.0f));
            
            // Store in the texture data
            newTextureData[historyIndex * width_ + x] = byteValue;
        }
        
        historyIndex++;
    }
    
    // Update texture data
    textureData_ = std::move(newTextureData);
    
    // Update the OpenGL texture
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width_, historySize_, 0, GL_RED, GL_UNSIGNED_BYTE, textureData_.data());
}

float WaterfallDisplay::normalize(float value, float min, float max) const {
    if (min >= max) {
        return 0.5f;
    }
    
    // Clamp to range and normalize
    value = std::min(std::max(value, min), max);
    return (value - min) / (max - min);
}

void WaterfallDisplay::render() {
    // Set up view and projection matrices
    glm::mat4 projection = glm::ortho(0.0f, 1.0f, 0.0f, 1.0f);
    glm::mat4 view = glm::mat4(1.0f);
    
    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, colorMapTexture_);
    
    // Draw the quad
    waterfallShader_->use();
    waterfallShader_->setMat4("uProjection", projection);
    waterfallShader_->setMat4("uView", view);
    waterfallShader_->setInt("uWaterfallTexture", 0);
    waterfallShader_->setInt("uColorMapTexture", 1);
    
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
}

void WaterfallDisplay::resize(int width, int height) {
    // If width changed, we need to recreate the texture
    if (width != width_) {
        width_ = width;
        textureData_.resize(width_ * historySize_, 0);
        
        // Update the texture
        if (!historyData_.empty()) {
            updateTexture();
        }
    }
    
    height_ = height;
}

void WaterfallDisplay::setFrequencyRange(double startFreq, double endFreq) {
    if (startFreq >= endFreq) {
        std::cerr << "Warning: Invalid frequency range (start >= end)" << std::endl;
        return;
    }
    
    startFreq_ = startFreq;
    endFreq_ = endFreq;
}

void WaterfallDisplay::setAmplitudeRange(float minAmplitude, float maxAmplitude) {
    if (minAmplitude >= maxAmplitude) {
        std::cerr << "Warning: Invalid amplitude range (min >= max)" << std::endl;
        return;
    }
    
    minAmplitude_ = minAmplitude;
    maxAmplitude_ = maxAmplitude;
    autoScaleEnabled_ = false;
    
    // Update the texture
    updateTexture();
}

void WaterfallDisplay::setTimeScale(float timeScale) {
    if (timeScale <= 0.0f) {
        std::cerr << "Warning: Invalid time scale (must be positive)" << std::endl;
        return;
    }
    
    timeScale_ = timeScale;
}

void WaterfallDisplay::setPaused(bool paused) {
    paused_ = paused;
    
    if (!paused_) {
        // Reset scroll offset when resuming
        scrollOffset_ = 0;
    }
}

void WaterfallDisplay::scroll(int scrollOffset) {
    if (!paused_) {
        // Only scroll when paused
        return;
    }
    
    scrollOffset_ = scrollOffset;
    
    // Clamp to valid range
    int maxScroll = static_cast<int>(historyData_.size()) - 1;
    scrollOffset_ = std::min(std::max(scrollOffset_, 0), maxScroll);
    
    // Update texture based on scroll offset
    updateTexture();
}

void WaterfallDisplay::enableAutoScale(bool enable) {
    autoScaleEnabled_ = enable;
    
    if (enable) {
        findAmplitudeRange();
        updateTexture();
    }
}

} // namespace ui
} // namespace df 