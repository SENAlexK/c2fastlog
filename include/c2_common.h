#pragma once

#include <cstdint>

#include <chrono>
#include <source_location>

namespace c2fastlog {

// ============================================================================
// Timestamp Type
// ============================================================================

using Timestamp = std::uint64_t;  // Nanoseconds since epoch

// ============================================================================
// Log Levels
// ============================================================================

enum class LogLevel : int {
    trace = 0,
    debug = 1,
    info = 2,
    warn = 3,
    error = 4,
};

template <typename OStream>
inline OStream& operator<<(OStream& stream, LogLevel level) {
    switch (level) {
    case LogLevel::trace:
        stream << "trace";
        break;
    case LogLevel::debug:
        stream << "debug";
        break;
    case LogLevel::info:
        stream << "info";
        break;
    case LogLevel::warn:
        stream << "warn";
        break;
    case LogLevel::error:
        stream << "error";
        break;
    }
    return stream;
}

// ============================================================================
// Timestamp Utilities
// ============================================================================

namespace detail {

#if defined(_WIN32) || defined(_WIN64)

[[nodiscard]] inline Timestamp get_timestamp_ns() noexcept {
    return static_cast<Timestamp>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

#else

[[nodiscard]] inline Timestamp get_timestamp_ns() noexcept {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<Timestamp>(ts.tv_sec) * 1'000'000'000ULL + static_cast<Timestamp>(ts.tv_nsec);
}

#endif

// Forward declaration
class C2SpdLogger;

}  // namespace detail

}  // namespace c2fastlog
