# TDOA Direction Finding System - Project Brief

## Overview
The TDOA Direction Finding System is a high-performance multi-node signal geolocation system that utilizes CUDA acceleration for real-time signal processing and location determination. The system employs a dual-receiver architecture combining a Search Receiver (SignalHound BB60C) and a TDOA Receiver (SignalHound SM200C) to provide comprehensive signal detection and precise geolocation capabilities.

## Core Requirements

### Hardware Requirements
1. Dual-Receiver Architecture
   - Search Receiver (SignalHound BB60C)
     - Frequency Range: 9kHz - 6GHz
     - Max Sweep Rate: 24GHz/s
     - Instant Bandwidth: 27MHz
   - TDOA Receiver (SignalHound SM200C)
     - Frequency Range: 100MHz - 6GHz
     - Instant Bandwidth: 160MHz
     - Timing Accuracy: <10ns

### Software Requirements
1. CUDA-Accelerated Processing
   - Real-time FFT processing
   - Multi-node TDOA calculations
   - Parallel signal analysis
   - GPU memory optimization

2. Signal Processing Capabilities
   - Continuous spectrum monitoring
   - Multi-signal processing
   - Real-time signal analysis
   - Automated handoff between receivers

3. Performance Targets
   - Search Sensitivity: -158 dBm/Hz
   - TDOA Sensitivity: -160 dBm/Hz
   - Geolocation CEP: 50m at 20km
   - Timing Accuracy: 0.01ns
   - Phase Accuracy: 0.1 degrees

## Project Goals
1. Create a high-performance signal geolocation system for military and commercial applications
2. Implement efficient CUDA-accelerated signal processing algorithms
3. Achieve precise geolocation using distributed receiver nodes
4. Provide real-time signal detection and analysis capabilities
5. Support various deployment scenarios (fixed, mobile, portable)

## Success Criteria
1. System achieves specified sensitivity and accuracy targets
2. Real-time processing capabilities demonstrated
3. Successful integration of dual-receiver architecture
4. CUDA acceleration provides measurable performance improvements
5. System operates reliably in various deployment scenarios 