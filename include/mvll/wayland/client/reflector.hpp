#ifndef INCLUDE_MVLL_WAYLAND_CLIENT_REFLECTOR_HPP
#define INCLUDE_MVLL_WAYLAND_CLIENT_REFLECTOR_HPP

#include <string_view>

#include <cstddef>
#include <cstdint>

#include <mvll/fixed-string.hpp>

#include <wayland-client.h>

namespace mvll::inline wayland::inline client
{
    struct empty_type {};

    template <class T> struct proxy_meta_info {
        using proxy_type = empty_type;
        using listener_type = empty_type;
        static constexpr std::string_view name = "";
        static constexpr wl_interface const *const interface_ptr = nullptr;
        static constexpr void (*deleter)(proxy_type*) = nullptr;
    };

    template <> struct proxy_meta_info<wl_display> {
        using proxy_type = wl_display;
        using listener_type = empty_type; // exists, but not for us!
        static constexpr std::string_view name = "wl_display";
        static constexpr wl_interface const *const interface_ptr = &wl_display_interface;
        static constexpr void (*deleter)(proxy_type*) = wl_display_disconnect;
    };

    template <class T> concept is_defined = requires { sizeof (T); };
    template <class T> struct empty_or { using type = empty_type; };
    template <is_defined T> struct empty_or<T> { using type = T; };
} // ::mvll::wayland::client

#define MVLL_PROXY_LIST_BUILTIN(V)                                      \
    V(wl_registry)                                                      \
    V(wl_compositor)                                                    \
    V(wl_shm)

#define MVLL_PROXY_LIST_USER(V)                 \
    V(xdg_wm_base)

#define MVLL_PROXY_LIST_MASTER(V)               \
    MVLL_PROXY_LIST_BUILTIN(V)

#define REGISTER_PROXY_META_INFO(CLASS)                                 \
    struct CLASS##_listener;                                            \
    template <> struct mvll::wayland::client::proxy_meta_info<CLASS> {  \
        using proxy_type = CLASS;                                       \
        using listener_type =                                           \
            mvll::wayland::client::empty_or<CLASS##_listener>::type;    \
        static constexpr std::string_view name = #CLASS;                \
        static constexpr wl_interface const *const interface_ptr =      \
            &(CLASS##_interface);                                       \
        static constexpr void (*deleter)(proxy_type *) = CLASS##_destroy; \
    };
MVLL_PROXY_LIST_MASTER(REGISTER_PROXY_META_INFO)
#undef REGISTER_PROXY_META_INFO

enum class proxy_id : std::uint32_t {
#define REGISTER_PROXY_ID(CLASS)                \
    CLASS##_identifier,
#undef REGISTER_PROXY_ID
};



#endif /*INCLUDE_MVLL_WAYLAND_CLIENT_REFLECTOR_HPP*/
