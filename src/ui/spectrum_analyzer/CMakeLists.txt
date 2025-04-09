set(SPECTRUM_ANALYZER_SOURCES
    spectrum_display.cpp
    waterfall_display.cpp
    measurement_tools.cpp
    shader_program.cpp
    spectrum_analyzer.cpp
)

set(SPECTRUM_ANALYZER_HEADERS
    spectrum_display.h
    waterfall_display.h
    measurement_tools.h
    shader_program.h
    spectrum_analyzer.h
)

add_library(df_spectrum_analyzer ${SPECTRUM_ANALYZER_SOURCES} ${SPECTRUM_ANALYZER_HEADERS})

target_include_directories(df_spectrum_analyzer
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(df_spectrum_analyzer
    PUBLIC
    ${OPENGL_LIBRARIES}
    glfw
    glm
)

# Include signal_flow library for data integration
target_link_libraries(df_spectrum_analyzer
    PRIVATE
    signal_flow
) 