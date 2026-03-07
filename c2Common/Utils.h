#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <ctime>
#include <sys/file.h>

namespace c2logger {

constexpr size_t CacheLineSize = 64;

constexpr uint64_t NanosPerSecond  = 1'000'000'000ULL;
constexpr uint64_t NanosPerMicro   = 1'000ULL;
constexpr uint64_t MicrosPerSecond = 1'000'000ULL;

#if defined( WIN32 ) || defined( _WIN32 ) || defined( WIN64 ) || defined( _WIN64 ) || \
    defined( _WINDOWS )
static inline bool( likely )( bool x )
{
    return x;
}
static inline bool( unlikely )( bool x )
{
    return x;
}
#else
static inline bool( likely )( bool x )
{
    return __builtin_expect( ( x ), true );
}
static inline bool( unlikely )( bool x )
{
    return __builtin_expect( ( x ), false );
}
#endif

#if defined( WIN32 ) || defined( _WIN32 ) || defined( WIN64 ) || defined( _WIN64 ) || \
    defined( _WINDOWS )
#define force_inline __forceinline
#else
#define force_inline __attribute__( ( always_inline ) )
#endif

#if defined( WIN32 ) || defined( _WIN32 ) || defined( WIN64 ) || defined( _WIN64 ) || \
    defined( _WINDOWS )
static inline uint64_t getNanoTime()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch() )
        .count();
}

static inline uint64_t getMicroTime()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch() )
        .count();
}

static inline uint64_t getSecondsTime()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch() )
        .count();
}
#else

static inline uint64_t readTimeStampCounter()
{
    uint32_t lo, hi;
    __asm__ volatile( "rdtsc" : "=a"( lo ), "=d"( hi ) );
    return static_cast<uint64_t>( lo ) | ( static_cast<uint64_t>( hi ) << 32 );
}

static inline uint64_t getNanoTime()
{
    struct timespec ts;
    clock_gettime( CLOCK_REALTIME, &ts );
    return static_cast<uint64_t>( ts.tv_sec ) * NanosPerSecond +
           static_cast<uint64_t>( ts.tv_nsec );
}

static inline uint64_t getMicroTime()
{
    struct timespec ts;
    clock_gettime( CLOCK_REALTIME, &ts );
    return static_cast<uint64_t>( ts.tv_sec ) * MicrosPerSecond +
           static_cast<uint64_t>( ts.tv_nsec ) / NanosPerMicro;
}

static inline uint64_t getSecondsTime()
{
    struct timespec ts;
    clock_gettime( CLOCK_REALTIME, &ts );
    return ts.tv_sec;
}
#endif

static inline void nanoSleep( uint64_t duration )
{
    uint64_t t1 = getNanoTime(), t2 = 0;
    do
    {
        t2 = getNanoTime();
    } while( t2 - t1 < duration );
    return;
}

inline bool isProcessRunning( const char* processName )
{
    std::string lockFile = std::string( processName ).append( ".lock" );
    int         lockFd = open( lockFile.data(), O_CREAT | O_RDWR, 0666 );
    int         rc = flock( lockFd, LOCK_EX | LOCK_NB );
    if( rc )
    {
        if( EWOULDBLOCK == errno )
        {
            printf( "Main process %s is already running, exit !!! ", processName );
        }
        else
        {
            printf( "Check single instance error: %s , check your permission",
                    std::strerror( errno ) );
        }
        return true;
    }
    return false;
}

} // namespace c2logger
