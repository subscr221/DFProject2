#include "spectrum_analyzer.h"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace df {
namespace ui {

// Static member to store the this pointer for callbacks
static SpectrumAnalyzer* g_spectrumAnalyzerInstance = nullptr;

SpectrumAnalyzer::SpectrumAnalyzer(const SpectrumAnalyzerConfig& config)
    : config_(config),
      window_(nullptr),
      currentStartFreq_(config.startFreq),
      currentEndFreq_(config.endFreq),
      isDragging_(false),
      lastMouseX_(0),
      lastMouseY_(0),
      displayMode_(config.displayMode) {
    
    // Store instance for callbacks
    g_spectrumAnalyzerInstance = this;
}

SpectrumAnalyzer::~SpectrumAnalyzer() {
    // Clean up GLFW
    if (window_) {
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
    
    // Reset global instance pointer
    if (g_spectrumAnalyzerInstance == this) {
        g_spectrumAnalyzerInstance = nullptr;
    }
}

bool SpectrumAnalyzer::initialize() {
    // Set up GLFW window
    if (!setupWindow()) {
        std::cerr << "Failed to set up GLFW window" << std::endl;
        return false;
    }
    
    // Initialize OpenGL
    if (!initializeOpenGL()) {
        std::cerr << "Failed to initialize OpenGL" << std::endl;
        return false;
    }
    
    // Create and initialize spectrum display
    spectrumDisplay_ = std::make_unique<SpectrumDisplay>();
    if (!spectrumDisplay_->initialize(config_.windowWidth, config_.windowHeight)) {
        std::cerr << "Failed to initialize spectrum display" << std::endl;
        return false;
    }
    
    // Create and initialize waterfall display
    waterfallDisplay_ = std::make_unique<WaterfallDisplay>();
    if (!waterfallDisplay_->initialize(config_.windowWidth, 
                                       config_.windowHeight / 2, 
                                       config_.waterfallHistorySize)) {
        std::cerr << "Failed to initialize waterfall display" << std::endl;
        return false;
    }
    
    // Create and initialize measurement tools
    measurementTools_ = std::make_unique<MeasurementTools>();
    if (!measurementTools_->initialize(config_.windowWidth, config_.windowHeight)) {
        std::cerr << "Failed to initialize measurement tools" << std::endl;
        return false;
    }
    
    // Set frequency range
    spectrumDisplay_->setFrequencyRange(config_.startFreq, config_.endFreq);
    waterfallDisplay_->setFrequencyRange(config_.startFreq, config_.endFreq);
    
    // Set amplitude range
    if (!config_.autoScale) {
        spectrumDisplay_->setAmplitudeRange(config_.minAmplitude, config_.maxAmplitude);
        waterfallDisplay_->setAmplitudeRange(config_.minAmplitude, config_.maxAmplitude);
    }
    
    // Set auto-scale mode
    spectrumDisplay_->enableAutoScale(config_.autoScale);
    waterfallDisplay_->enableAutoScale(config_.autoScale);
    
    return true;
}

bool SpectrumAnalyzer::setupWindow() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }
    
    // Set OpenGL version
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    // Create window
    window_ = glfwCreateWindow(config_.windowWidth, config_.windowHeight, 
                              config_.title.c_str(), nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }
    
    // Make OpenGL context current
    glfwMakeContextCurrent(window_);
    
    // Set callbacks
    glfwSetFramebufferSizeCallback(window_, framebufferSizeCallback);
    glfwSetKeyCallback(window_, keyCallback);
    glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    glfwSetCursorPosCallback(window_, cursorPositionCallback);
    glfwSetScrollCallback(window_, scrollCallback);
    
    // Store user pointer for callbacks
    glfwSetWindowUserPointer(window_, this);
    
    // Enable vsync
    glfwSwapInterval(1);
    
    return true;
}

bool SpectrumAnalyzer::initializeOpenGL() {
    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return false;
    }
    
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    
    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Set clear color
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    
    return true;
}

void SpectrumAnalyzer::run() {
    // Main loop
    while (!glfwWindowShouldClose(window_)) {
        // Process input
        processInput();
        
        // Render
        render();
        
        // Swap buffers
        glfwSwapBuffers(window_);
        
        // Poll events
        glfwPollEvents();
    }
}

