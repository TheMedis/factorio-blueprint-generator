cmake_minimum_required(VERSION 3.24)
project(factory CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
    GIT_SHALLOW TRUE)
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(Catch2 json)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)  # for include(Catch)
find_package(Threads REQUIRED)

# Warnings (and sanitizers in Debug) for our own targets only.
add_library(factory_flags INTERFACE)
target_compile_options(factory_flags INTERFACE -Wall -Wextra -Wpedantic)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(factory_flags INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(factory_flags INTERFACE -fsanitize=address,undefined)
endif()

add_library(factory_core
    src/entities.cpp
)
target_include_directories(factory_core PUBLIC src)
target_link_libraries(factory_core PUBLIC factory_flags nlohmann_json::nlohmann_json Threads::Threads)

enable_testing()
add_executable(factory_tests
    tests/test_entities.cpp
)
target_link_libraries(factory_tests PRIVATE factory_core Catch2::Catch2WithMain)
include(Catch)
catch_discover_tests(factory_tests)
