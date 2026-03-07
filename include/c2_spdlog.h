// c2_spdlog.h - C2FastLog spdlog integration

#pragma once

#include "c2_common.h"

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

namespace c2fastlog {
namespace detail {

// ============================================================================
// Level Conversion
// ============================================================================

[[nodiscard]] inline LogLevel get_level_from_spdlog(spdlog::level::level_enum spd_level) noexcept {
    switch (spd_level) {
    case spdlog::level::trace:
        return LogLevel::trace;
    case spdlog::level::debug:
        return LogLevel::debug;
    case spdlog::level::info:
        return LogLevel::info;
    case spdlog::level::warn:
        return LogLevel::warn;
    case spdlog::level::err:
    default:
        return LogLevel::error;
    }
}

[[nodiscard]] inline spdlog::level::level_enum turn_level_to_spdlog(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::trace:
        return spdlog::level::trace;
    case LogLevel::debug:
        return spdlog::level::debug;
    case LogLevel::info:
        return spdlog::level::info;
    case LogLevel::warn:
        return spdlog::level::warn;
    case LogLevel::error:
    default:
        return spdlog::level::err;
    }
}

// ============================================================================
// C2 SPD Logger
// ============================================================================

// Custom spdlog::logger for C2FastLog.
// Since C2FastLog implements async operations, C2SpdLogger only needs to support synchronous logging.
class C2SpdLogger : public spdlog::logger {
    using Base = spdlog::logger;

public:
    // Forward base class constructors
    explicit C2SpdLogger(std::string name) : Base(std::move(name)) {
    }

    template <typename It>
    C2SpdLogger(std::string name, It begin, It end) : Base(std::move(name), begin, end) {
    }

    C2SpdLogger(std::string name, spdlog::sink_ptr single_sink) : Base(std::move(name), {std::move(single_sink)}) {
    }

    C2SpdLogger(std::string name, spdlog::sinks_init_list sinks) : Base(std::move(name), sinks.begin(), sinks.end()) {
    }

    // Custom interface
    explicit C2SpdLogger(const std::shared_ptr<spdlog::logger>& spdlogger) : Base(*spdlogger) {
    }

    // Log with custom thread_id and timestamp
    void log(std::size_t thread_id, Timestamp timestamp, spdlog::source_loc loc, spdlog::level::level_enum lvl, const std::string& msg) {
        const bool log_enabled = should_log(lvl);
        const bool traceback_enabled = tracer_.enabled();
        if (!log_enabled && !traceback_enabled) {
            return;
        }

        auto log_time = spdlog::log_clock::time_point(std::chrono::duration_cast<spdlog::log_clock::duration>(std::chrono::nanoseconds(timestamp)));

        spdlog::details::log_msg log_msg(log_time, loc, name_, lvl, spdlog::string_view_t{msg});
        log_msg.thread_id = thread_id;
        log_it_(log_msg, log_enabled, traceback_enabled);
    }
};

}  // namespace detail

// ============================================================================
// Initialize Function Declaration
// ============================================================================

// Initialize function declaration (implementation in c2_fastlog.h)
void initialize(std::shared_ptr<detail::C2SpdLogger> logger, LogLevel logging_level, std::chrono::seconds flush_interval);

// spdlog level overload
inline void initialize(std::shared_ptr<detail::C2SpdLogger> logger,
                       spdlog::level::level_enum logging_level,
                       std::chrono::seconds flush_interval = std::chrono::seconds(5)) {
    initialize(std::move(logger), detail::get_level_from_spdlog(logging_level), flush_interval);
}

}  // namespace c2fastlog
