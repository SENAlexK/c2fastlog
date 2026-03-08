<p align="center">
  <h1 align="center">C2FastLog</h1>
  <p align="center">
    <b>Ultra-Low Latency Asynchronous Logging Library for High-Frequency Trading Systems</b>
  </p>
  <p align="center">
    <a href="README.md"><b>English</b></a> | <a href="README_CN.md">中文</a>
  </p>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License">
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue.svg" alt="C++20">
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20Windows-lightgrey.svg" alt="Platform">
  <img src="https://img.shields.io/badge/header--only-yes-green.svg" alt="Header Only">
</p>

---

## Overview

**C2FastLog** is a high-performance, header-only asynchronous logging library designed specifically for latency-critical applications such as high-frequency trading (HFT) systems. It achieves **sub-200 nanosecond** logging latency by deferring formatting to a background thread and using lock-free SPSC queues.

### Key Features

- **Ultra-Low Latency**: ~170ns per log call in the hot path
- **High Throughput**: 5+ million logs per second
- **Zero Allocation in Hot Path**: Pre-allocated thread-local buffers
- **Lock-Free Design**: SPSC queue for minimal contention
- **Header-Only**: Easy integration, just include and use
- **Modern C++20**: Leverages latest language features
- **spdlog Backend**: Flexible output destinations (console, file, etc.)
- **Thread-Safe**: Each thread has its own buffer, no contention

## Performance

| Metric | Value |
|--------|-------|
| Throughput | **5.7+ M logs/sec** |
| Latency (avg) | **~174 ns/log** |
| Buffer Size | 16 MB per thread (configurable) |

*Benchmarked on Linux with GCC 15, Release build (-O3)*

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        Producer Threads                          │
├─────────────┬─────────────┬─────────────┬─────────────┬─────────┤
│  Thread 1   │  Thread 2   │  Thread 3   │    ...      │ Thread N│
│ ┌─────────┐ │ ┌─────────┐ │ ┌─────────┐ │             │         │
│ │  SPSC   │ │ │  SPSC   │ │ │  SPSC   │ │             │         │
│ │  Queue  │ │ │  Queue  │ │ │  Queue  │ │             │         │
│ └────┬────┘ │ └────┬────┘ │ └────┬────┘ │             │         │
└──────┼──────┴──────┼──────┴──────┼──────┴─────────────┴─────────┘
       │             │             │
       └─────────────┼─────────────┘
                     │
                     ▼
       ┌─────────────────────────────┐
       │     Background Thread       │
       │  ┌───────────────────────┐  │
       │  │   Min-Heap Merge      │  │
       │  │  (Timestamp Order)    │  │
       │  └───────────┬───────────┘  │
       │              │              │
       │  ┌───────────▼───────────┐  │
       │  │   Format & Output     │  │
       │  │   (spdlog backend)    │  │
       │  └───────────────────────┘  │
       └─────────────────────────────┘
```

### Design Philosophy

1. **Hot Path Optimization**: Only capture raw arguments, defer formatting
2. **Per-Thread Isolation**: Lock-free SPSC queues eliminate contention
3. **Timestamp Ordering**: Background thread merges logs by timestamp
4. **Flexible Backend**: Leverage spdlog's rich sink ecosystem

## Quick Start

### Requirements

- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- CMake 3.20+
- spdlog (included as submodule)

### Installation

```bash
# Clone with submodules
git clone --recursive https://github.com/SENAlexK/c2fastlog.git
cd c2fastlog

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run examples
./c2fastlog_example
./c2fastlog_benchmark
```

### Basic Usage

```cpp
#include <c2_fastlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

