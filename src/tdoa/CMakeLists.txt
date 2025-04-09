##
# CMakeLists.txt for TDOA module
##

# List source files
set(TDOA_SOURCES
    correlation/cross_correlation.h
    correlation/cross_correlation.cpp
    correlation/window_functions.cpp
    correlation/correlation_peak.cpp
    time_difference/time_difference_extractor.h
    time_difference/time_difference_extractor.cpp
    multilateration/multilateration_solver.h
    multilateration/multilateration_solver.cpp
)

# Add library
add_library(tdoa STATIC ${TDOA_SOURCES})

# Include directories
target_include_directories(tdoa PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Find and link Eigen3
list(APPEND CMAKE_PREFIX_PATH "C:/DEVTOOLS/vcpkg/packages/eigen3_x64-windows")
find_package(Eigen3 REQUIRED NO_MODULE)

# Link libraries
target_link_libraries(tdoa
    pthread
    m
    Eigen3::Eigen
)

# Set C++ standard
target_compile_features(tdoa PRIVATE cxx_std_17)

# Set compile options
target_compile_options(tdoa PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O3>
)

# Add installation targets
install(TARGETS tdoa
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES
    correlation/cross_correlation.h
    time_difference/time_difference_extractor.h
    multilateration/multilateration_solver.h
    DESTINATION include/tdoa
)

# Add subdirectories as we add more components
add_subdirectory(multilateration)
add_subdirectory(utils)

# Add tests subdirectory
add_subdirectory(tests) 