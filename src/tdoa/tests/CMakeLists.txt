##
# CMakeLists.txt for TDOA tests
##

# Add test executables
add_executable(test_cross_correlation test_cross_correlation.cpp)
add_executable(test_time_difference_extractor test_time_difference_extractor.cpp)

# Link libraries
target_link_libraries(test_cross_correlation
    tdoa
    pthread
    m
)

target_link_libraries(test_time_difference_extractor
    tdoa
    pthread
    m
)

# Set C++ standard
target_compile_features(test_cross_correlation PRIVATE cxx_std_17)
target_compile_features(test_time_difference_extractor PRIVATE cxx_std_17)

# Install tests
install(TARGETS 
    test_cross_correlation
    test_time_difference_extractor
    RUNTIME DESTINATION bin/tests
) 