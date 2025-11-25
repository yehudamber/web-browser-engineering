cmake_minimum_required(VERSION 4.0)

project(web-browser-engineering LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost 1.69 REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS SSL)

add_executable(browser src/main.cpp src/client-impl.cpp)
target_sources(browser PRIVATE FILE_SET CXX_MODULES FILES src/client.cppm src/display.cppm)
target_link_libraries(browser PRIVATE Boost::headers OpenSSL::SSL)
target_compile_definitions(browser PRIVATE RESOURCE_DIR="${CMAKE_SOURCE_DIR}/resources")
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(browser PRIVATE -Wall -Wextra -Wpedantic)
endif ()