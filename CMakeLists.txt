##
# Main CMakeLists.txt for DF Project
##

cmake_minimum_required(VERSION 3.12)
project(df_project VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Enable testing
enable_testing()

# Add subdirectories
add_subdirectory(src/devices)
add_subdirectory(src/time_sync)
add_subdirectory(src/tdoa)
add_subdirectory(examples)

# Install targets
install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/
        DESTINATION include/df_project
        FILES_MATCHING PATTERN "*.h"
) 