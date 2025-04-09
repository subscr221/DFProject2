add_executable(test_map_display test_map_display.cpp)
add_executable(test_signal_display test_signal_display.cpp)
add_executable(test_tile_sources test_tile_sources.cpp)

target_link_libraries(test_map_display
    PRIVATE
    df_mapping
)

target_link_libraries(test_signal_display
    PRIVATE
    df_mapping
)

target_link_libraries(test_tile_sources
    PRIVATE
    df_mapping
)

# Copy test HTML file to build directory
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../web/index.html
              ${CMAKE_BINARY_DIR}/test/index.html
              COPYONLY) 