#pragma once

#include <vector>
#include <deque>
#include <memory>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "shader_program.h"

namespace df {
namespace ui {

/**
 * @class WaterfallDisplay
 * @brief GPU-accelerated component for rendering time-domain waterfall/spectrogram visualization
 */
class WaterfallDisplay {
public:
    /**
     * @brief Constructor
     */
    WaterfallDisplay();

    /**
     * @brief Destructor
     */
    ~WaterfallDisplay();

    /**
     * @brief Initialize the waterfall display with OpenGL
     * 
     * @param width Initial width of the display
     * @param height Initial height of the display
     * @param historySize Number of spectrum traces to keep in history
     * @return bool True if initialization was successful
     */
    bool initialize(int width, int height, size_t historySize = 256);

    /**
     * @brief Add a new spectrum trace to the waterfall
     * 
     * @param frequencyData Vector of spectrum amplitude values
     * @param startFreq Start frequency in Hz
     * @param endFreq End frequency in Hz
     */
    void addTrace(const std::vector<float>& frequencyData, double startFreq, double endFreq);

    /**
     * @brief Render the waterfall display
     */
    void render();

    /**
     * @brief Handle window resize
     * 
     * @param width New width
     * @param height New height
     */
    void resize(int width, int height);

    /**
     * @brief Set the range of frequencies to display
     * 
     * @param startFreq Start frequency in Hz
     * @param endFreq End frequency in Hz
     */
    void setFrequencyRange(double startFreq, double endFreq);

    /**
     * @brief Set the amplitude range for color mapping
     * 
     * @param minAmplitude Minimum amplitude in dB
     * @param maxAmplitude Maximum amplitude in dB
     */
    void setAmplitudeRange(float minAmplitude, float maxAmplitude);

    /**
     * @brief Set the time scale of the waterfall
     * 
     * @param timeScale Time scale in seconds for the entire waterfall
     */
    void setTimeScale(float timeScale);

    /**
     * @brief Pause or resume the waterfall scroll
     * 
     * @param paused True to pause, false to resume
     */
    void setPaused(bool paused);

    /**
     * @brief Scroll through history when paused
     * 
     * @param scrollOffset Offset to scroll (positive numbers scroll back in time)
     */
    void scroll(int scrollOffset);

    /**
     * @brief Enable or disable automatic scaling of amplitude
     * 
     * @param enable True to enable automatic amplitude scaling
     */
    void enableAutoScale(bool enable);

    /**
     * @brief Set the color map used for rendering
     * 
     * @param colorMapIndex Index of the color map to use (0: thermal, 1: rainbow, 2: grayscale)
     */
    void setColorMap(int colorMapIndex);

private:
    /**
     * @brief Setup the OpenGL vertex buffer objects and texture
     */
    void setupBuffers();

    /**
     * @brief Update the OpenGL texture with current waterfall data
     */
    void updateTexture();

    /**
     * @brief Generate color maps for different visualization styles
     */
    void generateColorMaps();

    /**
     * @brief Find an appropriate amplitude range for auto scaling
     */
    void findAmplitudeRange();

    /**
     * @brief Normalize a value to range [0, 1]
     * 
     * @param value Value to normalize
     * @param min Minimum value
     * @param max Maximum value
     * @return float Normalized value
     */
    float normalize(float value, float min, float max) const;

    /**
     * @brief Prepare default shaders for waterfall rendering
     * 
     * @return bool True if shader creation was successful
     */
    bool prepareShaders();

    int width_;                         ///< Display width
    int height_;                        ///< Display height
    double startFreq_;                  ///< Start frequency in Hz
    double endFreq_;                    ///< End frequency in Hz
    float minAmplitude_;                ///< Minimum amplitude in dB
    float maxAmplitude_;                ///< Maximum amplitude in dB
    float timeScale_;                   ///< Time scale in seconds
    bool paused_;                       ///< Whether display is paused
    int scrollOffset_;                  ///< Scroll offset when paused
    bool autoScaleEnabled_;             ///< Whether automatic amplitude scaling is enabled
    int colorMapIndex_;                 ///< Index of the current color map
    
    size_t historySize_;                ///< Maximum number of frequency traces to store
    std::deque<std::vector<float>> historyData_; ///< History of frequency data
    
    GLuint vao_;                        ///< Vertex array object
    GLuint vbo_;                        ///< Vertex buffer object
    GLuint texture_;                    ///< Texture for the waterfall
    GLuint colorMapTexture_;            ///< Texture for the color map
    
    std::unique_ptr<ShaderProgram> waterfallShader_; ///< Shader for waterfall rendering
    
    std::vector<uint8_t> textureData_;  ///< Raw texture data
    std::vector<uint8_t> colorMapData_; ///< Color map data
};

} // namespace ui
} // namespace df 