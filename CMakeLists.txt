cmake_minimum_required(VERSION 3.10)
project(DSA_Project)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find CURL package
find_package(CURL REQUIRED)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/backend/include)
include_directories(${PROJECT_SOURCE_DIR}/backend/external)
include_directories(${CURL_INCLUDE_DIRS})

# Source files
set(SOURCES
    backend/src/part1_ingestion/graph.cpp
    backend/src/part1_ingestion/overpass.cpp
    backend/src/part2_spatial/geometry.cpp
    backend/src/part2_spatial/kdtree.cpp
    backend/src/part3_allocation/allotment.cpp
    backend/src/part3_allocation/routing.cpp
    backend/src/part3_allocation/state.cpp
    backend/src/part4_api/server.cpp
)

# Create executable
add_executable(route_finder ${SOURCES})

# Link libraries
# Windows: link Winsock library for socket functions and CURL
if(WIN32)
    target_link_libraries(route_finder ws2_32 CURL::libcurl)
else()
    target_link_libraries(route_finder CURL::libcurl pthread)
endif()