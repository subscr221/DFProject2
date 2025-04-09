set(MAPPING_SOURCES
    map_display.cpp
    signal_marker.cpp
    tile_server.cpp
    tile_source.cpp
    tile_coverage.cpp
)

set(MAPPING_HEADERS
    map_display.h
    signal_marker.h
    tile_server.h
    tile_source.h
    tile_coverage.h
)

add_library(df_mapping ${MAPPING_SOURCES} ${MAPPING_HEADERS})

target_include_directories(df_mapping
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Find required packages
list(APPEND CMAKE_PREFIX_PATH "C:/DEVTOOLS/vcpkg/packages/nlohmann-json_x64-windows/share/nlohmann_json")
find_package(CURL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenCV REQUIRED)

# Link with libraries
target_link_libraries(df_mapping
    PUBLIC
    ${CURL_LIBRARIES}
    ZLIB::ZLIB
    nlohmann_json::nlohmann_json
    ${OpenCV_LIBS}
)

# Link with Emscripten libraries when building for web
if(EMSCRIPTEN)
    target_link_libraries(df_mapping
        PUBLIC
        ${EMSCRIPTEN_LIBRARIES}
    )
endif()

# Create web directory if it doesn't exist
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/web)

# Set web assets destination
set(WEB_ASSETS_DEST "${CMAKE_BINARY_DIR}/web")

# Copy web assets to build directory
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/web/leaflet.css"
     DESTINATION "${WEB_ASSETS_DEST}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/web/leaflet.js"
     DESTINATION "${WEB_ASSETS_DEST}")

# Copy Leaflet.markercluster files
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/web/leaflet.markercluster.js"
     DESTINATION "${WEB_ASSETS_DEST}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/web/MarkerCluster.css"
     DESTINATION "${WEB_ASSETS_DEST}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/web/MarkerCluster.Default.css"
     DESTINATION "${WEB_ASSETS_DEST}")

# Copy Leaflet.Measure files
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/web/leaflet.measure.js"
     DESTINATION "${WEB_ASSETS_DEST}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/web/leaflet.measure.css"
     DESTINATION "${WEB_ASSETS_DEST}")

# Copy Leaflet.Bearing files
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/web/leaflet.bearing.js"
     DESTINATION "${WEB_ASSETS_DEST}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/web/leaflet.bearing.css"
     DESTINATION "${WEB_ASSETS_DEST}")

# Add test executables
add_executable(test_map_display test/test_map_display.cpp)
target_link_libraries(test_map_display PRIVATE df_mapping)

add_executable(test_signal_display test/test_signal_display.cpp)
target_link_libraries(test_signal_display PRIVATE df_mapping)

add_executable(test_measurement_tools test/test_measurement_tools.cpp)
target_link_libraries(test_measurement_tools PRIVATE df_mapping)

add_executable(test_tile_server test/test_tile_server.cpp)
target_link_libraries(test_tile_server PRIVATE df_mapping)

add_executable(test_tile_sources test/test_tile_sources.cpp)
target_link_libraries(test_tile_sources PRIVATE df_mapping)

add_executable(test_tile_coverage test/test_tile_coverage.cpp)
target_link_libraries(test_tile_coverage PRIVATE df_mapping)