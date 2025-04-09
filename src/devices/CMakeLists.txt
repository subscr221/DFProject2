# CMakeLists.txt for devices directory

# Add subdirectories
add_subdirectory(signalhound)

# Add source files
set(DEVICES_SOURCES
    signal_source_factory.cpp
)

# Add the devices library
add_library(tdoa_devices ${DEVICES_SOURCES})

# Set include directories
target_include_directories(tdoa_devices PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link dependencies
target_link_libraries(tdoa_devices
    bb60c_device
)

# Add the factory test executable
add_executable(signal_source_factory_test signal_source_factory_test.cpp)

# Link the factory test with the devices library
target_link_libraries(signal_source_factory_test
    tdoa_devices
) 