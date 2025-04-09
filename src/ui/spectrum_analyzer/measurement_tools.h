#pragma once

#include <vector>
#include <memory>
#include <string>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "shader_program.h"

namespace df {
namespace ui {

/**
 * @struct Marker
 * @brief Represents a marker for frequency/amplitude measurement
 */
struct Marker {
    double frequency;   ///< Frequency in Hz
    float amplitude;    ///< Amplitude in dB
    bool active;        ///< Whether the marker is active
    bool isDelta;       ///< Whether this is a delta marker
    int referenceId;    ///< ID of the reference marker for delta markers
    int id;             ///< Marker ID
    glm::vec4 color;    ///< Marker color
};

/**
 * @struct BandwidthMeasurement
 * @brief Represents a bandwidth measurement between two frequency points
 */
struct BandwidthMeasurement {
    double startFreq;   ///< Start frequency in Hz
    double endFreq;     ///< End frequency in Hz
    float referencedB;  ///< Reference amplitude in dB
    float offsetdB;     ///< Offset from reference in dB (e.g., -3dB, -6dB)
    bool active;        ///< Whether the measurement is active
    int id;             ///< Measurement ID
    glm::vec4 color;    ///< Measurement color
};

/**
 * @class MeasurementTools
 * @brief Provides tools for signal measurement such as markers and bandwidth calculation
 */
class MeasurementTools {
public:
    /**
     * @brief Constructor
     */
    MeasurementTools();

    /**
     * @brief Destructor
     */
    ~MeasurementTools();

    /**
     * @brief Initialize the measurement tools
     * 
     * @param width Initial width of the display
     * @param height Initial height of the display
     * @return bool True if initialization was successful
     */
    bool initialize(int width, int height);

    /**
     * @brief Render the measurement tools
     * 
     * @param frequencyData Current spectrum data
     * @param startFreq Start frequency in Hz
     * @param endFreq End frequency in Hz
     * @param minAmplitude Minimum amplitude in dB
     * @param maxAmplitude Maximum amplitude in dB
     */
    void render(const std::vector<float>& frequencyData, double startFreq, double endFreq,
                float minAmplitude, float maxAmplitude);

    /**
     * @brief Handle window resize
     * 
     * @param width New width
     * @param height New height
     */
    void resize(int width, int height);

    /**
     * @brief Add a new marker
     * 
     * @param frequency Initial frequency in Hz
     * @param amplitude Initial amplitude in dB
     * @param isDelta Whether this is a delta marker
     * @param referenceId ID of the reference marker for delta markers
     * @return int Marker ID
     */
    int addMarker(double frequency, float amplitude, bool isDelta = false, int referenceId = -1);

    /**
     * @brief Remove a marker
     * 
     * @param markerId ID of the marker to remove
     * @return bool True if marker was removed
     */
    bool removeMarker(int markerId);

    /**
     * @brief Move a marker to a specific frequency
     * 
     * @param markerId ID of the marker to move
     * @param frequency New frequency in Hz
     * @return bool True if marker was moved
     */
    bool moveMarker(int markerId, double frequency);

    /**
     * @brief Add a bandwidth measurement
     * 
     * @param startFreq Start frequency in Hz
     * @param endFreq End frequency in Hz
     * @param offsetdB Offset from peak in dB (e.g., -3dB, -6dB)
     * @return int Measurement ID
     */
    int addBandwidthMeasurement(double startFreq, double endFreq, float offsetdB = -3.0f);

    /**
     * @brief Remove a bandwidth measurement
     * 
     * @param measurementId ID of the measurement to remove
     * @return bool True if measurement was removed
     */
    bool removeBandwidthMeasurement(int measurementId);

    /**
     * @brief Find and add marker at the peak frequency
     * 
     * @param frequencyData Current spectrum data
     * @param startFreq Start frequency in Hz
     * @param endFreq End frequency in Hz
     * @return int Marker ID
     */
    int addPeakMarker(const std::vector<float>& frequencyData, double startFreq, double endFreq);

    /**
     * @brief Update marker amplitude values based on current spectrum data
     * 
     * @param frequencyData Current spectrum data
     * @param startFreq Start frequency in Hz
     * @param endFreq End frequency in Hz
     */
    void updateMarkerValues(const std::vector<float>& frequencyData, double startFreq, double endFreq);

