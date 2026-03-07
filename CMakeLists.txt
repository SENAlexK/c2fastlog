cmake_minimum_required(VERSION 3.20)
project(c2fastlog VERSION 1.0.0 LANGUAGES CXX)

# ============================================================================
# C++ Standard
# ============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# Build Options
# ============================================================================

option(C2FASTLOG_BUILD_EXAMPLES "Build example executables" ON)
option(C2FASTLOG_BUILD_BENCHMARK "Build benchmark executable" ON)

# ============================================================================
# Compiler Flags
# ============================================================================

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi")
endif()

# ============================================================================
# Dependencies
# ============================================================================

# spdlog (header-only mode)
add_subdirectory(third_party/spdlog EXCLUDE_FROM_ALL)

# ============================================================================
# Header-Only Library
# ============================================================================

add_library(c2fastlog INTERFACE)
add_library(c2fastlog::c2fastlog ALIAS c2fastlog)

target_include_directories(c2fastlog INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# For building: link to bundled spdlog
# For installation: users should find_package(spdlog) themselves (handled in Config.cmake.in)
target_link_libraries(c2fastlog INTERFACE
    $<BUILD_INTERFACE:spdlog::spdlog_header_only>
)

target_compile_features(c2fastlog INTERFACE cxx_std_20)

# ============================================================================
# Examples
# ============================================================================

if(C2FASTLOG_BUILD_EXAMPLES)
    add_executable(c2fastlog_example example/main.cpp)
    target_link_libraries(c2fastlog_example PRIVATE c2fastlog)

    if(UNIX AND NOT APPLE)
        target_link_libraries(c2fastlog_example PRIVATE pthread)
    endif()
endif()

# ============================================================================
# Benchmark
# ============================================================================

if(C2FASTLOG_BUILD_BENCHMARK)
    add_executable(c2fastlog_benchmark example/benchmark.cpp)
    target_link_libraries(c2fastlog_benchmark PRIVATE c2fastlog)

    if(UNIX AND NOT APPLE)
        target_link_libraries(c2fastlog_benchmark PRIVATE pthread)
    endif()
endif()

# ============================================================================
# Installation
# ============================================================================

include(GNUInstallDirs)

install(TARGETS c2fastlog
    EXPORT c2fastlogTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT c2fastlogTargets
    FILE c2fastlogTargets.cmake
    NAMESPACE c2fastlog::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/c2fastlog
)

# ============================================================================
# Package Configuration
# ============================================================================

include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/c2fastlogConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/c2fastlogConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/c2fastlog
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/c2fastlogConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/c2fastlogConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/c2fastlogConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/c2fastlog
)
