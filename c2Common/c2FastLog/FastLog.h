#pragma once

#include <thread>

#include "Logger.h"

namespace c2fastlog {
inline namespace C2FASTLOG_VERSION {

inline void Initialize( std::shared_ptr<detail::c2SPDLogger> logger,
                        level                                loggingLevel,
                        std::chrono::seconds                 flushInterval )
{
    detail::FastLogger::instance().initialize( logger, loggingLevel, flushInterval );
}

inline void Preallocate()
{
    detail::FastLogger::instance().allocate( spdlog::details::os::thread_id() );
}

inline void Release()
{
    detail::FastLogger::instance().release();
}

template<typename... Args>
inline void FastLog( uint32_t          logID,
                     const char*       fileName,
                     const int         line,
                     const char*       funcName,
                     const level       level,
                     std::string_view  formatString,
                     Args&&... args )
{
    // 在前端按日志等级过滤日志。
    if( detail::unlikely( level < detail::FastLogger::instance().loggingLevel() ) )
    {
        return;
    }
    // 首次打日志，分配 ThreadBuffer 并注册静态信息。
    if( logID == 0 )
    {
        Preallocate();
        detail::FastLogger::instance().registerLogInfo(
            logID,
            detail::StaticLogInfo{ spdlog::source_loc{ fileName, line, funcName },
                                   level,
                                   formatString,
                                   detail::unpackAndFormat<sizeof...( args ), Args...> } );
    }
    // 当前日志数据由时间戳和日志参数组
    auto&  buffer  = detail::FastLogger::instance().getThreadBuffer();
    size_t msgSize = sizeof( detail::Timestamp ) + detail::calculateSize( args... );
    auto   ptr     = buffer.allocate( msgSize );
    // buffer 满时自旋等待，不丢弃日志
    while( detail::unlikely( ptr == nullptr ) )
    {
        std::this_thread::yield();
        ptr = buffer.allocate( msgSize );
    }
    ptr->Type = logID;
    ptr->Size = msgSize;
    auto content = ptr->Content;
    *( reinterpret_cast<detail::Timestamp*>( content ) ) = detail::getTimestampInNano();
    content += sizeof( detail::Timestamp );
    detail::pack( reinterpret_cast<char*>( content ), args... );
    buffer.writeCommit();
    return;
}

// 丢弃最旧日志策略的日志函数
// 警告: 此函数在 buffer 满时会丢弃最旧的日志，不适合需要完整日志的场景
// 适用场景: 高频交易系统中，保留最新日志比完整性更重要
template<typename... Args>
inline void FastLogDiscardOldest( uint32_t          logID,
                                  const char*       fileName,
                                  const int         line,
                                  const char*       funcName,
                                  const level       level,
                                  std::string_view  formatString,
                                  Args&&... args )
{
    // 在前端按日志等级过滤日志。
    if( detail::unlikely( level < detail::FastLogger::instance().loggingLevel() ) )
    {
        return;
    }
    // 首次打日志，分配 ThreadBuffer 并注册静态信息。
    if( logID == 0 )
    {
        Preallocate();
        detail::FastLogger::instance().registerLogInfo(
            logID,
            detail::StaticLogInfo{ spdlog::source_loc{ fileName, line, funcName },
                                   level,
                                   formatString,
                                   detail::unpackAndFormat<sizeof...( args ), Args...> } );
    }
    // 当前日志数据由时间戳和日志参数组
    auto&  buffer  = detail::FastLogger::instance().getThreadBuffer();
    size_t msgSize = sizeof( detail::Timestamp ) + detail::calculateSize( args... );
    // 使用丢弃最旧策略分配空间
    auto ptr = buffer.allocateDiscardOldest( msgSize );
    if( detail::unlikely( ptr == nullptr ) )
    {
        // 消息太大，无法放入队列
        return;
    }
    ptr->Type = logID;
    ptr->Size = msgSize;
    auto content = ptr->Content;
    *( reinterpret_cast<detail::Timestamp*>( content ) ) = detail::getTimestampInNano();
    content += sizeof( detail::Timestamp );
    detail::pack( reinterpret_cast<char*>( content ), args... );
    buffer.writeCommit();
    return;
}

} // namespace C2FASTLOG_VERSION
} // namespace c2fastlog
