#pragma once

#include "c2_logger.h"

#include <thread>

namespace c2fastlog {

// ============================================================================
// Public API
// ============================================================================

inline void initialize(std::shared_ptr<detail::C2SpdLogger> logger, LogLevel logging_level, std::chrono::seconds flush_interval) {
    detail::FastLogger::instance().init(std::move(logger), logging_level, flush_interval);
}

inline void preallocate() {
    detail::FastLogger::instance().allocate(spdlog::details::os::thread_id());
}

inline void release() {
    detail::FastLogger::instance().release();
}

// ============================================================================
// Fast Log Function (Spin Wait Policy)
// ============================================================================

template <typename... Args>
inline void fast_log(std::uint32_t& log_id,
                     const char* file_name,
                     int line,
                     const char* func_name,
                     LogLevel level,
                     std::string_view format_string,
                     Args&&... args) {
    // Filter logs by level at front-end
    if (level < detail::FastLogger::instance().get_logging_level()) [[unlikely]] {
        return;
    }

    // First log call: allocate ThreadBuffer and register static info
    if (log_id == 0) {
        preallocate();
        detail::FastLogger::instance().register_log_info(log_id,
                                                         detail::StaticLogInfo{spdlog::source_loc{file_name, line, func_name},
                                                                               level,
                                                                               format_string,
                                                                               detail::unpack_and_format<sizeof...(args), Args...>});
    }

    // Current log data: timestamp + log arguments
    auto& buffer = detail::FastLogger::instance().get_thread_buffer();
    std::size_t msg_size = sizeof(Timestamp) + detail::calculate_size(args...);
    auto ptr = buffer.allocate(static_cast<std::uint32_t>(msg_size));

    // Spin wait when buffer is full (never drop logs)
    while (ptr == nullptr) [[unlikely]] {
        std::this_thread::yield();
        ptr = buffer.allocate(static_cast<std::uint32_t>(msg_size));
    }

    ptr->type = log_id;
    ptr->size = static_cast<std::uint32_t>(msg_size);
    auto* content = ptr->content;
    *reinterpret_cast<Timestamp*>(content) = detail::get_timestamp_ns();
    content += sizeof(Timestamp);
    detail::pack(reinterpret_cast<char*>(content), std::forward<Args>(args)...);
    buffer.write_commit();
}

// ============================================================================
// Fast Log Function (Discard Oldest Policy)
// ============================================================================

// WARNING: This function discards oldest logs when buffer is full.
// Not suitable for scenarios requiring complete logs.
// Use case: HFT systems where latest logs are more important than completeness.
template <typename... Args>
inline void fast_log_discard_oldest(std::uint32_t& log_id,
                                    const char* file_name,
                                    int line,
                                    const char* func_name,
                                    LogLevel level,
                                    std::string_view format_string,
                                    Args&&... args) {
    // Filter logs by level at front-end
    if (level < detail::FastLogger::instance().get_logging_level()) [[unlikely]] {
        return;
    }

    // First log call: allocate ThreadBuffer and register static info
    if (log_id == 0) {
        preallocate();
        detail::FastLogger::instance().register_log_info(log_id,
                                                         detail::StaticLogInfo{spdlog::source_loc{file_name, line, func_name},
                                                                               level,
                                                                               format_string,
                                                                               detail::unpack_and_format<sizeof...(args), Args...>});
    }

    // Current log data: timestamp + log arguments
    auto& buffer = detail::FastLogger::instance().get_thread_buffer();
    std::size_t msg_size = sizeof(Timestamp) + detail::calculate_size(args...);

    // Use discard-oldest policy for allocation
    auto ptr = buffer.allocate_discard_oldest(static_cast<std::uint32_t>(msg_size));
    if (ptr == nullptr) [[unlikely]] {
        // Message too large to fit in queue
        return;
    }

    ptr->type = log_id;
    ptr->size = static_cast<std::uint32_t>(msg_size);
    auto* content = ptr->content;
    *reinterpret_cast<Timestamp*>(content) = detail::get_timestamp_ns();
    content += sizeof(Timestamp);
    detail::pack(reinterpret_cast<char*>(content), std::forward<Args>(args)...);
    buffer.write_commit();
}

}  // namespace c2fastlog

// ============================================================================
// Logging Macros
// ============================================================================

#define C2_LOG(level, ...)                                                             \
    do {                                                                               \
        static std::uint32_t log_id = 0;                                               \
        c2fastlog::fast_log(log_id, __FILE__, __LINE__, __func__, level, __VA_ARGS__); \
    } while (0)

#define C2_LOG_TRACE(...) C2_LOG(c2fastlog::LogLevel::trace, __VA_ARGS__)
#define C2_LOG_DEBUG(...) C2_LOG(c2fastlog::LogLevel::debug, __VA_ARGS__)
#define C2_LOG_INFO(...) C2_LOG(c2fastlog::LogLevel::info, __VA_ARGS__)
#define C2_LOG_WARN(...) C2_LOG(c2fastlog::LogLevel::warn, __VA_ARGS__)
#define C2_LOG_ERROR(...) C2_LOG(c2fastlog::LogLevel::error, __VA_ARGS__)

#define C2_LOG_DISCARD(level, ...)                                                                    \
    do {                                                                                              \
        static std::uint32_t log_id = 0;                                                              \
        c2fastlog::fast_log_discard_oldest(log_id, __FILE__, __LINE__, __func__, level, __VA_ARGS__); \
    } while (0)

#define C2_LOG_TRACE_DISCARD(...) C2_LOG_DISCARD(c2fastlog::LogLevel::trace, __VA_ARGS__)
#define C2_LOG_DEBUG_DISCARD(...) C2_LOG_DISCARD(c2fastlog::LogLevel::debug, __VA_ARGS__)
#define C2_LOG_INFO_DISCARD(...) C2_LOG_DISCARD(c2fastlog::LogLevel::info, __VA_ARGS__)
#define C2_LOG_WARN_DISCARD(...) C2_LOG_DISCARD(c2fastlog::LogLevel::warn, __VA_ARGS__)
#define C2_LOG_ERROR_DISCARD(...) C2_LOG_DISCARD(c2fastlog::LogLevel::error, __VA_ARGS__)