void SpectrumAnalyzer::render() {
    // Clear the screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    int displayHeight = config_.windowHeight;
    
    // Render based on display mode
    switch (displayMode_) {
        case DisplayMode::SPECTRUM_ONLY:
            // Render spectrum only
            glViewport(0, 0, config_.windowWidth, displayHeight);
            spectrumDisplay_->render();
            
            // Render measurement tools on top of spectrum
            measurementTools_->render(currentSpectrumData_, currentStartFreq_, currentEndFreq_,
                                     config_.minAmplitude, config_.maxAmplitude);
            break;
            
        case DisplayMode::WATERFALL_ONLY:
            // Render waterfall only
            glViewport(0, 0, config_.windowWidth, displayHeight);
            waterfallDisplay_->render();
            break;
            
        case DisplayMode::COMBINED:
            // Render spectrum in top half
            glViewport(0, displayHeight / 2, config_.windowWidth, displayHeight / 2);
            spectrumDisplay_->render();
            
            // Render measurement tools on top of spectrum
            measurementTools_->render(currentSpectrumData_, currentStartFreq_, currentEndFreq_,
                                     config_.minAmplitude, config_.maxAmplitude);
            
            // Render waterfall in bottom half
            glViewport(0, 0, config_.windowWidth, displayHeight / 2);
            waterfallDisplay_->render();
            break;
    }
}

void SpectrumAnalyzer::processInput() {
    // Close window when Escape is pressed
    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window_, true);
    }
}

void SpectrumAnalyzer::updateData(const std::vector<float>& frequencyData, double startFreq, double endFreq) {
    // Store current data
    currentSpectrumData_ = frequencyData;
    currentStartFreq_ = startFreq;
    currentEndFreq_ = endFreq;
    
    // Update spectrum display
    spectrumDisplay_->updateData(frequencyData, startFreq, endFreq);
    
    // Update waterfall display
    waterfallDisplay_->addTrace(frequencyData, startFreq, endFreq);
}

void SpectrumAnalyzer::setDisplayMode(DisplayMode mode) {
    displayMode_ = mode;
}

void SpectrumAnalyzer::setFrequencyRange(double startFreq, double endFreq) {
    if (startFreq >= endFreq) {
        std::cerr << "Warning: Invalid frequency range (start >= end)" << std::endl;
        return;
    }
    
    currentStartFreq_ = startFreq;
    currentEndFreq_ = endFreq;
    
    spectrumDisplay_->setFrequencyRange(startFreq, endFreq);
    waterfallDisplay_->setFrequencyRange(startFreq, endFreq);
}

void SpectrumAnalyzer::setAmplitudeRange(float minAmplitude, float maxAmplitude) {
    if (minAmplitude >= maxAmplitude) {
        std::cerr << "Warning: Invalid amplitude range (min >= max)" << std::endl;
        return;
    }
    
    config_.minAmplitude = minAmplitude;
    config_.maxAmplitude = maxAmplitude;
    
    spectrumDisplay_->setAmplitudeRange(minAmplitude, maxAmplitude);
    waterfallDisplay_->setAmplitudeRange(minAmplitude, maxAmplitude);
    
    // Disable auto-scaling
    config_.autoScale = false;
    spectrumDisplay_->enableAutoScale(false);
    waterfallDisplay_->enableAutoScale(false);
}

void SpectrumAnalyzer::enableAutoScale(bool enable) {
    config_.autoScale = enable;
    spectrumDisplay_->enableAutoScale(enable);
    waterfallDisplay_->enableAutoScale(enable);
}

void SpectrumAnalyzer::enablePeakDetection(bool enable) {
    spectrumDisplay_->enablePeakDetection(enable);
}

int SpectrumAnalyzer::addMarker(double frequency) {
    if (frequency < currentStartFreq_ || frequency > currentEndFreq_) {
        std::cerr << "Warning: Marker frequency out of range" << std::endl;
        return -1;
    }
    
    // Find amplitude at the given frequency
    float amplitude = 0.0f;
    if (!currentSpectrumData_.empty()) {
        // Calculate index into spectrum data
        double freqRange = currentEndFreq_ - currentStartFreq_;
        double normFreq = (frequency - currentStartFreq_) / freqRange;
        size_t index = static_cast<size_t>(normFreq * (currentSpectrumData_.size() - 1));
        
        // Clamp to valid range
        index = std::min(index, currentSpectrumData_.size() - 1);
        
        amplitude = currentSpectrumData_[index];
    }
    
    // Add marker
    return measurementTools_->addMarker(frequency, amplitude);
}

int SpectrumAnalyzer::addPeakMarker() {
    if (currentSpectrumData_.empty()) {
        std::cerr << "Warning: No spectrum data available" << std::endl;
        return -1;
    }
    
    // Add marker at peak
    return measurementTools_->addPeakMarker(currentSpectrumData_, currentStartFreq_, currentEndFreq_);
}

