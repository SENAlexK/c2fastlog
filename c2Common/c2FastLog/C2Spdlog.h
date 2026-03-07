// C2Spdlog.h 包含了 C2FastLog 对 spdlog 的修改。

#pragma once

// spdlog 调优宏，参见 spdlog/tweakme.h
// 以下 spdlog 宏需要在引入 spdlog 的头文件前定义。

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include "Common.h"

namespace c2fastlog {
inline namespace C2FASTLOG_VERSION {
namespace detail {

inline level getLevelFromSPDLog( spdlog::level::level_enum spdLevel )
{
    level level = trace;
    switch( spdLevel )
    {
        case spdlog::level::trace:
            level = trace;
            break;

        case spdlog::level::debug:
            level = debug;
            break;

        case spdlog::level::info:
            level = info;
            break;

        case spdlog::level::warn:
            level = warn;
            break;

        case spdlog::level::err:
        default:
            level = error;
            break;
    }
    return level;
}

inline spdlog::level::level_enum turnLevelToSPDLog( level level )
{
    auto spdLevel = spdlog::level::trace;
    switch( level )
    {
        case trace:
            spdLevel = spdlog::level::trace;
            break;

        case debug:
            spdLevel = spdlog::level::debug;
            break;

        case info:
            spdLevel = spdlog::level::info;
            break;

        case warn:
            spdLevel = spdlog::level::warn;
            break;

        case error:
        default:
            spdLevel = spdlog::level::err;
            break;
    }
    return spdLevel;
}

// c2SPDLogger 是 C2FastLog 定制的 spdlog::logger。
// 由于 C2FastLog 实现了异步操作，因此 c2SPDLogger 仅需支持同步 logging。
class c2SPDLogger : public spdlog::logger
{
    using Base = spdlog::logger;

public:
    // 转发基类函数
    explicit c2SPDLogger( std::string name )
        : Base( std::move( name ) )
    {
    }

    template<typename It>
    c2SPDLogger( std::string name, It begin, It end )
        : Base( std::move( name ), begin, end )
    {
    }

    c2SPDLogger( std::string name, spdlog::sink_ptr single_sink )
        : Base( std::move( name ), { std::move( single_sink ) } )
    {
    }

    c2SPDLogger( std::string name, spdlog::sinks_init_list sinks )
        : Base( std::move( name ), sinks.begin(), sinks.end() )
    {
    }

public:
    // 自定义接口
    c2SPDLogger( const std::shared_ptr<spdlog::logger>& spdlogger )
        : Base( *spdlogger )
    {
    }

    void log( size_t                    thread_id,
              Timestamp                 timestamp,
              spdlog::source_loc        loc,
              spdlog::level::level_enum lvl,
              const std::string&        msg )
    {
        bool log_enabled = should_log( lvl );
        bool traceback_enabled = tracer_.enabled();
        if( !log_enabled && !traceback_enabled )
        {
            return;
        }
        spdlog::log_clock::time_point log_time =
            std::chrono::time_point<spdlog::log_clock, typename spdlog::log_clock::duration>(
                std::chrono::duration_cast<typename spdlog::log_clock::duration>(
                    std::chrono::nanoseconds( timestamp ) ) );
        spdlog::details::log_msg log_msg( log_time, loc, name_, lvl, spdlog::string_view_t{ msg } );
        log_msg.thread_id = thread_id;
        log_it_( log_msg, log_enabled, traceback_enabled );
    }
};

// 考虑到生产环境中其他线程可能修改 spdlog::default_logger，因此删去 getDefaultLogger()
// // getDefaultLogger 返回使用 spdlog::default_logger 构造的 x2SPDLogger
// inline std::shared_ptr<x2SPDLogger> getDefaultLogger()
// {
//     static auto logger = std::make_shared<x2SPDLogger>( spdlog::default_logger() );
//     return logger;
// }

} // namespace detail

// Initialize 函数声明 (实现在 FastLog.h 中)
void Initialize( std::shared_ptr<detail::c2SPDLogger> logger,
                 level                                loggingLevel,
                 std::chrono::seconds                 flushInterval );

// spdlog 版 Initialize 重载，接收 spdlog::level::level_enum
inline void Initialize( std::shared_ptr<detail::c2SPDLogger> logger,
                        spdlog::level::level_enum            loggingLevel,
                        std::chrono::seconds                 flushInterval = std::chrono::seconds( 5 ) )
{
    return Initialize( logger, detail::getLevelFromSPDLog( loggingLevel ), flushInterval );
}

} // namespace C2FASTLOG_VERSION
} // namespace c2fastlog
