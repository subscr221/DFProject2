##
# CMakeLists.txt for GPS devices module
##

# Check for GPSD
find_library(GPSD_LIBRARY NAMES gps)
find_path(GPSD_INCLUDE_DIR NAMES gps.h PATH_SUFFIXES libgps)

# List source files
set(GPS_SOURCES
    gps_device.h
    gps_device_factory.cpp
    gpsd_device.h
)

# Check if we can compile GPSD implementation
if(GPSD_LIBRARY AND GPSD_INCLUDE_DIR)
    message(STATUS "Found GPSD: ${GPSD_LIBRARY}")
    list(APPEND GPS_SOURCES gpsd_device.cpp)
    add_definitions(-DHAVE_GPSD)
else()
    message(STATUS "GPSD not found, GPSD support will be disabled")
endif()

# Add library
add_library(gps_devices STATIC ${GPS_SOURCES})

# Include directories
target_include_directories(gps_devices PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Add GPSD includes if found
if(GPSD_LIBRARY AND GPSD_INCLUDE_DIR)
    target_include_directories(gps_devices PRIVATE ${GPSD_INCLUDE_DIR})
    target_link_libraries(gps_devices ${GPSD_LIBRARY})
endif()

# Set C++ standard
target_compile_features(gps_devices PRIVATE cxx_std_17)

# Add installation targets
install(TARGETS gps_devices
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES
    gps_device.h
    DESTINATION include/tdoa/devices/gps
)