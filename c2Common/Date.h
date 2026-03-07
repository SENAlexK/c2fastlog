#pragma once

#include <cassert>
#include <chrono>

#include "c2Common/Utils.h"

namespace c2trader {

// UTC+12 是以北京时间 20 时为零点的时区，用于解决交易中跨自然日的时间划分问题。
// 假设现在是北京时间(UTC+8) 2025-04-14 20:00，UTC+12 的时间就是：2025-04-15 00:00。

using UTC8Second = int32_t;   // 北京时间 UTC+8 从零点开始计算的秒数
using UTC12Second = int32_t;  // UTC+12 从零点开始计算的秒数
using ExchangeSecond = UTC12Second;

constexpr static int SecondsInMinutes = 60;                        // 一分钟秒数
constexpr static int SecondsInHour = 60 * SecondsInMinutes;        // 一小时秒数
constexpr static int SecondsInDay = 24 * SecondsInHour;            // 一天秒数

constexpr static int HourOfSwitchTradeday = 20;                    // 20 时切换交易日
constexpr static int UTC8 = 8 * SecondsInHour;                     // 北京时间的时区
constexpr static int UTC12 = 12 * SecondsInHour;                   // 以北京时间 20 时为零点的时区
constexpr static int UTC8ToUTC12 = 4 * SecondsInHour;              // 北京时间转交易日时间

inline std::tm localtime( const std::time_t& t ) noexcept
{
#ifdef _WIN32
    std::tm tm;
    ::localtime_s( &tm, &t );
#else
    std::tm tm;
    ::localtime_r( &t, &tm );
#endif
    return tm;
}

// 获取 N 天后自然日，默认返回当前自然日
inline std::tm getNaturalDate( int nextDay = 0 )
{
    auto t = std::chrono::system_clock::to_time_t( std::chrono::system_clock::now() );
    t += ( SecondsInDay * nextDay );
    return localtime( t );
}

// 获取当前交易日。
// 20 时前周一至周五返回当前自然日，周六、周日返回下周一。
// 20 时后周一至周四返回下一自然日，周五、周六、周日返回下周一。
inline std::tm getTradeDate()
{
    auto     today = getNaturalDate();
    std::tm  tradeDate;
    if( today.tm_hour < HourOfSwitchTradeday )
    {
        if( today.tm_wday != 6 && today.tm_wday != 0 )
        {
            tradeDate = today;
        }
        else
        {
            tradeDate = getNaturalDate( ( 7 - today.tm_wday ) % 7 + 1 );
        }
    }
    else // today.tm_hour >= 20
    {
        if( today.tm_wday < 5 )
        {
            tradeDate = getNaturalDate( 1 );
        }
        else
        {
            tradeDate = getNaturalDate( ( 7 - today.tm_wday ) % 7 + 1 );
        }
    }
    return tradeDate;
}

// turnEpochTo12Seconds 将 Unix epoch timestamp 转换为当前交易日开始（北京时间 20 点整）的秒数
inline UTC12Second turnEpochToUTC12Seconds( int32_t epochTimestamp = c2logger::getSecondsTime() )
{
    // 偏移到 UTC+12 时区，然后计算该时区零点开始的秒数。
    return ( epochTimestamp + UTC12 ) % SecondsInDay;
}

inline UTC12Second turnUTC8ToUTC12Seconds( int32_t hours, int32_t minutes = 0, int32_t seconds = 0 )
{
    assert( hours >= 0 && hours <= 24 );
    assert( minutes >= 0 && minutes < 60 );
    assert( seconds >= 0 && seconds < 60 );

    UTC12Second res = hours * SecondsInHour + minutes * SecondsInMinutes + seconds + UTC8ToUTC12;
    if( res > SecondsInDay )
    {
        res -= SecondsInDay;
    }
    return res;
}

// turnEpochToUTC8Seconds 将 Unix epoch timestamp 转换为北京时间零点开始的秒数
inline UTC8Second turnEpochToUTC8Seconds( int32_t epochTimestamp = c2logger::getSecondsTime() )
{
    // 偏移到 UTC+8 时区，然后计算该时区零点开始的秒数。
    return ( epochTimestamp + UTC8 ) % SecondsInDay;
}

inline ExchangeSecond turnEpochToExchangeSeconds( int32_t epochTimestamp = c2logger::getSecondsTime() )
{
    return turnEpochToUTC12Seconds( epochTimestamp );
}

inline ExchangeSecond
turnUTC8ToExchangeSeconds( int32_t hours, int32_t minutes = 0, int32_t seconds = 0 )
{
    return turnUTC8ToUTC12Seconds( hours, minutes, seconds );
}

} // namespace c2trader