bool SpectrumAnalyzer::removeMarker(int markerId) {
    return measurementTools_->removeMarker(markerId);
}

int SpectrumAnalyzer::addBandwidthMeasurement(double centerFreq, float offsetdB) {
    if (currentSpectrumData_.empty()) {
        std::cerr << "Warning: No spectrum data available" << std::endl;
        return -1;
    }
    
    if (centerFreq < currentStartFreq_ || centerFreq > currentEndFreq_) {
        std::cerr << "Warning: Center frequency out of range" << std::endl;
        return -1;
    }
    
    // Calculate bandwidth
    double bandwidth = measurementTools_->calculateBandwidth(currentSpectrumData_, 
                                                           currentStartFreq_, 
                                                           currentEndFreq_,
                                                           centerFreq, 
                                                           offsetdB);
    
    // Calculate start and end frequencies
    double startFreq = centerFreq - bandwidth / 2.0;
    double endFreq = centerFreq + bandwidth / 2.0;
    
    // Clamp to valid range
    startFreq = std::max(startFreq, currentStartFreq_);
    endFreq = std::min(endFreq, currentEndFreq_);
    
    // Add bandwidth measurement
    return measurementTools_->addBandwidthMeasurement(startFreq, endFreq, offsetdB);
}

bool SpectrumAnalyzer::removeBandwidthMeasurement(int measurementId) {
    return measurementTools_->removeBandwidthMeasurement(measurementId);
}

double SpectrumAnalyzer::getSignalCenterFrequency() const {
    if (currentSpectrumData_.empty()) {
        return 0.0;
    }
    
    // Find the peak
    auto peakIt = std::max_element(currentSpectrumData_.begin(), currentSpectrumData_.end());
    size_t peakIndex = std::distance(currentSpectrumData_.begin(), peakIt);
    
    // Calculate center frequency
    double freqStep = (currentEndFreq_ - currentStartFreq_) / (currentSpectrumData_.size() - 1);
    return currentStartFreq_ + peakIndex * freqStep;
}

double SpectrumAnalyzer::getSignalBandwidth(float offsetdB) const {
    if (currentSpectrumData_.empty()) {
        return 0.0;
    }
    
    // Get center frequency
    double centerFreq = getSignalCenterFrequency();
    
    // Calculate bandwidth
    return measurementTools_->calculateBandwidth(currentSpectrumData_, 
                                               currentStartFreq_, 
                                               currentEndFreq_,
                                               centerFreq, 
                                               offsetdB);
}

std::pair<double, float> SpectrumAnalyzer::getPeakValue() const {
    if (currentSpectrumData_.empty()) {
        return {0.0, 0.0f};
    }
    
    // Find the peak
    auto peakIt = std::max_element(currentSpectrumData_.begin(), currentSpectrumData_.end());
    size_t peakIndex = std::distance(currentSpectrumData_.begin(), peakIt);
    
    // Calculate center frequency
    double freqStep = (currentEndFreq_ - currentStartFreq_) / (currentSpectrumData_.size() - 1);
    double peakFreq = currentStartFreq_ + peakIndex * freqStep;
    
    return {peakFreq, *peakIt};
}

float SpectrumAnalyzer::calculateIntegratedPower(double startFreq, double endFreq) const {
    if (currentSpectrumData_.empty()) {
        return 0.0f;
    }
    
    return measurementTools_->calculateIntegratedPower(currentSpectrumData_,
                                                     currentStartFreq_,
                                                     currentEndFreq_,
                                                     startFreq,
                                                     endFreq);
}

// Static callback functions

void SpectrumAnalyzer::framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    auto* analyzer = static_cast<SpectrumAnalyzer*>(glfwGetWindowUserPointer(window));
    if (!analyzer) return;
    
    // Update configuration
    analyzer->config_.windowWidth = width;
    analyzer->config_.windowHeight = height;
    
    // Resize components
    if (analyzer->spectrumDisplay_) {
        analyzer->spectrumDisplay_->resize(width, height);
    }
    
    if (analyzer->waterfallDisplay_) {
        analyzer->waterfallDisplay_->resize(width, height / 2);
    }
    
    if (analyzer->measurementTools_) {
        analyzer->measurementTools_->resize(width, height);
    }
}

