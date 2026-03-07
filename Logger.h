#pragma once

// Logger.h 提供三种日志库：c2fastlog、fmt::print 和 spdlog。没有启用任何编译标志时，Logger.h 将使用
// fmt::print。当在 CMakeLists.txt 中使用 target_compile_definitions 启用编译标志 ENABLE_X2FASTLOG
// 且不启用编译标志 FORCE_SPDLOG 时，Logger.h 将使用 c2fastlog。当在 CMakeLists.txt 中使用
// target_compile_definitions 启用编译标志 FORCE_SPDLOG 时，无论是否启用编译标志 ENABLE_X2FASTLOG，
// Logger.h 都将使用 spdlog。

#include <iomanip>
#include <iostream>
#include <map>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <vector>

#include "c2Common/Date.h"
#include "c2Common/Utils.h"

// "trace", "debug", "info", "warning", "error", "critical", "off"
#define DEFAULT_LOG_LEVEL_NAME "info"
#define DEFAULT_LOG_PATTERN "[%m/%d %T.%F] [%^%=81%$] [%6P/%-6t] [%@] [%!] %v"

// 默认使用 c2fastlog 或 fmt::print，backtest 使用 spdlog。
#if defined( FORCE_SPDLOG ) || defined( X2TRADER_BACKTEST ) || defined( X2TRADER_BACKTEST_MT )
#define USE_SPDLOG
#elif defined( ENABLE_X2FASTLOG )
#define USE_X2FASTLOG
#else
#define USE_FMTPRINT
#endif

#if defined( USE_SPDLOG )

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace x2trader {
// SpdlogHandle 用于保存自行创建的 spdlog::logger。
// ATQ 2.0 版本将日志库切换为 spdlog，且在 release 时主动调用 spdlog::shutdown 关闭
// spdlog::logger。因此，使用 spdlog 时需要注意：
// 1. 使用 SpdlogHandle 保存的 spdlog::logger 输出日志，不能直接使用 spdlog::default_logger。
// 2. 在 X2ModelBase::OnRelease 后可能被调用的函数中不能使用 spdlog。
class SpdlogHandle
{
private:
    SpdlogHandle() = default;

public:
    static SpdlogHandle& instance()
    {
        static SpdlogHandle logger;
        return logger;
    }

    void setLogger( std::shared_ptr<spdlog::logger> logger )
    {
        std::cout << "Set spdlog logger! Old logger name: " << m_logger->name() << std::endl;
        std::unique_lock lock( m_mtx );
        m_logger = logger;
    }

    std::shared_ptr<spdlog::logger> getLogger()
    {
        std::shared_lock lock( m_mtx );
        return m_logger;
    }

private:
    // todo: read write mutex
    std::shared_mutex               m_mtx;
    std::shared_ptr<spdlog::logger> m_logger = spdlog::default_logger();
};
} // namespace x2trader

#elif defined( USE_X2FASTLOG )

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

// 当前生产环境中，一秒内报单峰值为 1k，队列内订单数峰值为 100，因此线程 buffer 使用默认值。
// #define FASTLOG_THREAD_BUFFER_SIZE 1 << 20
#include "c2Common/c2FastLog/FastLog.h"

#define FAST_LOG(level, format, ...) c2fastlog::FastLog(0, __FILE__, __LINE__, __FUNCTION__, level, format, ##__VA_ARGS__)
#define FAST_LOG_DISCARD_OLDEST(level, format, ...) c2fastlog::FastLogDiscardOldest(0, __FILE__, __LINE__, __FUNCTION__, level, format, ##__VA_ARGS__)

#elif defined( USE_FMTPRINT )

#include <spdlog/common.h>
#include <spdlog/fmt/chrono.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/ostr.h>

