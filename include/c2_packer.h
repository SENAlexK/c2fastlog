#pragma once

#include "c2_common.h"

#include <cstdint>
#include <cstring>

#include <array>
#include <concepts>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

#include <spdlog/fmt/fmt.h>

namespace c2fastlog {
namespace detail {

// ============================================================================
// Size Calculation
// ============================================================================

// Get the packed size of a single argument
template <typename Arg>
[[nodiscard]] constexpr std::size_t get_size(Arg&& arg) noexcept {
    constexpr auto type = fmt::detail::mapped_type_constant<Arg, fmt::format_context>::value;
    if constexpr (type == fmt::detail::type::cstring_type) {
        return sizeof(std::size_t) + std::strlen(arg) + 1;
    } else if constexpr (type == fmt::detail::type::string_type) {
        return sizeof(std::size_t) + arg.size() + 1;
    } else {
        return sizeof(std::decay_t<Arg>);
    }
}

// Calculate the total packed size of all arguments (base case)
[[nodiscard]] constexpr std::size_t calculate_size() noexcept {
    return 0;
}

// Calculate the total packed size of all arguments (recursive case)
template <typename Arg, typename... Args>
[[nodiscard]] constexpr std::size_t calculate_size(Arg&& arg, Args&&... args) noexcept {
    return get_size(std::forward<Arg>(arg)) + calculate_size(std::forward<Args>(args)...);
}

// ============================================================================
// Packing
// ============================================================================

// Pack arguments into buffer (base case)
constexpr void pack(char*) noexcept {
}

// Pack arguments into buffer (recursive case)
template <typename Arg, typename... Args>
void pack(char* out, Arg&& arg, Args&&... args) noexcept {
    using ArgType = std::remove_cvref_t<Arg>;
    const std::size_t size = get_size(std::forward<Arg>(arg));
    constexpr auto type = fmt::detail::mapped_type_constant<Arg, fmt::format_context>::value;

    if constexpr (type == fmt::detail::type::cstring_type) {
        const auto length = size - sizeof(std::size_t);
        std::memcpy(out, &length, sizeof(std::size_t));
        std::memcpy(out + sizeof(std::size_t), arg, length);
    } else if constexpr (type == fmt::detail::type::string_type) {
        const auto length = size - sizeof(std::size_t);
        std::memcpy(out, &length, sizeof(std::size_t));
        std::memcpy(out + sizeof(std::size_t), arg.data(), length - 1);
        out[size - 1] = '\0';
    } else {
        if constexpr (std::is_trivially_copyable_v<ArgType>) {
            const auto* src = reinterpret_cast<const char*>(std::addressof(arg));
            std::memcpy(out, src, sizeof(ArgType));
        } else if constexpr (std::is_copy_constructible_v<ArgType>) {
            new (out) ArgType(std::forward<Arg>(arg));
        } else if constexpr (std::is_copy_assignable_v<ArgType>) {
            *reinterpret_cast<ArgType*>(out) = arg;
        } else {
            static_assert(std::is_copy_constructible_v<ArgType> || std::is_copy_assignable_v<ArgType>,
                          "Non-trivial type requires copy constructor or copy assignment!");
        }
    }
    pack(out + size, std::forward<Args>(args)...);
}

// ============================================================================
// Unpacking
// ============================================================================

using FormatArg = fmt::basic_format_args<fmt::format_context>::format_arg;

// Unpack arguments from buffer (base case)
template <std::size_t I, std::size_t SIZE>
auto unpack(std::array<FormatArg, SIZE>&, const char*) noexcept -> std::enable_if_t<I == SIZE> {
}

// Unpack arguments from buffer (recursive case)
template <std::size_t I, std::size_t SIZE, typename Arg, typename... Args>
void unpack(std::array<FormatArg, SIZE>& args, const char* in) noexcept {
    using ArgType = fmt::remove_cvref_t<Arg>;
    std::size_t size;
    constexpr auto type = fmt::detail::mapped_type_constant<Arg, fmt::format_context>::value;

    if constexpr (type == fmt::detail::type::cstring_type || type == fmt::detail::type::string_type) {
        const auto length = *reinterpret_cast<const std::size_t*>(in);
        size = length + sizeof(std::size_t);
        fmt::string_view value(in + sizeof(std::size_t), length - 1);
        args[I] = FormatArg(value);
    } else {
        const auto* value = reinterpret_cast<const ArgType*>(in);
        size = get_size(*value);
        args[I] = FormatArg(*value);
    }
    unpack<I + 1, SIZE, Args...>(args, in + size);
}

// ============================================================================
// Destruction
// ============================================================================

// Destruct non-trivial objects created via placement new (base case)
template <std::size_t I, std::size_t SIZE>
auto destruct(const char*) noexcept -> std::enable_if_t<I == SIZE> {
}

// Destruct non-trivial objects created via placement new (recursive case)
template <std::size_t I, std::size_t SIZE, typename Arg, typename... Args>
void destruct(const char* in) noexcept {
    using ArgType = fmt::remove_cvref_t<Arg>;
    std::size_t size;
    constexpr auto type = fmt::detail::mapped_type_constant<Arg, fmt::format_context>::value;

    if constexpr (type == fmt::detail::type::cstring_type || type == fmt::detail::type::string_type) {
        size = *reinterpret_cast<const std::size_t*>(in) + sizeof(std::size_t);
    } else {
        const auto* value = reinterpret_cast<const ArgType*>(in);
        size = get_size(*value);
        if constexpr (!std::is_trivially_copyable_v<ArgType>) {
            value->~ArgType();
        }
    }
    destruct<I + 1, SIZE, Args...>(in + size);
}

// ============================================================================
// Unpack and Format
// ============================================================================

// Unpack arguments and format into a string
template <std::size_t SIZE, typename... Args>
[[nodiscard]] std::string unpack_and_format(fmt::string_view format_str, const char* in) {
    std::array<FormatArg, SIZE> args{};
    unpack<0, SIZE, Args...>(args, in);
    auto result = fmt::vformat(format_str, fmt::format_args(args.data(), SIZE));
    destruct<0, SIZE, Args...>(in);
    return result;
}

}  // namespace detail
}  // namespace c2fastlog
