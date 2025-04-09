##
# CMakeLists.txt for examples
##

# Add time sync example
add_executable(time_sync_example time_sync_example.cpp)
target_link_libraries(time_sync_example time_sync)

# Add multilateration example
add_executable(multilateration_example multilateration_example.cpp)
target_link_libraries(multilateration_example tdoa)

# Set C++ standard for all examples
set_property(TARGET time_sync_example multilateration_example PROPERTY CXX_STANDARD 17)

# Install examples
install(TARGETS 
    time_sync_example
    multilateration_example
    DESTINATION bin/examples
) 