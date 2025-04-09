cmake_minimum_required(VERSION 3.10)

# UI components for Direction Finder
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

# Add spectrum analyzer subdirectory
add_subdirectory(spectrum_analyzer)

# Create UI library
add_library(df_ui INTERFACE)
target_link_libraries(df_ui 
    INTERFACE 
    df_spectrum_analyzer
    ${OPENGL_LIBRARIES}
    glfw
    glm
)

target_include_directories(df_ui
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
) 