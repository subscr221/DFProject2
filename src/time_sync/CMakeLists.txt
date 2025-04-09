##
# CMakeLists.txt for Time Synchronization module
##

# List source files
set(TIME_SYNC_SOURCES
    time_sync_interface.h
    time_sync_factory.cpp
    gps_time_sync.h
    gps_time_sync.cpp
    kalman_filter.h
    allan_variance.h
    temperature_compensation.h
    time_reference_protocol.h
    time_reference_protocol.cpp
    udp_transport.h
    udp_transport.cpp
)

# Add library
add_library(time_sync STATIC ${TIME_SYNC_SOURCES})

# Include directories
target_include_directories(time_sync PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Link libraries
target_link_libraries(time_sync
    devices
    pthread
    rt
)

# Set C++ standard
target_compile_features(time_sync PRIVATE cxx_std_17)

# Set defines
target_compile_definitions(time_sync PRIVATE
    DEBUG=$<CONFIG:Debug>
)

# Add installation targets
install(TARGETS time_sync
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES
    time_sync_interface.h
    gps_time_sync.h
    kalman_filter.h
    allan_variance.h
    temperature_compensation.h
    time_reference_protocol.h
    udp_transport.h
    DESTINATION include/tdoa/time_sync
)