    /**
     * @brief Calculate bandwidth based on offset from a reference level
     * 
     * @param frequencyData Current spectrum data
     * @param startFreq Start frequency in Hz
     * @param endFreq End frequency in Hz
     * @param centerFreq Center frequency for bandwidth calculation
     * @param offsetdB Offset in dB from the peak (e.g., -3dB, -6dB)
     * @return double Calculated bandwidth in Hz
     */
    double calculateBandwidth(const std::vector<float>& frequencyData, double startFreq, double endFreq,
                              double centerFreq, float offsetdB);

    /**
     * @brief Calculate integrated power over a frequency range
     * 
     * @param frequencyData Current spectrum data
     * @param startFreq Start frequency of the entire spectrum in Hz
     * @param endFreq End frequency of the entire spectrum in Hz
     * @param rangeStartFreq Start frequency of the range for integration in Hz
     * @param rangeEndFreq End frequency of the range for integration in Hz
     * @return float Integrated power in dB
     */
    float calculateIntegratedPower(const std::vector<float>& frequencyData, double startFreq, double endFreq,
                                  double rangeStartFreq, double rangeEndFreq);

    /**
     * @brief Get the list of all markers
     * 
     * @return const std::vector<Marker>& Reference to markers vector
     */
    const std::vector<Marker>& getMarkers() const { return markers_; }

    /**
     * @brief Get the list of all bandwidth measurements
     * 
     * @return const std::vector<BandwidthMeasurement>& Reference to measurements vector
     */
    const std::vector<BandwidthMeasurement>& getBandwidthMeasurements() const { return bwMeasurements_; }

private:
    /**
     * @brief Setup the OpenGL vertex buffer objects and vertex array object
     */
    void setupBuffers();

    /**
     * @brief Draw a marker on the spectrum
     * 
     * @param marker Marker to draw
     * @param screenX X position in normalized coordinates [0,1]
     * @param screenY Y position in normalized coordinates [0,1]
     */
    void drawMarker(const Marker& marker, float screenX, float screenY);

    /**
     * @brief Draw a bandwidth measurement on the spectrum
     * 
     * @param bwMeasurement Bandwidth measurement to draw
     * @param startX Start X position in normalized coordinates [0,1]
     * @param endX End X position in normalized coordinates [0,1]
     * @param levelY Y position of the reference level in normalized coordinates [0,1]
     * @param offsetY Y position of the offset level in normalized coordinates [0,1]
     */
    void drawBandwidthMeasurement(const BandwidthMeasurement& bwMeasurement,
                                 float startX, float endX, float levelY, float offsetY);

    /**
     * @brief Interpolate amplitude value at a specific frequency
     * 
     * @param frequencyData Spectrum data
     * @param startFreq Start frequency in Hz
     * @param endFreq End frequency in Hz
     * @param frequency Target frequency in Hz
     * @return float Interpolated amplitude value in dB
     */
    float interpolateAmplitude(const std::vector<float>& frequencyData,
                              double startFreq, double endFreq, double frequency);

    /**
     * @brief Prepare shaders for marker and measurement rendering
     * 
     * @return bool True if shader creation was successful
     */
    bool prepareShaders();

    int width_;                    ///< Display width
    int height_;                   ///< Display height
    GLuint markerVao_;             ///< Vertex array object for markers
    GLuint markerVbo_;             ///< Vertex buffer object for markers
    GLuint bandwidthVao_;          ///< Vertex array object for bandwidth measurements
    GLuint bandwidthVbo_;          ///< Vertex buffer object for bandwidth measurements
    
    std::vector<Marker> markers_;  ///< List of active markers
    std::vector<BandwidthMeasurement> bwMeasurements_; ///< List of active bandwidth measurements
    
    std::unique_ptr<ShaderProgram> measurementShader_; ///< Shader for rendering markers and measurements
    
    int nextMarkerId_;             ///< Next ID to assign to a marker
    int nextMeasurementId_;        ///< Next ID to assign to a measurement
};

} // namespace ui
} // namespace df 