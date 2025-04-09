##
# CMakeLists.txt for multilateration module
##

# List source files 
set(MULTILATERATION_SOURCES
    multilateration_solver.h
    multilateration_solver.cpp
)

# Add library
add_library(tdoa_multilateration STATIC ${MULTILATERATION_SOURCES})

# Find and link Eigen3
find_package(Eigen3 REQUIRED NO_MODULE)

# Include directories
target_include_directories(tdoa_multilateration PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${EIGEN3_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(tdoa_multilateration PUBLIC
    Eigen3::Eigen
    tdoa_time_difference
)

# Set C++ standard
target_compile_features(tdoa_multilateration PRIVATE cxx_std_17)

# Set compile options
target_compile_options(tdoa_multilateration PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O3>
)

# Add installation targets
install(TARGETS tdoa_multilateration
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES
    multilateration_solver.h
    DESTINATION include/tdoa/multilateration
) 