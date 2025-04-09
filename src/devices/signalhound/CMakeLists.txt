# CMakeLists.txt for SignalHound BB60C integration

# Add external dependencies
list(APPEND CMAKE_PREFIX_PATH "C:/DEVTOOLS/vcpkg/packages/nlohmann-json_x64-windows/share/nlohmann_json")
find_package(nlohmann_json CONFIG REQUIRED)

# Define SignalHound paths
set(SIGNALHOUND_ROOT "${CMAKE_SOURCE_DIR}/external/signalhound")
set(SIGNALHOUND_LIB_NAME "bb_api")

if(WIN32)
    set(SIGNALHOUND_LIB_PATH "${SIGNALHOUND_ROOT}/lib/${SIGNALHOUND_LIB_NAME}.lib")
else()
    set(SIGNALHOUND_LIB_PATH "${SIGNALHOUND_ROOT}/lib/lib${SIGNALHOUND_LIB_NAME}.so")
endif()

# Verify SignalHound library exists
if(NOT EXISTS ${SIGNALHOUND_LIB_PATH})
    message(FATAL_ERROR "SignalHound library not found at ${SIGNALHOUND_LIB_PATH}. Please ensure the library is installed in the correct location.")
endif()

# Add the source files
set(BB60C_SOURCES
    bb60c_device.cpp
    bb60c_abstract_device.cpp
)

# Add the device library
add_library(bb60c_device ${BB60C_SOURCES})

# Set include directories
target_include_directories(bb60c_device PUBLIC
    ${SIGNALHOUND_ROOT}/wrapper
    ${SIGNALHOUND_ROOT}/include  # SignalHound BB60C API headers
    ${CMAKE_SOURCE_DIR}/src/devices  # For signal_source_device.h
)

# Link libraries
target_link_libraries(bb60c_device
    ${SIGNALHOUND_LIB_PATH}
    nlohmann_json::nlohmann_json
)

# Add the basic test executable
add_executable(bb60c_device_test bb60c_device_test.cpp)

# Link the basic test with the device library
target_link_libraries(bb60c_device_test
    bb60c_device
)

# Add the abstract device test executable
add_executable(bb60c_abstract_device_test bb60c_abstract_device_test.cpp)

# Link the abstract test with the device library
target_link_libraries(bb60c_abstract_device_test
    bb60c_device
    nlohmann_json::nlohmann_json
) 