int main() {
    // 1. Create sink (spdlog compatible)
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");

    // 2. Create C2FastLog logger
    auto logger = std::make_shared<c2fastlog::detail::C2SpdLogger>("app", console_sink);
    logger->set_level(spdlog::level::trace);

    // 3. Initialize
    c2fastlog::initialize(logger, c2fastlog::LogLevel::trace, std::chrono::seconds(1));

    // 4. Log using macros
    C2_LOG_TRACE("This is a trace message");
    C2_LOG_DEBUG("Debug value: {}", 42);
    C2_LOG_INFO("User {} logged in from {}", "alice", "192.168.1.1");
    C2_LOG_WARN("Memory usage at {}%", 85);
    C2_LOG_ERROR("Failed to connect: {}", "timeout");

    // 5. Cleanup
    std::this_thread::sleep_for(std::chrono::seconds(1));
    c2fastlog::release();
    return 0;
}
```

### CMake Integration

```cmake
# Option 1: Add as subdirectory
add_subdirectory(third_party/c2fastlog)
target_link_libraries(your_target PRIVATE c2fastlog::c2fastlog)

# Option 2: Find package (after installation)
find_package(c2fastlog REQUIRED)
target_link_libraries(your_target PRIVATE c2fastlog::c2fastlog)
```

## API Reference

### Initialization Functions

```cpp
namespace c2fastlog {

// Initialize the logging system
void initialize(
    std::shared_ptr<detail::C2SpdLogger> logger,  // spdlog-based logger
    LogLevel logging_level,                        // Minimum log level
    std::chrono::seconds flush_interval            // Auto-flush interval
);

// Pre-allocate thread buffer (optional, auto-allocated on first log)
void preallocate();

// Release resources and flush all pending logs
void release();

}
```

### Logging Macros

| Macro | Description |
|-------|-------------|
| `C2_LOG_TRACE(fmt, ...)` | Trace level log (most verbose) |
| `C2_LOG_DEBUG(fmt, ...)` | Debug level log |
| `C2_LOG_INFO(fmt, ...)` | Info level log |
| `C2_LOG_WARN(fmt, ...)` | Warning level log |
| `C2_LOG_ERROR(fmt, ...)` | Error level log (least verbose) |

### Discard-Oldest Policy Macros

For scenarios where latest logs are more important than completeness (e.g., HFT market data logging):

| Macro | Description |
|-------|-------------|
| `C2_LOG_TRACE_DISCARD(fmt, ...)` | Trace with discard-oldest |
| `C2_LOG_DEBUG_DISCARD(fmt, ...)` | Debug with discard-oldest |
| `C2_LOG_INFO_DISCARD(fmt, ...)` | Info with discard-oldest |
| `C2_LOG_WARN_DISCARD(fmt, ...)` | Warn with discard-oldest |
| `C2_LOG_ERROR_DISCARD(fmt, ...)` | Error with discard-oldest |

### Log Levels

```cpp
enum class LogLevel : int {
    trace = 0,  // Most verbose - detailed tracing
    debug = 1,  // Debug information
    info = 2,   // General information
    warn = 3,   // Warning conditions
    error = 4,  // Error conditions (least verbose)
};
```

## Configuration

### Buffer Size

Default buffer size is 16MB per thread. Override at compile time:

```cpp
// Before including c2_fastlog.h
#define FASTLOG_THREAD_BUFFER_SIZE (1 << 26)  // 64MB
#include <c2_fastlog.h>
```

### Custom Sinks

C2FastLog uses spdlog as backend, supporting all spdlog sinks:

```cpp
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>

// File sink
auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("app.log");

// Rotating file sink (5MB max, 3 rotated files)
auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
    "app.log", 5*1024*1024, 3);

// Daily rotating file sink
auto daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
    "app.log", 0, 0);  // Rotate at midnight

// Multiple sinks
auto logger = std::make_shared<c2fastlog::detail::C2SpdLogger>(
    "multi", spdlog::sinks_init_list{console_sink, file_sink});
```

## Best Practices

### 1. Preallocate in Performance-Critical Threads

```cpp
void trading_thread() {
    // Preallocate buffer before entering hot path
    c2fastlog::preallocate();

    while (running) {
        // Critical trading logic here
        // Logging won't allocate memory
        C2_LOG_INFO("Order executed: id={}, price={}", order.id, order.price);
    }
}
```

### 2. Use Appropriate Log Levels

```cpp
// Development
c2fastlog::initialize(logger, c2fastlog::LogLevel::trace, ...);