// fmtlib 10.0.0 后支持了 fmt::println 接口。考虑到兼容性问题，暂时继续沿用 spdlog 内置的
// fmtlib 7.0.3，并在 namespace x2trader 中支持 println 接口。
namespace x2trader {

static inline spdlog::level::level_enum LOG_LEVEL = spdlog::level::trace;

template<typename S, typename... T>
inline void println( spdlog::level::level_enum level, const S& format_str, T&&... args )
{
    if( level < LOG_LEVEL )
    {
        return;
    }
    auto now = getNanoTime();
    auto time = std::chrono::system_clock::to_time_t( std::chrono::system_clock::time_point(
        std::chrono::system_clock::duration( std::chrono::seconds( now / 1'000'000'000 ) ) ) );
    auto nanosecond = now % 1'000'000'000;
    return fmt::print( "[{:%m/%d %H:%M:%S}.{:09}] [{:^8}] {}\n",
                       *std::localtime( &time ),
                       nanosecond,
                       spdlog::level::to_string_view( level ),
                       fmt::format( format_str, std::forward<T>( args )... ) );
}

} // namespace c2trader

#endif

#if defined( USE_SPDLOG )
#define LOG_INFO( ... )  SPDLOG_LOGGER_INFO( x2trader::SpdlogHandle::instance().getLogger(), __VA_ARGS__ )
#define LOG_TRACE( ... ) SPDLOG_LOGGER_TRACE( x2trader::SpdlogHandle::instance().getLogger(), __VA_ARGS__ )
#define LOG_DEBUG( ... ) SPDLOG_LOGGER_DEBUG( x2trader::SpdlogHandle::instance().getLogger(), __VA_ARGS__ )
#define LOG_WARN( ... )  SPDLOG_LOGGER_WARN( x2trader::SpdlogHandle::instance().getLogger(), __VA_ARGS__ )
#define LOG_ERROR( ... ) SPDLOG_LOGGER_ERROR( x2trader::SpdlogHandle::instance().getLogger(), __VA_ARGS__ )
#elif defined( USE_X2FASTLOG )
#define LOG_TRACE( ... ) FAST_LOG( c2fastlog::level::trace, __VA_ARGS__ )
#define LOG_DEBUG( ... ) FAST_LOG( c2fastlog::level::debug, __VA_ARGS__ )
#define LOG_INFO( ... )  FAST_LOG( c2fastlog::level::info, __VA_ARGS__ )
#define LOG_WARN( ... )  FAST_LOG( c2fastlog::level::warn, __VA_ARGS__ )
#define LOG_ERROR( ... ) FAST_LOG( c2fastlog::level::error, __VA_ARGS__ )
// 丢弃最旧日志策略的宏 (buffer满时丢弃最旧的日志,保留最新日志)
// 警告: 仅适用于单线程生产者或确保后台消费线程暂停的场景
#define LOG_TRACE_DISCARD_OLDEST( ... ) FAST_LOG_DISCARD_OLDEST( c2fastlog::level::trace, __VA_ARGS__ )
#define LOG_DEBUG_DISCARD_OLDEST( ... ) FAST_LOG_DISCARD_OLDEST( c2fastlog::level::debug, __VA_ARGS__ )
#define LOG_INFO_DISCARD_OLDEST( ... )  FAST_LOG_DISCARD_OLDEST( c2fastlog::level::info, __VA_ARGS__ )
#define LOG_WARN_DISCARD_OLDEST( ... )  FAST_LOG_DISCARD_OLDEST( c2fastlog::level::warn, __VA_ARGS__ )
#define LOG_ERROR_DISCARD_OLDEST( ... ) FAST_LOG_DISCARD_OLDEST( c2fastlog::level::error, __VA_ARGS__ )
#elif defined( USE_FMTPRINT )
#define LOG_TRACE( ... ) x2trader::println( spdlog::level::trace, __VA_ARGS__ )
#define LOG_DEBUG( ... ) x2trader::println( spdlog::level::debug, __VA_ARGS__ )
#define LOG_INFO( ... )  x2trader::println( spdlog::level::info, __VA_ARGS__ )
#define LOG_WARN( ... )  x2trader::println( spdlog::level::warn, __VA_ARGS__ )
#define LOG_ERROR( ... ) x2trader::println( spdlog::level::err, __VA_ARGS__ )
#endif

namespace x2trader {

inline std::string addTradeDayIntoLogFileName( std::string file_name )
{
    std::string base_name, ext_name;
    auto        ext_index = file_name.rfind( '.' );
    if( ext_index == std::string::npos || ext_index == 0 || ext_index == file_name.size() - 1 )
    {
        base_name = file_name;
    }
    else
    {
        base_name = file_name.substr( 0, ext_index );
        ext_name = file_name.substr( ext_index );
    }

    auto              trade_date = c2trader::getTradeDate();
    std::stringstream ss;
    ss << base_name << "_" << std::put_time( &trade_date, "%F" ) << ext_name;
    return ss.str();
}

/// 非默认日志相关定义 - end
inline void setupLog( std::string          log_level_name = DEFAULT_LOG_LEVEL_NAME,
                      [[maybe_unused]] std::string log_file = "x2trader.log",
                      std::string          log_name = "x2trader",
                      [[maybe_unused]] int flush_interval = 5 )
{
    auto log_level = spdlog::level::from_str( log_level_name );
    if( log_level == spdlog::level::off )
    {
        std::cout << "[ warning ] The log level is set to off" << std::endl;
    }

#if defined( USE_SPDLOG )
    auto daily_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        addTradeDayIntoLogFileName( log_file ) );
#ifndef NDEBUG
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level( spdlog::level::warn );
    spdlog::sinks_init_list log_sinks = { console_sink, daily_sink };
#else
    spdlog::sinks_init_list log_sinks = { daily_sink };
#endif
    auto logger = std::make_shared<spdlog::logger>( log_name, log_sinks );
    logger->set_pattern( DEFAULT_LOG_PATTERN );
    logger->set_level( log_level );
    logger->flush_on( spdlog::level::warn );
    // 使用 SpdlogHandle 保存 logger，不能将 logger 保存到 spdlog::set_default_logger 中。
    SpdlogHandle::instance().setLogger( logger );
    LOG_INFO( "Setup spdlog for {}", log_name );

#elif defined( USE_X2FASTLOG )
    auto daily_sink = std::make_shared<spdlog::sinks::basic_file_sink_st>(
        addTradeDayIntoLogFileName( log_file ) );
#ifndef NDEBUG
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
    console_sink->set_level( spdlog::level::warn );
    spdlog::sinks_init_list log_sinks = { console_sink, daily_sink };
#else
    spdlog::sinks_init_list log_sinks = { daily_sink };
#endif
    auto logger = std::make_shared<c2fastlog::detail::c2SPDLogger>( log_name, log_sinks );
    c2fastlog::Initialize( logger, log_level, std::chrono::seconds( flush_interval ) );
    logger->set_pattern( DEFAULT_LOG_PATTERN );
    logger->set_level( log_level );
    logger->flush_on( spdlog::level::warn );
    LOG_INFO( "Setup fastlog for {}", log_name );

#elif defined( USE_FMTPRINT )
    // fmt::print 无需初始化
    LOG_LEVEL = log_level;
    LOG_INFO( "Setup fmt::print for {}", log_name );
#endif
}

} // namespace x2trader

