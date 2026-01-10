#ifndef INCLUDE_MVLL_WAYLAND_CLIENT_REFLECTOR_HPP
#define INCLUDE_MVLL_WAYLAND_CLIENT_REFLECTOR_HPP

#include <string_view>

#include <mvll/fixed-string.hpp>

#include <wayland-client.h>

namespace mvll::inline wayland::inline client
{
    struct empty_type {};

    template <fixed_string NAME> struct proxy_interner {
        using proxy_type = empty_type;
        using listener_type = empty_type;
        static constexpr std::string_view name = NAME;
        static constexpr wl_interface const *const interface_ptr = nullptr;
        static constexpr void (*deleter)(proxy_type*) = nullptr;
    };

    template <> struct proxy_interner<"wl_display"> {
        using proxy_type = wl_display;
        using listener_type = empty_type; // exists, but not for us...
        static constexpr std::string_view name = "wl_display";
        static constexpr wl_interface const *const interface_ptr = &wl_display_interface;
        static constexpr void (*deleter)(proxy_type*) = wl_display_disconnect;
    };

#define REGISTER_PROXY(CLASS, LISTENER)                                 \
    template <> struct proxy_interner<#CLASS> {                         \
        using proxy_type = CLASS;                                       \
        using listener_type = LISTENER;                                 \
        static constexpr std::string_view name = #CLASS;                \
        static constexpr wl_interface const *const interface_ptr = &(CLASS##_interface); \
        static constexpr void (*deleter)(proxy_type*) = CLASS##_destroy; \
    };

    REGISTER_PROXY(wl_registry, wl_registry_listener)
    REGISTER_PROXY(wl_compositor, empty_type)

} // ::mvll::wayland::client

#endif /*INCLUDE_MVLL_WAYLAND_CLIENT_REFLECTOR_HPP*/
