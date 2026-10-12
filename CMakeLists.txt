cmake_minimum_required(VERSION 3.16)
project(distributed_in_memory_cache LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Threads REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_library(partitioned_cache src/partitioned_cache.cpp)
target_include_directories(partitioned_cache PUBLIC include)
target_link_libraries(partitioned_cache PUBLIC Threads::Threads)
target_compile_options(partitioned_cache PRIVATE -Wall -Wextra -Wpedantic)
add_executable(cache_test tests/cache_test.cpp)
target_link_libraries(cache_test PRIVATE partitioned_cache)
add_executable(cache_benchmark benchmarks/cache_benchmark.cpp)
target_link_libraries(cache_benchmark PRIVATE partitioned_cache)

add_executable(cache_server src/cache_server.cpp)
target_link_libraries(cache_server PRIVATE partitioned_cache)
enable_testing()
add_test(NAME cache_correctness COMMAND cache_test)
add_test(
  NAME distributed_routing
  COMMAND ${CMAKE_COMMAND} -E env
    PYTHONPATH=${CMAKE_SOURCE_DIR}/python
    CACHE_SERVER=$<TARGET_FILE:cache_server>
    ${Python3_EXECUTABLE} -m unittest tests/distributed_test.py -v
)
set_tests_properties(distributed_routing PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