void SpectrumAnalyzer::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto* analyzer = static_cast<SpectrumAnalyzer*>(glfwGetWindowUserPointer(window));
    if (!analyzer || action != GLFW_PRESS) return;
    
    switch (key) {
        case GLFW_KEY_1:
            // Switch to spectrum only mode
            analyzer->setDisplayMode(DisplayMode::SPECTRUM_ONLY);
            break;
            
        case GLFW_KEY_2:
            // Switch to waterfall only mode
            analyzer->setDisplayMode(DisplayMode::WATERFALL_ONLY);
            break;
            
        case GLFW_KEY_3:
            // Switch to combined mode
            analyzer->setDisplayMode(DisplayMode::COMBINED);
            break;
            
        case GLFW_KEY_A:
            // Toggle auto-scale
            analyzer->enableAutoScale(!analyzer->config_.autoScale);
            break;
            
        case GLFW_KEY_P:
            // Toggle peak detection
            if (analyzer->spectrumDisplay_) {
                bool enable = !analyzer->spectrumDisplay_->getPeakValue().second;
                analyzer->enablePeakDetection(enable);
            }
            break;
            
        case GLFW_KEY_M:
            // Add marker at peak
            analyzer->addPeakMarker();
            break;
            
        case GLFW_KEY_B:
            // Add bandwidth measurement at center frequency
            analyzer->addBandwidthMeasurement(analyzer->getSignalCenterFrequency());
            break;
    }
}

void SpectrumAnalyzer::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto* analyzer = static_cast<SpectrumAnalyzer*>(glfwGetWindowUserPointer(window));
    if (!analyzer) return;
    
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            // Start dragging
            analyzer->isDragging_ = true;
            glfwGetCursorPos(window, &analyzer->lastMouseX_, &analyzer->lastMouseY_);
        } else if (action == GLFW_RELEASE) {
            // Stop dragging
            analyzer->isDragging_ = false;
        }
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        // Add marker at clicked frequency
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        
        double normX = xpos / analyzer->config_.windowWidth;
        double freq = analyzer->currentStartFreq_ + normX * (analyzer->currentEndFreq_ - analyzer->currentStartFreq_);
        
        analyzer->addMarker(freq);
    }
}

void SpectrumAnalyzer::cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
    auto* analyzer = static_cast<SpectrumAnalyzer*>(glfwGetWindowUserPointer(window));
    if (!analyzer || !analyzer->isDragging_) return;
    
    // Calculate delta movement
    double deltaX = xpos - analyzer->lastMouseX_;
    
    // Pan frequency range based on mouse movement
    if (deltaX != 0 && analyzer->spectrumDisplay_) {
        double panFactor = -deltaX / analyzer->config_.windowWidth;
        analyzer->spectrumDisplay_->pan(panFactor);
        
        // Update frequency range
        double freqRange = analyzer->currentEndFreq_ - analyzer->currentStartFreq_;
        double delta = panFactor * freqRange;
        analyzer->currentStartFreq_ += delta;
        analyzer->currentEndFreq_ += delta;
        
        // Update waterfall display
        if (analyzer->waterfallDisplay_) {
            analyzer->waterfallDisplay_->setFrequencyRange(analyzer->currentStartFreq_, analyzer->currentEndFreq_);
        }
    }
    
    // Update last mouse position
    analyzer->lastMouseX_ = xpos;
    analyzer->lastMouseY_ = ypos;
}

void SpectrumAnalyzer::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    auto* analyzer = static_cast<SpectrumAnalyzer*>(glfwGetWindowUserPointer(window));
    if (!analyzer || !analyzer->spectrumDisplay_) return;
    
    // Zoom in/out based on scroll
    if (yoffset != 0) {
        // Get cursor position to zoom around that point
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        
        // Calculate center frequency for zoom
        double normX = xpos / analyzer->config_.windowWidth;
        double centerFreq = analyzer->currentStartFreq_ + 
                           normX * (analyzer->currentEndFreq_ - analyzer->currentStartFreq_);
        
        // Calculate zoom factor
        float zoomFactor = (yoffset > 0) ? 1.1f : 0.9f;
        
        // Apply zoom
        analyzer->spectrumDisplay_->zoom(zoomFactor, centerFreq);
        
        // Update current frequency range
        double newRange = (analyzer->currentEndFreq_ - analyzer->currentStartFreq_) / zoomFactor;
        double newStartFreq = centerFreq - normX * newRange;
        double newEndFreq = newStartFreq + newRange;
        
        analyzer->currentStartFreq_ = newStartFreq;
        analyzer->currentEndFreq_ = newEndFreq;
        
        // Update waterfall display
        if (analyzer->waterfallDisplay_) {
            analyzer->waterfallDisplay_->setFrequencyRange(newStartFreq, newEndFreq);
        }
    }
}

} // namespace ui
} // namespace df 