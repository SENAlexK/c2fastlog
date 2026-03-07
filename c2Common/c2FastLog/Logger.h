#pragma once

#include <algorithm>
#include <iostream>

#include "Packer.h"
#include "C2Spdlog.h"
#include "c2Common/queue/VariableQueue.h"

namespace c2fastlog {
inline namespace C2FASTLOG_VERSION {
namespace detail {

#ifndef FASTLOG_THREAD_BUFFER_SIZE
#define FASTLOG_THREAD_BUFFER_SIZE 1 << 24
#endif

// ThreadBuffer 是日志数据的缓冲区
class ThreadBuffer
{
public:
    static constexpr size_t ThreadBufferSize = FASTLOG_THREAD_BUFFER_SIZE;
    using BufferType = c2logger::SPSCVariableQueue<ThreadBufferSize>;

public:
    ThreadBuffer() = default;

    ~ThreadBuffer() = default;

private:
    friend class ThreadBufferDestroyer;
    void release()
    {
        m_shouldDeallocate = true;
    }

private:
    BufferType m_buffer;
    size_t     m_threadID;
    bool       m_shouldDeallocate = false;

public:
    inline BufferType& getBuffer()
    {
        return m_buffer;
    }

    inline void setThreadID( size_t threadID )
    {
        m_threadID = threadID;
    }

    inline size_t getThreadID()
    {
        return m_threadID;
    }

    bool shouldDeallocate()
    {
        return m_shouldDeallocate;
    }
};
inline thread_local std::shared_ptr<ThreadBuffer> t_threadBuffer;

// ThreadBufferDestroyer 是 ThreadBuffer 的销毁类
class ThreadBufferDestroyer
{
public:
    ThreadBufferDestroyer() = default;
    ~ThreadBufferDestroyer()
    {
        if( t_threadBuffer != nullptr )
        {
            t_threadBuffer->release();
            t_threadBuffer = nullptr;
        }
    }
};
inline thread_local ThreadBufferDestroyer t_threadBufferDestroyer;

// StaticLogInfo 是日志的静态数据，在第一次 logging 时生成。
struct StaticLogInfo
{
    typedef std::string ( *FormatFunction )( fmt::string_view, const char* );

    spdlog::source_loc       source;
    spdlog::level::level_enum level;
    fmt::string_view         fmt;
    FormatFunction           fmtFn;
    StaticLogInfo( spdlog::source_loc source,
                   c2fastlog::level   level,
                   fmt::string_view   fmt,
                   FormatFunction     fmtFn )
        : source( source )
        , level( turnLevelToSPDLog( level ) )
        , fmt( fmt )
        , fmtFn( fmtFn )
    {
    }
};

class FastLogger
{
    FastLogger() = default;
    inline ~FastLogger();

public:
    inline static FastLogger& instance();
    inline void               initialize( std::shared_ptr<c2SPDLogger> logger,
                                          level                        loggingLevel = trace,
                                          std::chrono::seconds flushInterval = std::chrono::seconds( 5 ) );
    inline void               release();

private:
    inline void loggingThread();

public:
    inline void loggingLevel( level level )
    {
        m_loggingLevel = level;
    }

    inline level loggingLevel()
    {
        return m_loggingLevel;
    }

private:
    volatile bool m_running = false;
    volatile bool m_checkBufferAgain = false; // 用于 release 时检查所有缓存是否读完
    mutable std::mutex              m_loggingThreadMutex;
    std::thread                     m_loggingThread;
    std::shared_ptr<c2SPDLogger> m_spdLogger; // 保存 spdlog::logger，避免 spdlog 提前析构。
    level                           m_loggingLevel = level::trace;
    std::chrono::seconds            m_flushInterval; // spdlog flush interval

public:
    // allocate 用于分配 ThreadBuffer
    inline void allocate( const size_t threadID );
    // getThreadBuffer 用于获取 ThreadBuffer 实例
    inline auto& getThreadBuffer();
    inline void  threadBufferSizeNotEnough();

private:
    mutable std::mutex                              m_threadBuffersMutex;
    std::vector<std::shared_ptr<ThreadBuffer>>      m_threadBuffers;
    std::vector<std::shared_ptr<ThreadBuffer>>      m_freeThreadBuffers;
    bool                                            m_threadBufferSizeEnough = true;
    static constexpr size_t                         MinFreeThreadBufferSize = 8;
    static constexpr size_t                         MaxFreeThreadBufferSize = 32;

public:
    // registerLogInfo 注册日志的静态数据。
    inline void registerLogInfo( uint32_t& logID, StaticLogInfo staticInfo );

private:
    std::mutex                    m_staticLogInfosMutex;
    std::vector<StaticLogInfo>    m_staticLogInfos;
};

FastLogger::~FastLogger()
{
    release();
}

FastLogger& FastLogger::instance()
{
    static FastLogger logger;
    return logger;
}

void FastLogger::initialize( std::shared_ptr<c2SPDLogger> logger,
                             level                        loggingLevel,
                             std::chrono::seconds         flushInterval )
{
    if( logger == nullptr )
    {
        return;
    }
    this->m_spdLogger = logger;
    this->m_loggingLevel = loggingLevel;
    this->m_flushInterval = flushInterval;

    // 创建日志线程
    std::unique_lock<std::mutex> lock( m_loggingThreadMutex );
    if( m_loggingThread.joinable() )
    {
        m_running = false;
        m_loggingThread.join();
    }
    m_running = true;
    m_loggingThread = std::thread( &FastLogger::loggingThread, &FastLogger::instance() );
}

void FastLogger::release()
{
    if( !m_running )
    {
        return;
    }
    m_checkBufferAgain = true;
    m_running = false;

    std::unique_lock<std::mutex> threadlock( m_loggingThreadMutex );
    if( m_loggingThread.joinable() )
    {
        m_loggingThread.join();
    }

    std::unique_lock<std::mutex> bufferslock( m_threadBuffersMutex );
    m_freeThreadBuffers.clear();
}

// loggingThread 是日志线程的主函数
void FastLogger::loggingThread()
{
    // 首条消息时间戳的小根堆比较函数
    struct
    {
        bool operator()( const std::shared_ptr<ThreadBuffer>& lhv,
                         const std::shared_ptr<ThreadBuffer>& rhv ) const
        {
            Timestamp lTimestamp = std::numeric_limits<Timestamp>::max();
            Timestamp rTimestamp = std::numeric_limits<Timestamp>::max();

            auto lMsg = lhv->getBuffer().front();
            if( lMsg != nullptr )
            {
                lTimestamp = *reinterpret_cast<const Timestamp*>( lMsg->Content );
            }
            auto rMsg = rhv->getBuffer().front();
            if( rMsg != nullptr )
            {
                rTimestamp = *reinterpret_cast<const Timestamp*>( rMsg->Content );
            }
            return lTimestamp > rTimestamp;
        }
    } MsgTimestampCompare;

    auto logInfos = std::vector<StaticLogInfo>();
    auto logger = this->m_spdLogger;
    auto lastFlushTicker = spdlog::details::os::now();
    while( m_running || m_checkBufferAgain )
    {
        m_checkBufferAgain = false;

        // 检查 freeThreadBuffers 是否充足
        if( m_freeThreadBuffers.size() < MinFreeThreadBufferSize )
        {
            // 经测试，分配 128 个 Thread Buffer 约需 2s，
            std::unique_lock<std::mutex> lock( m_threadBuffersMutex );
            while( m_freeThreadBuffers.size() < MaxFreeThreadBufferSize )
            {
                m_freeThreadBuffers.push_back( std::make_shared<ThreadBuffer>() );
            }
        }

        // update static log info
        if( m_staticLogInfos.size() > logInfos.size() )
        {
            std::unique_lock<std::mutex> lock( m_staticLogInfosMutex );
            for( auto i = logInfos.size(); i < m_staticLogInfos.size(); i++ )
            {
                logInfos.push_back( m_staticLogInfos.at( i ) );
            }
        }

        // logging
        std::unique_lock<std::mutex> lock( m_threadBuffersMutex );
        // 释放对应的线程已经结束的 threadBuffer。
        for( size_t i = 0; i < m_threadBuffers.size(); )
        {
            auto tb = m_threadBuffers[i];
            if( tb->getBuffer().front() == nullptr && tb->shouldDeallocate() )
            {
                m_threadBuffers[i] = m_threadBuffers.back();
                m_threadBuffers.pop_back();
            }
            else
            {
                i++;
            }
        }
        auto startConsume = getTimestampInNano();
        // 用首条消息的时间戳将 m_threadBuffers 构建为堆结构。
        std::make_heap( m_threadBuffers.begin(), m_threadBuffers.end(), MsgTimestampCompare );
        while( !m_threadBuffers.empty() && m_threadBuffers.front()->getBuffer().front() != nullptr )
        {
            auto  tb = m_threadBuffers.front();
            auto& buffer = tb->getBuffer();
            auto  msg = buffer.front();
            auto  content = reinterpret_cast<const char*>( msg->Content );
            auto& timestamp = *reinterpret_cast<const Timestamp*>( content );
            if( timestamp > startConsume || msg->Type > logInfos.size() )
            {
                m_checkBufferAgain = true;
                break;
            }
            auto& logInfo = logInfos[msg->Type - 1];
            logger->log( tb->getThreadID(),
                         timestamp,
                         logInfo.source,
                         logInfo.level,
                         logInfo.fmtFn( logInfo.fmt, content + sizeof( Timestamp ) ) );
            buffer.readCommit();
            // 堆底的 ThreadBuffer 的首条消息已经变更，重新维护 m_threadBuffers 的堆结构。
            std::pop_heap( m_threadBuffers.begin(), m_threadBuffers.end(), MsgTimestampCompare );
            std::push_heap( m_threadBuffers.begin(), m_threadBuffers.end(), MsgTimestampCompare );
        }
        lock.unlock();

        if( !m_threadBufferSizeEnough )
        {
            static std::once_flag flag;
            std::call_once( flag, [&]() {
                logger->log( spdlog::details::os::thread_id(),
                             getTimestampInNano(),
                             spdlog::source_loc{ __FILE_NAME__, __LINE__, __func__ },
                             spdlog::level::err,
                             "FastLog Thread Buffer Size Not Enough!" );
            } );
        }

        // 主动调用 spdlog::flush 刷新缓存
        if( spdlog::details::os::now() - lastFlushTicker > this->m_flushInterval )
        {
            logger->flush();
            lastFlushTicker = spdlog::details::os::now();
        }
        std::this_thread::sleep_for( std::chrono::microseconds( 1 ) );
    }
}

void FastLogger::allocate( const size_t threadID )
{
    if( t_threadBuffer != nullptr )
    {
        return;
    }

    // add to m_threadBuffers
    std::unique_lock<std::mutex> lock( m_threadBuffersMutex );
    if( !m_freeThreadBuffers.empty() )
    {
        t_threadBuffer = m_freeThreadBuffers.back();
        m_freeThreadBuffers.pop_back();
    }
    else
    {
        t_threadBuffer = std::make_shared<ThreadBuffer>();
    }
    t_threadBuffer->setThreadID( threadID );
    m_threadBuffers.push_back( t_threadBuffer );
};

auto& FastLogger::getThreadBuffer()
{
    return t_threadBuffer->getBuffer();
}

void FastLogger::threadBufferSizeNotEnough()
{
    m_threadBufferSizeEnough = false;
}

void FastLogger::registerLogInfo( uint32_t& logID, StaticLogInfo staticInfo )
{
    if( logID != 0 )
    {
        return;
    }
    std::unique_lock<std::mutex> lock( m_staticLogInfosMutex );
    logID = m_staticLogInfos.size() + 1;
    m_staticLogInfos.push_back( std::move( staticInfo ) );
}

} // namespace detail
} // namespace C2FASTLOG_VERSION
} // namespace c2fastlog

