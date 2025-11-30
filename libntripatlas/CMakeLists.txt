# NTRIP Atlas Library - CMake Configuration
cmake_minimum_required(VERSION 3.10)
project(ntripatlas VERSION 1.0.0 LANGUAGES C CXX)

# C/C++ Standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Options
option(BUILD_SHARED_LIBS "Build shared library" ON)
option(BUILD_EXAMPLES "Build example programs" OFF)
option(ENABLE_TESTING "Enable unit tests" OFF)

# Platform detection
if(ESP32)
    set(NTRIP_PLATFORM "esp32")
    message(STATUS "Building for ESP32 (Arduino framework)")
elseif(WIN32)
    set(NTRIP_PLATFORM "windows")
    message(STATUS "Building for Windows")
elseif(UNIX AND NOT APPLE)
    set(NTRIP_PLATFORM "linux")
    message(STATUS "Building for Linux")
elseif(APPLE)
    set(NTRIP_PLATFORM "linux")
    message(STATUS "Building for macOS (using Linux platform)")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Core source files
set(CORE_SOURCES
    src/ntrip_stream_parser.c
    src/ntrip_gga.c
    src/ntrip_utils.c
)

# Platform-specific sources
if(NTRIP_PLATFORM STREQUAL "linux")
    list(APPEND CORE_SOURCES platforms/linux/ntrip_platform_linux.c)
    find_package(CURL REQUIRED)
    set(PLATFORM_LIBS ${CURL_LIBRARIES} m)
    include_directories(${CURL_INCLUDE_DIRS})
elseif(NTRIP_PLATFORM STREQUAL "windows")
    list(APPEND CORE_SOURCES platforms/windows/ntrip_platform_windows.c)
    set(PLATFORM_LIBS winhttp advapi32 ws2_32)
elseif(NTRIP_PLATFORM STREQUAL "esp32")
    list(APPEND CORE_SOURCES platforms/esp32/ntrip_platform_esp32.cpp)
    # ESP32 libs provided by Arduino framework
    set(PLATFORM_LIBS "")
endif()

# Library target
add_library(ntripatlas ${CORE_SOURCES})

# Link libraries
target_link_libraries(ntripatlas ${PLATFORM_LIBS})

# Public headers
set_target_properties(ntripatlas PROPERTIES
    PUBLIC_HEADER "include/ntrip_atlas.h;include/ntrip_atlas_config.h"
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# Compiler warnings
if(MSVC)
    target_compile_options(ntripatlas PRIVATE /W4)
else()
    target_compile_options(ntripatlas PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Installation
include(GNUInstallDirs)
install(TARGETS ntripatlas
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ntrip_atlas
)

# Examples
if(BUILD_EXAMPLES AND NOT ESP32)
    message(STATUS "Building examples")
    # Add example programs here
endif()

# Testing
if(ENABLE_TESTING)
    enable_testing()
    # Add test programs here
endif()

# Package configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ntripatlas-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ntripatlas-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ntripatlas
)

# Summary
message(STATUS "")
message(STATUS "NTRIP Atlas Configuration Summary")
message(STATUS "==================================")
message(STATUS "Version:         ${PROJECT_VERSION}")
message(STATUS "Platform:        ${NTRIP_PLATFORM}")
message(STATUS "Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "Shared libs:     ${BUILD_SHARED_LIBS}")
message(STATUS "Examples:        ${BUILD_EXAMPLES}")
message(STATUS "Testing:         ${ENABLE_TESTING}")
message(STATUS "Install prefix:  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
