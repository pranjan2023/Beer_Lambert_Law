cmake_minimum_required(VERSION 3.20)
project(mc_transport LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)


add_library(simulation_lib src/simulation.cpp src/thread_pool.cpp src/shower.cpp)

add_executable(mc_transport src/main.cpp)
target_link_libraries(mc_transport PRIVATE simulation_lib)

# -------------------------
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(run_tests tests/test_simulation.cpp)
target_link_libraries(run_tests PRIVATE simulation_lib GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(run_tests)