// Production
c2fastlog::initialize(logger, c2fastlog::LogLevel::info, ...);
```

### 3. Avoid Expensive Arguments in Hot Path

```cpp
// BAD: expensive toString() called even if log is filtered
C2_LOG_DEBUG("State: {}", expensive_object.toString());

// GOOD: simple types are cheap to capture
C2_LOG_DEBUG("State: id={}, status={}", obj.id, obj.status);
```

### 4. Use Discard Policy for Market Data

```cpp
void market_data_handler(const Quote& quote) {
    // Use discard-oldest policy for high-frequency data
    // Latest quotes are more important than historical completeness
    C2_LOG_INFO_DISCARD("Quote: {}@{}", quote.price, quote.volume);
}
```

### 5. Graceful Shutdown

```cpp
void shutdown() {
    // Signal threads to stop
    running = false;

    // Wait for pending logs to flush
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Release resources
    c2fastlog::release();
}
```

## Memory Layout

### Thread Buffer Structure

```
Thread Buffer (16MB default):
┌──────────────────────────────────────────────────────────────┐
│ Msg1 │ Msg2 │ Msg3 │ ... │ Padding (if needed) │ ... │      │
└──────────────────────────────────────────────────────────────┘
   │
   ▼
┌─────────────────────────────┐
│ type: uint32_t              │  ← Log ID (registered on first use)
│ size: uint32_t              │  ← Content size in bytes
│ content[]:                  │  ← [timestamp (8B)][packed args...]
│   - timestamp: uint64_t     │
│   - arg1, arg2, ...         │
└─────────────────────────────┘
```

### Argument Packing

| Type | Storage |
|------|---------|
| Integers | Direct copy (1-8 bytes) |
| Floats | Direct copy (4-8 bytes) |
| C-strings | Length (8B) + data + null |
| std::string | Length (8B) + data + null |
| Custom types | Copy construct or memcpy |

## Benchmarking

Run the included benchmark:

```bash
./c2fastlog_benchmark
```

Expected output:
```
=== C2FastLog Benchmark ===

Warming up with 100000 logs...
Running benchmark with 10000000 logs...

=== Results ===
Total logs:      10000000
Total time:      1.74 s
Throughput:      5.73 M logs/sec
Latency:         174.47 ns/log

Waiting for logs to flush...
Benchmark completed!
```

## Project Structure

```
c2fastlog/
├── include/
│   ├── c2_fastlog.h         # Main API header
│   ├── c2_logger.h          # Core FastLogger implementation
│   ├── c2_packer.h          # Argument pack/unpack utilities
│   ├── c2_spdlog.h          # spdlog integration
│   ├── c2_variable_queue.h  # SPSC queue implementation
│   ├── c2_common.h          # Common definitions
│   └── utils.h              # Utility functions
├── example/
│   ├── main.cpp             # Usage example
│   └── benchmark.cpp        # Performance benchmark
├── third_party/
│   └── spdlog/              # spdlog submodule
├── cmake/
│   └── c2fastlogConfig.cmake.in
├── CMakeLists.txt
├── README.md                # English documentation
├── README_CN.md             # Chinese documentation
└── LICENSE
```

## Comparison with Other Libraries

| Feature | C2FastLog | spdlog async | NanoLog |
|---------|-----------|--------------|---------|
| Latency | ~170ns | ~500ns | ~10ns |
| Throughput | 5.7M/s | 1M/s | 80M/s |
| Format Location | Background | Background | Compile-time |
| Header-Only | Yes | Yes | No |
| C++ Standard | C++20 | C++11 | C++17 |
| Thread-Safe | Yes | Yes | Yes |

*Note: NanoLog achieves lower latency through compile-time format parsing, but requires special tooling.*

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- [spdlog](https://github.com/gabime/spdlog) - Fast C++ logging library
- [fmt](https://github.com/fmtlib/fmt) - Modern formatting library
- [NanoLog](https://github.com/PlatformLab/NanoLog) - Inspiration for low-latency design

---

<p align="center">
  <b>Built for High-Frequency Trading</b><br>
  <i>Where every nanosecond counts</i>
</p>
