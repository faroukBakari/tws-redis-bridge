# Tests CMakeLists.txt

# REASON: Tests are optional, only build if Catch2 is available
if(NOT Catch2_FOUND)
    message(STATUS "Catch2 not found, skipping tests")
    return()
endif()

# Test executable placeholder
# TODO: Add test files as they are created
add_executable(test_serialization
    test_serialization.cpp
)

target_link_libraries(test_serialization
    PRIVATE
    Catch2::Catch2WithMain
)

target_include_directories(test_serialization
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${RapidJSON_INCLUDE_DIRS}
)

# Register with CTest
include(Catch)
catch_discover_tests(test_serialization)

# PERFORMANCE: Queue benchmark (not a test, standalone executable)
add_executable(benchmark_queue
    benchmark_queue.cpp
)

target_link_libraries(benchmark_queue
    PRIVATE
    concurrentqueue::concurrentqueue
)

target_include_directories(benchmark_queue
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
