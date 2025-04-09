##
# CMakeLists.txt for time_difference module
##

# List source files
set(TIME_DIFFERENCE_SOURCES
    time_difference_extractor.h
    time_difference_extractor.cpp
)

# Add library
add_library(tdoa_time_difference STATIC ${TIME_DIFFERENCE_SOURCES})

# Include directories
target_include_directories(tdoa_time_difference PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Link libraries
target_link_libraries(tdoa_time_difference PUBLIC
    tdoa_correlation
)

# Set C++ standard
target_compile_features(tdoa_time_difference PRIVATE cxx_std_17)

# Set compile options
target_compile_options(tdoa_time_difference PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O3>
)

# Add installation targets
install(TARGETS tdoa_time_difference
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES
    time_difference_extractor.h
    DESTINATION include/tdoa/time_difference
) 