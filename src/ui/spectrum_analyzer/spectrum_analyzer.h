#pragma once

#include <memory>
#include <vector>
#include <string>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "spectrum_display.h"
#include "waterfall_display.h"
#include "measurement_tools.h"

namespace df {
namespace ui {

/**
 * @enum DisplayMode
 * @brief Display modes for the spectrum analyzer
 */
enum class DisplayMode {
    SPECTRUM_ONLY,      ///< Show only spectrum display
    WATERFALL_ONLY,     ///< Show only waterfall display
    COMBINED            ///< Show both spectrum and waterfall displays
};

/**
 * @struct SpectrumAnalyzerConfig
 * @brief Configuration for the spectrum analyzer
 */
struct SpectrumAnalyzerConfig {
    int windowWidth = 800;          ///< Window width in pixels
    int windowHeight = 600;         ///< Window height in pixels
    std::string title = "Spectrum Analyzer"; ///< Window title
    DisplayMode displayMode = DisplayMode::COMBINED; ///< Display mode
    double startFreq = 100.0e6;     ///< Start frequency in Hz (default: 100MHz)
    double endFreq = 6.0e9;         ///< End frequency in Hz (default: 6GHz)
    float minAmplitude = -100.0f;   ///< Minimum amplitude in dB
    float maxAmplitude = 0.0f;      ///< Maximum amplitude in dB
    bool autoScale = true;          ///< Whether to auto-scale amplitude
    size_t waterfallHistorySize = 256; ///< Number of traces to keep in waterfall history
};

/**
 * @class SpectrumAnalyzer
 * @brief Main class that integrates spectrum display, waterfall display, and measurement tools
 */
class SpectrumAnalyzer {
public:
    /**
     * @brief Constructor
     * 
     * @param config Configuration for the spectrum analyzer
     */
    explicit SpectrumAnalyzer(const SpectrumAnalyzerConfig& config = SpectrumAnalyzerConfig());

    /**
     * @brief Destructor
     */
    ~SpectrumAnalyzer();

    /**
     * @brief Initialize the spectrum analyzer
     * 
     * @return bool True if initialization was successful
     */
    bool initialize();

    /**
     * @brief Run the main loop
     */
    void run();

    /**
     * @brief Update the spectrum data
     * 
     * @param frequencyData Vector of spectrum amplitude values
     * @param startFreq Start frequency in Hz
     * @param endFreq End frequency in Hz
     */
    void updateData(const std::vector<float>& frequencyData, double startFreq, double endFreq);

    /**
     * @brief Set the display mode
     * 
     * @param mode Display mode
     */
    void setDisplayMode(DisplayMode mode);

    /**
     * @brief Set the frequency range
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
     * @brief Enable or disable automatic scaling of amplitude
     * 
     * @param enable True to enable automatic amplitude scaling
     */
    void enableAutoScale(bool enable);

    /**
     * @brief Enable or disable peak detection
     * 
     * @param enable True to enable peak detection
     */
    void enablePeakDetection(bool enable);

    /**
     * @brief Add a marker at the specified frequency
     * 
     * @param frequency Frequency in Hz
     * @return int Marker ID
     */
    int addMarker(double frequency);

    /**
     * @brief Add a marker at the peak frequency
     * 
     * @return int Marker ID
     */
    int addPeakMarker();

    /**
     * @brief Remove a marker
     * 
     * @param markerId ID of the marker to remove
     * @return bool True if marker was removed
     */
    bool removeMarker(int markerId);

    /**
     * @brief Add a bandwidth measurement
     * 
     * @param centerFreq Center frequency in Hz
     * @param offsetdB Offset from peak in dB (e.g., -3dB, -6dB)
     * @return int Measurement ID
     */
    int addBandwidthMeasurement(double centerFreq, float offsetdB = -3.0f);

    /**
     * @brief Remove a bandwidth measurement
     * 
     * @param measurementId ID of the measurement to remove
     * @return bool True if measurement was removed
     */
    bool removeBandwidthMeasurement(int measurementId);

    /**
     * @brief Get the estimated center frequency of the signal
     * 
     * @return double Center frequency in Hz
     */
    double getSignalCenterFrequency() const;

    /**
     * @brief Get the estimated bandwidth of the signal
     * 
     * @param offsetdB Offset from peak in dB (e.g., -3dB, -6dB)
     * @return double Bandwidth in Hz
     */
    double getSignalBandwidth(float offsetdB = -3.0f) const;

    /**
     * @brief Get the peak frequency and amplitude
     * 
     * @return std::pair<double, float> Pair of peak frequency (Hz) and amplitude (dB)
     */
    std::pair<double, float> getPeakValue() const;

    /**
     * @brief Calculate the integrated power over a frequency range
     * 
     * @param startFreq Start frequency of the range in Hz
     * @param endFreq End frequency of the range in Hz
     * @return float Integrated power in dB
     */
    float calculateIntegratedPower(double startFreq, double endFreq) const;

private:
    /**
     * @brief Set up GLFW window and callbacks
     * 
     * @return bool True if setup was successful
     */
    bool setupWindow();

    /**
     * @brief Initialize OpenGL
     * 
     * @return bool True if initialization was successful
     */
    bool initializeOpenGL();

    /**
     * @brief Render the spectrum analyzer
     */
    void render();

    /**
     * @brief Process input events
     */
    void processInput();

    /**
     * @brief GLFW framebuffer size callback
     * 
     * @param window GLFW window
     * @param width New width
     * @param height New height
     */
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);

    /**
     * @brief GLFW key callback
     * 
     * @param window GLFW window
     * @param key Key
     * @param scancode Scan code
     * @param action Action
     * @param mods Modifiers
     */
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    /**
     * @brief GLFW mouse button callback
     * 
     * @param window GLFW window
     * @param button Button
     * @param action Action
     * @param mods Modifiers
     */
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

    /**
     * @brief GLFW cursor position callback
     * 
     * @param window GLFW window
     * @param xpos X position
     * @param ypos Y position
     */
    static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);

    /**
     * @brief GLFW scroll callback
     * 
     * @param window GLFW window
     * @param xoffset X offset
     * @param yoffset Y offset
     */
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    SpectrumAnalyzerConfig config_;             ///< Configuration
    GLFWwindow* window_;                        ///< GLFW window
    
    std::unique_ptr<SpectrumDisplay> spectrumDisplay_;     ///< Spectrum display component
    std::unique_ptr<WaterfallDisplay> waterfallDisplay_;   ///< Waterfall display component
    std::unique_ptr<MeasurementTools> measurementTools_;   ///< Measurement tools component
    
    std::vector<float> currentSpectrumData_;    ///< Current spectrum data
    double currentStartFreq_;                   ///< Current start frequency in Hz
    double currentEndFreq_;                     ///< Current end frequency in Hz
    
    bool isDragging_;                           ///< Whether user is dragging
    double lastMouseX_;                         ///< Last mouse X position
    double lastMouseY_;                         ///< Last mouse Y position
    
    DisplayMode displayMode_;                   ///< Current display mode
};

} // namespace ui
} // namespace df 