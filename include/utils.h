#pragma once

#include <cstdint>
#include <cstring>
#include <ctime>

#include <string>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace c2fastlog {

// ============================================================================
// Constants
// ============================================================================

inline constexpr std::size_t cache_line_size = 64;

inline constexpr std::uint64_t nanos_per_second = 1'000'000'000ULL;
inline constexpr std::uint64_t nanos_per_micro = 1'000ULL;
inline constexpr std::uint64_t micros_per_second = 1'000'000ULL;

// ============================================================================
// Force Inline
// ============================================================================

#if defined(_MSC_VER)
#define C2_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define C2_FORCE_INLINE inline __attribute__((always_inline))
#else
#define C2_FORCE_INLINE inline
#endif

// ============================================================================
// Time Utilities
// ============================================================================

#if defined(_WIN32) || defined(_WIN64)

[[nodiscard]] inline std::uint64_t get_nano_time() noexcept {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return static_cast<std::uint64_t>(counter.QuadPart) * nanos_per_second / static_cast<std::uint64_t>(freq.QuadPart);
}

[[nodiscard]] inline std::uint64_t get_micro_time() noexcept {
    return get_nano_time() / nanos_per_micro;
}

[[nodiscard]] inline std::uint64_t get_seconds_time() noexcept {
    return get_nano_time() / nanos_per_second;
}

#else  // POSIX

[[nodiscard]] inline std::uint64_t read_tsc() noexcept {
    std::uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return static_cast<std::uint64_t>(lo) | (static_cast<std::uint64_t>(hi) << 32);
}

[[nodiscard]] inline std::uint64_t get_nano_time() noexcept {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * nanos_per_second + static_cast<std::uint64_t>(ts.tv_nsec);
}

[[nodiscard]] inline std::uint64_t get_micro_time() noexcept {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * micros_per_second + static_cast<std::uint64_t>(ts.tv_nsec) / nanos_per_micro;
}

[[nodiscard]] inline std::uint64_t get_seconds_time() noexcept {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec);
}

#endif

inline void nano_sleep(std::uint64_t duration) noexcept {
    const std::uint64_t start = get_nano_time();
    while (get_nano_time() - start < duration) {
        // Busy wait for precise timing
    }
}

// ============================================================================
// Process Utilities
// ============================================================================

#if !defined(_WIN32) && !defined(_WIN64)

[[nodiscard]] inline bool is_process_running(const char* process_name) noexcept {
    std::string lock_file = std::string(process_name) + ".lock";
    int lock_fd = open(lock_file.c_str(), O_CREAT | O_RDWR, 0666);
    if (lock_fd < 0) [[unlikely]] {
        return true;
    }

    int rc = flock(lock_fd, LOCK_EX | LOCK_NB);
    if (rc != 0) [[unlikely]] {
        close(lock_fd);
        return true;
    }
    return false;
}

#endif

}  // namespace c2fastlog
