cmake_minimum_required(VERSION 3.20)
project(tws-redis-bridge VERSION 0.1.0 LANGUAGES CXX)

# Modern C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Strict compiler flags
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0 -fsanitize=address)
        add_link_options(-fsanitize=address)
    endif()
endif()

# Find Conan-provided dependencies
find_package(redis++ REQUIRED)
find_package(RapidJSON REQUIRED)
find_package(concurrentqueue REQUIRED)

# REASON: Use system protobuf 3.12.4 (matches TWS API regenerated files)
# CMAKE_PREFIX_PATH from Conan interferes with system protobuf, so we use pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(Protobuf REQUIRED protobuf>=3.12)
include_directories(${Protobuf_INCLUDE_DIRS})
link_directories(${Protobuf_LIBRARY_DIRS})

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/vendor/tws-api/source/cppclient/client)
include_directories(${CMAKE_SOURCE_DIR}/vendor/tws-api/source/cppclient/client/protobufUnix)

# TWS API library (vendored)
# Note: TWS API must be placed in vendor/tws-api/
set(TWS_API_DIR "${CMAKE_SOURCE_DIR}/vendor/tws-api/source/cppclient/client")
set(TWS_API_PROTO_DIR "${TWS_API_DIR}/protobufUnix")

# Collect TWS API source files
file(GLOB TWS_API_SOURCES "${TWS_API_DIR}/*.cpp" "${TWS_API_PROTO_DIR}/*.cc")

# Create TWS API static library
add_library(tws_api STATIC ${TWS_API_SOURCES})
target_include_directories(tws_api PUBLIC 
    ${TWS_API_DIR}
    ${TWS_API_PROTO_DIR}
)

# Find Intel BID library (required for TWS Decimal type)
find_library(INTEL_BID_LIB
    NAMES libbid.a
    PATHS ${CMAKE_SOURCE_DIR}/vendor/IntelRDFPMathLib20U2/LIBRARY
    NO_DEFAULT_PATH
    REQUIRED
)
message(STATUS "Intel BID library found: ${INTEL_BID_LIB}")

target_link_libraries(tws_api PUBLIC 
    ${Protobuf_LIBRARIES}
    ${INTEL_BID_LIB}
)

# REASON: TWS API is third-party code with unused parameters; disable -Werror for this target only
target_compile_options(tws_api PRIVATE -Wno-error -Wno-unused-parameter)

# Main executable
add_executable(tws_bridge
    src/main.cpp
    src/TwsClient.cpp
    src/RedisPublisher.cpp
    src/Serialization.cpp
)

target_link_libraries(tws_bridge
    PRIVATE
    tws_api
    redis++::redis++_static
    concurrentqueue::concurrentqueue
)

target_include_directories(tws_bridge
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${RapidJSON_INCLUDE_DIRS}
)

# Testing
enable_testing()
find_package(Catch2 3 QUIET)

if(Catch2_FOUND)
    add_subdirectory(tests)
endif()

# Installation
install(TARGETS tws_bridge DESTINATION bin)
