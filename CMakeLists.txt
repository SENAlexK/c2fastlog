cmake_minimum_required(VERSION 3.14)
project(c2log VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 编译选项
option(ENABLE_C2FASTLOG "Enable c2fastlog async logging" ON)
option(FORCE_SPDLOG "Force use spdlog instead of c2fastlog" OFF)
option(BUILD_BENCHMARK "Build benchmark executable" ON)

# 优化选项
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

# 头文件目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/spdlog/include
)

# 定义头文件库 (header-only)
add_library(c2log INTERFACE)
target_include_directories(c2log INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/spdlog/include
)

# 编译定义
if(ENABLE_C2FASTLOG AND NOT FORCE_SPDLOG)
    target_compile_definitions(c2log INTERFACE ENABLE_X2FASTLOG)
    message(STATUS "Using c2fastlog async logging")
elseif(FORCE_SPDLOG)
    target_compile_definitions(c2log INTERFACE FORCE_SPDLOG)
    message(STATUS "Using spdlog")
else()
    message(STATUS "Using fmt::print")
endif()

# 示例程序
add_executable(c2log_example example/main.cpp)
target_link_libraries(c2log_example PRIVATE c2log pthread)

# 性能测试
if(BUILD_BENCHMARK)
    add_executable(c2log_bench example/bench.cpp)
    target_link_libraries(c2log_bench PRIVATE c2log pthread)
    target_compile_definitions(c2log_bench PRIVATE ENABLE_X2FASTLOG)
endif()
