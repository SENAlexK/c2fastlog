#pragma once

#include <memory>

// C2FASTLOG_VERSION 用于区分日志版本。当前后端版本不一致时，前后端使用的 FastLogger::instance()
// 不再是同一对象实例，因此后端将不会读写前端数据。
#define C2FASTLOG_VERSION v1

namespace c2fastlog {
inline namespace C2FASTLOG_VERSION {
namespace detail {

#if defined( __GNUC__ ) || defined( __clang__ )
static inline bool( likely )( bool x )
{
    return __builtin_expect( ( x ), true );
}
static inline bool( unlikely )( bool x )
{
    return __builtin_expect( ( x ), false );
}
#else
static inline bool( likely )( bool x )
{
    return x;
}
static inline bool( unlikely )( bool x )
{
    return x;
}
#endif

#ifdef _WIN32
#define PATH_SPLIT '\\'
#else
#define PATH_SPLIT '/'
#endif

#if !defined( __FILE_NAME__ )
#define __FILE_NAME__                                                                              \
    ( strrchr( __FILE__, PATH_SPLIT ) ? strrchr( __FILE__, PATH_SPLIT ) + 1 : __FILE__ )
#endif

using Timestamp = uint64_t;
#if defined( WIN32 )
static inline Timestamp getTimestampInNano()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch() )
        .count();
}
#else
static inline Timestamp getTimestampInNano()
{
    struct timespec ts;
    clock_gettime( CLOCK_REALTIME, &ts );
    return ts.tv_sec * 1000000000 + ts.tv_nsec;
}
#endif

class c2SPDLogger;

} // namespace detail

enum level : int
{
    trace = 0,
    debug = 1,
    info = 2,
    warn = 3,
    error = 4,
};

template<typename Ostream>
inline Ostream& operator<<( Ostream& stream, const level& data )
{
    switch( data )
    {
        case trace:
            stream << "trace";
            break;
        case debug:
            stream << "debug";
            break;
        case info:
            stream << "info";
            break;
        case warn:
            stream << "warn";
            break;
        case error:
            stream << "error";
            break;
    }
    return stream;
}

} // namespace C2FASTLOG_VERSION
} // namespace c2fastlog