cmake_minimum_required(VERSION 3.15)
project(TDOA_Direction_Finder LANGUAGES CXX CUDA)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 14)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# Find required packages
find_package(CUDA REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem thread)

# Set CUDA architecture
set(CMAKE_CUDA_ARCHITECTURES 75)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib
    ${CUDA_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

# Add subdirectories
add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(lib)

# Set build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# CUDA flags
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -Xcompiler -Wall")

# Define main executable
add_executable(tdoa_finder 
    src/main.cpp
    src/receiver/dual_receiver.cu
    src/processing/signal_processor.cu
    src/analysis/tdoa_analyzer.cu
)

# Link libraries
target_link_libraries(tdoa_finder
    ${CUDA_LIBRARIES}
    ${OpenCV_LIBS}
    ${Boost_LIBRARIES}
)

# Testing
enable_testing()
add_test(NAME unit_tests COMMAND unit_tests)
add_test(NAME integration_tests COMMAND integration_tests) 