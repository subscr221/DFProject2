#pragma once

#include <vector>
#include <memory>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "shader_program.h"

namespace df {
namespace ui {

/**
 * @class SpectrumDisplay
 * @brief GPU-accelerated component for rendering frequency spectrum data
 */
class SpectrumDisplay {
public:
    /**
     * @brief Constructor
     */
    SpectrumDisplay();

    /**
     * @brief Destructor
     */
    ~SpectrumDisplay();

    /**
     * @brief Initialize the spectrum display with OpenGL
     * 
     * @param width Initial width of the display
     * @param height Initial height of the display
     * @return bool True if initialization was successful
     */
    bool initialize(int width, int height);

    /**
     * @brief Update the spectrum data
     * 
     * @param frequencyData Vector of spectrum amplitude values
     * @param startFreq Start frequency in Hz
     * @param endFreq End frequency in Hz
     */
    void updateData(const std::vector<float>& frequencyData, double startFreq, double endFreq);

    /**
     * @brief Render the spectrum display
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
     * @brief Set the amplitude range
     * 
     * @param minAmplitude Minimum amplitude in dB
     * @param maxAmplitude Maximum amplitude in dB
     */
    void setAmplitudeRange(float minAmplitude, float maxAmplitude);

    /**
     * @brief Enable or disable peak detection
     * 
     * @param enable True to enable peak detection
     */
    void enablePeakDetection(bool enable);

    /**
     * @brief Enable or disable automatic scaling of amplitude
     * 
     * @param enable True to enable automatic amplitude scaling
     */
    void enableAutoScale(bool enable);

    /**
     * @brief Get the peak frequency and amplitude
     * 
     * @return std::pair<double, float> Pair of peak frequency (Hz) and amplitude (dB)
     */
    std::pair<double, float> getPeakValue() const;

    /**
     * @brief Pan the view horizontally
     * 
     * @param deltaX Pan amount in frequency units
     */
    void pan(double deltaX);

    /**
     * @brief Zoom in or out
     * 
     * @param factor Zoom factor (> 1 for zoom in, < 1 for zoom out)
     * @param centerFreq Center frequency for zoom operation
     */
    void zoom(float factor, double centerFreq);

private:
    /**
     * @brief Setup the OpenGL vertex buffer objects and vertex array object
     */
    void setupBuffers();

    /**
     * @brief Update the grid lines for frequency and amplitude
     */
    void updateGridLines();

    /**
     * @brief Draw grid lines and axis labels
     */
    void drawGrid();

    /**
     * @brief Prepare default shaders for spectrum rendering
     * 
     * @return bool True if shader creation was successful
     */
    bool prepareShaders();

    /**
     * @brief Find peaks in the spectrum data
     */
    void findPeaks();

    int width_;                         ///< Display width
    int height_;                        ///< Display height
    double startFreq_;                  ///< Start frequency in Hz
    double endFreq_;                    ///< End frequency in Hz
    float minAmplitude_;                ///< Minimum amplitude in dB
    float maxAmplitude_;                ///< Maximum amplitude in dB
    bool peakDetectionEnabled_;         ///< Whether peak detection is enabled
    bool autoScaleEnabled_;             ///< Whether automatic amplitude scaling is enabled
    
    std::vector<float> frequencyData_;  ///< Current spectrum data
    std::vector<std::pair<double, float>> peaks_; ///< Detected peaks (frequency, amplitude)
    
    GLuint vao_;                        ///< Vertex array object
    GLuint vbo_;                        ///< Vertex buffer object
    GLuint gridVao_;                    ///< Grid vertex array object
    GLuint gridVbo_;                    ///< Grid vertex buffer object
    
    std::unique_ptr<ShaderProgram> spectrumShader_; ///< Shader for spectrum rendering
    std::unique_ptr<ShaderProgram> gridShader_;     ///< Shader for grid rendering
};

} // namespace ui
} // namespace df 