#pragma once

#include <cstdint>

#include <spdlog/fmt/fmt.h>

#include "Common.h"

namespace c2fastlog {
inline namespace C2FASTLOG_VERSION {
namespace detail {

// getSize 获取 arg 打包后的 size
template<typename Arg>
inline constexpr size_t getSize( Arg&& arg )
{
    size_t       size = 0;
    constexpr auto type = fmt::detail::mapped_type_constant<Arg, fmt::format_context>::value;
    if constexpr( type == fmt::detail::type::cstring_type )
    {
        size = sizeof( size_t ) + strlen( arg ) + 1;
    }
    else if constexpr( type == fmt::detail::type::string_type )
    {
        size = sizeof( size_t ) + arg.size() + 1;
    }
    else
    {
        size = sizeof( Arg );
    }
    return size;
}

// calculateSize 计算 args 打包后的总 size
inline constexpr size_t calculateSize()
{
    return 0;
}

template<typename Arg, typename... Args>
inline constexpr size_t calculateSize( Arg&& arg, Args&&... args )
{
    return getSize( arg ) + calculateSize( args... );
}

// pack 打包 args 并写入到 out 中
inline constexpr void pack( char* )
{
    return;
}

template<typename Arg, typename... Args>
inline void pack( char* out, Arg&& arg, Args&&... args )
{
    using ArgType = std::remove_cv_t<std::remove_reference_t<Arg>>;
    size_t         size = getSize( arg );
    constexpr auto type = fmt::detail::mapped_type_constant<Arg, fmt::format_context>::value;
    if constexpr( type == fmt::detail::type::cstring_type )
    {
        auto length = size - sizeof( size_t );
        memcpy( out, &length, sizeof( size_t ) );
        memcpy( out + sizeof( size_t ), arg, length );
    }
    else if constexpr( type == fmt::detail::type::string_type )
    {
        auto length = size - sizeof( size_t );
        memcpy( out, &length, sizeof( size_t ) );
        memcpy( out + sizeof( size_t ), arg.data(), length - 1 );
        out[size - 1] = '\0';
    }
    else
    {
        if constexpr( std::is_trivially_copyable_v<Arg> )
        {
            memcpy( out, &arg, size );
        }
        else if constexpr( std::is_copy_constructible<ArgType>::value )
        {
            new( out ) ArgType( std::forward<Arg>( arg ) );
        }
        else if constexpr( std::is_copy_assignable<ArgType>::value )
        {
            *static_cast<ArgType*>( out ) = arg;
        }
        else
        {
            static_assert( std::is_copy_constructible<ArgType>::value ||
                           std::is_copy_assignable<ArgType>::value,
                           "Untrivially type need copy constructor or copy assign!" );
        }
    }
    return pack( out + size, std::forward<Args>( args )... );
}

using format_arg = fmt::basic_format_args<fmt::format_context>::format_arg;

// unpack 解包并写入 args 中
template<std::size_t I, std::size_t Size>
inline typename std::enable_if<I == Size, void>::type unpack( std::array<format_arg, Size>&,
                                                              const char* )
{
    return;
}

template<std::size_t I, std::size_t Size, typename Arg, typename... Args>
inline void unpack( std::array<format_arg, Size>& args, const char* in )
{
    using ArgType = fmt::remove_cvref_t<Arg>;
    size_t         size;
    constexpr auto type = fmt::detail::mapped_type_constant<Arg, fmt::format_context>::value;
    if constexpr( type == fmt::detail::type::cstring_type ||
                  type == fmt::detail::type::string_type )
    {
        auto length = *reinterpret_cast<const size_t*>( in );
        size = length + sizeof( size_t );
        fmt::string_view value( in + sizeof( size_t ), length - 1 );
        args[I] = fmt::detail::make_arg<fmt::format_context>( value );
    }
    else
    {
        auto value = reinterpret_cast<const ArgType*>( in );
        size = getSize( *value );
        args[I] = fmt::detail::make_arg<fmt::format_context>( *value );
    }
    return unpack<I + 1, Size, Args...>( args, in + size );
}

// destruct 对在 pack 函数中使用 placement new 的对象调用析构函数
template<std::size_t I, std::size_t Size>
inline typename std::enable_if<I == Size, void>::type destruct( const char* )
{
    return;
}

template<std::size_t I, std::size_t Size, typename Arg, typename... Args>
inline void destruct( const char* in )
{
    using ArgType = fmt::remove_cvref_t<Arg>;
    size_t         size;
    constexpr auto type = fmt::detail::mapped_type_constant<Arg, fmt::format_context>::value;
    if constexpr( type == fmt::detail::type::cstring_type ||
                  type == fmt::detail::type::string_type )
    {
        size = *reinterpret_cast<const size_t*>( in ) + sizeof( size_t );
    }
    else
    {
        auto value = reinterpret_cast<const ArgType*>( in );
        size = getSize( *value );
        if constexpr( !std::is_trivially_copyable_v<Arg> )
        {
            value->~ArgType();
        }
    }
    return destruct<I + 1, Size, Args...>( in + size );
}

// unpackAndFormat 使用
template<std::size_t Size, typename... Args>
inline std::string unpackAndFormat( fmt::string_view formatStr, const char* in )
{
    auto args = std::array<format_arg, Size>();
    unpack<0, Size, Args...>( args, in );
    auto&& log = fmt::vformat( formatStr, fmt::format_args( args.data(), Size ) );
    destruct<0, Size, Args...>( in );
    return std::move( log );
}

} // namespace detail
} // namespace C2FASTLOG_VERSION
} // namespace c2fastlog

