#ifndef INCLUDE_MVLL_WAYLAND_CLIENT_PROXY_PRE_HPP
#define INCLUDE_MVLL_WAYLAND_CLIENT_PROXY_PRE_HPP

#include <cstdint>

namespace mvll::inline wayland::inline client
{    
    enum class proxy_attributes : std::uint32_t {
        plain = 0,
        has_listener = 1,
        // suppress_action = 2,
        // suppress_fiblet = 4,
    };

    // template <class E> struct is_bitmask_enum : std::false_type {};
    // template <> struct is_bitmask_enum<proxy_attributes> : std::true_type {};
    // template <class E> concept is_bitmask = is_bitmask_enum<E>::type;
    // template <is_bitmask E>
    // constexpr E operator|(E lhs, E rhs) noexcept {
    //     return static_cast<E>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
    // }
    // template <is_bitmask E>
    // constexpr E operator&(E lhs, E rhs) noexcept {
    //     return static_cast<E>(static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
    // }
    // template <is_bitmask E>
    // constexpr E operator^(E lhs, E rhs) noexcept {
    //     return static_cast<E>(static_cast<std::uint32_t>(lhs) ^ static_cast<std::uint32_t>(rhs));
    // }
    // template <is_bitmask E>
    // constexpr E operator~(E lhs) noexcept {
    //     return static_cast<E>(~static_cast<std::uint32_t>(lhs));
    // }

} // ::mvll::wayland::client

#endif /*INCLUDE_MVLL_WAYLAND_CLIENT_PROXY_PRE_HPP*/
