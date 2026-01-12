#ifndef INCLUDE_MVLL_WAYLAND_CLIENT_PROXY_META_HPP
#define INCLUDE_MVLL_WAYLAND_CLIENT_PROXY_META_HPP

#include <array>

#include <cstddef>
#include <cstdint>

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

#include <mvll/pfr.hpp>
#include <mvll/wayland/client/proxy-pre.hpp>

#define MVLL_PROXY_LIST_BUILTIN(V) \
    V(wl_registry)                 \
    V(wl_compositor)               \
    V(wl_shm)                      \
    V(wl_shm_pool)                 \
    V(wl_buffer)                   \
    V(wl_callback)                 \
    V(wl_output)                   \
    V(wl_seat)                     \
    V(wl_pointer)                  \
    V(wl_keyboard)                 \
    V(wl_touch)                    \
    V(wl_surface)                  \
    V(wl_region)                   \
    V(wl_subsurface)               \
    V(wl_subcompositor)            \
    V(wl_data_offer)               \
    V(wl_data_source)              \
    V(wl_data_device)              \
    V(wl_data_device_manager)

#ifdef MVLL_PROXY_LIST
#define MVLL_PROXY_LIST_MASTER(V)  \
    MVLL_PROXY_LIST_BUILTIN(V)     \
    MVLL_PROXY_LIST(V)
#else
#define MVLL_PROXY_LIST_MASTER(V)  \
    MVLL_PROXY_LIST_BUILTIN(V)
#endif

// Note: pollutive
#define MVLL_INTERN_DUMMY_DEFINITION(CLASS) struct CLASS##_listener;
MVLL_PROXY_LIST_MASTER(MVLL_INTERN_DUMMY_DEFINITION)
#undef MVLL_INTERN_DUMMY_DEFINITION

namespace mvll::inline wayland::inline client
{
    template <class T> concept is_defined = requires { sizeof (T); };

    enum class proxy_id : std::uint32_t {
        wl_display_id = 0,
#define MVLL_INTERN_PROXY_ID(CLASS)             \
        CLASS##_id,
        MVLL_PROXY_LIST_MASTER(MVLL_INTERN_PROXY_ID)
#undef MVLL_INTERN_PROXY_ID
        NOF_PROXIES,
        INVALID_PROXY_ID = static_cast<std::uint32_t>(-1),
    };
    constexpr std::size_t NOF_PROXIES = static_cast<std::size_t>(proxy_id::NOF_PROXIES);

    using erased_dsig = void (*)(void*);
    using erased_asig = int (*)(void*, void*, void*);

    struct proxy_metainfo {
        proxy_id id = proxy_id::INVALID_PROXY_ID;
        char const *const name = "unknown";
        wl_interface const *const interface_ptr = nullptr;
        bool has_listener = false;
        erased_dsig deleter = nullptr;
        erased_asig listener_adder = nullptr;
    };
    constexpr std::array<proxy_metainfo, NOF_PROXIES> metadb {
        proxy_metainfo{
            proxy_id::wl_display_id,
            "wl_display",
            &wl_display_interface,
            false,
            [](void* p) noexcept {
                wl_display_disconnect(reinterpret_cast<wl_display*>(p));
            },
            nullptr,
        },
#define MVLL_INTERN_PROXY_METAINFO(CLASS)                               \
        proxy_metainfo{                                                 \
            proxy_id::CLASS##_id,                                       \
            #CLASS,                                                     \
            &CLASS##_interface,                                         \
            is_defined<CLASS##_listener>,                               \
            [](void* p) noexcept {                                      \
                CLASS##_destroy(static_cast<CLASS*>(p));                \
            },                                                          \
            (is_defined<CLASS##_listener> ?                             \
             [](void* proxy, void* callback, void* data) noexcept {     \
                 return wl_proxy_add_listener(                          \
                     reinterpret_cast<wl_proxy*>(proxy),                \
                     reinterpret_cast<void(**)(void)>(callback),        \
                     data);                                             \
             } : nullptr),                                              \
        },
        MVLL_PROXY_LIST_MASTER(MVLL_INTERN_PROXY_METAINFO)
#undef MVLL_INTERN_PROXY_META_INFO
    };
    
    template <class F>
    constexpr auto dispatch_by_id(proxy_id id, F&& func) {
        switch (id) {
        case proxy_id::wl_display_id: return func.template operator()<wl_display>();
#define MVLL_INTERN_DISPATCH_CASE(CLASS)                                \
            case proxy_id::CLASS##_id: return func.template operator()<CLASS>();
            MVLL_PROXY_LIST_MASTER(MVLL_INTERN_DISPATCH_CASE)
#undef MVLL_INTERN_DISPATCH_CASE
        default: return func.template operator()<void>();
        }
    }

    template <class T> inline constexpr proxy_id identifier = proxy_id::INVALID_PROXY_ID;
    template <> inline constexpr proxy_id identifier<wl_display> = proxy_id::wl_display_id;
#define MVLL_INTERN_PROXY_ID_VALUE(CLASS) \
    template <> constexpr inline proxy_id identifier<CLASS> = proxy_id::CLASS##_id;
    MVLL_PROXY_LIST_MASTER(MVLL_INTERN_PROXY_ID_VALUE)
#undef MVLL_INTERN_PROXY_ID_VALUE

    template <class T> concept is_proxy = ((identifier<T>) < proxy_id::NOF_PROXIES);
    template <class T> concept is_proxy_observable =
        metadb[static_cast<std::size_t>(identifier<T>)].has_listener;
    template <is_proxy T> struct proxy_to_listener_impl;
    template <class L> struct listener_to_proxy_impl;
#define MVLL_INTERN_PROXY_LISTENER(CLASS)                         \
    template <> struct proxy_to_listener_impl<CLASS> {            \
        using listener_type = CLASS##_listener;                   \
    };                                                            \
    template <> struct listener_to_proxy_impl<CLASS##_listener> { \
        using proxy_type = CLASS;                                 \
    };
    MVLL_PROXY_LIST_MASTER(MVLL_INTERN_PROXY_LISTENER)
#undef MVLL_INTERN_PROXY_LISTENER
    template <is_proxy_observable T> using listener_type = proxy_to_listener_impl<T>::type;
    template <class L> concept is_listener = requires { listener_to_proxy_impl<L>::proxy_type; };

    template <class T> struct event_signature_traits;
    template <class... Rest>
    struct event_signature_traits<void (*)(void*, Rest...)> {
        using return_type = void;
        static inline constexpr std::size_t rest_arity = sizeof...(Rest);
        static inline constexpr std::size_t arity = 1 + rest_arity;
        using rest_args_tuple = std::tuple<Rest...>;
        using args_tuple = std::tuple<void*, Rest...>;
        template <std::size_t N> using rest_arg_t = std::tuple_element_t<N, rest_args_tuple>;
        template <std::size_t N> using arg_t = std::tuple_element_t<N, args_tuple>;
    };
    template <class T> concept is_event_signature = requires { event_signature_traits<T>::arity; };

    template <class, auto> struct event_traits_impl;
    template <is_listener L, is_event_signature M, M L::*Member>
    struct event_traits_impl<M L::*, Member> {
        using proxy_type = listener_to_proxy_impl<L>;
        using listener_type = std::remove_pointer_t<L>;
        using member_type = M;
        using rest_args_tuple = typename event_signature_traits<M>::rest_args_tuple;
        static inline constexpr std::uint32_t ordinal = [] noexcept {
            return pfr::get_ordinal<L, Member>();
        }();
    };
    template <auto Member> using event_traits = event_traits_impl<decltype (Member), Member>;

} // ::mvll::wayland::client

#endif /*INCLUDE_MVLL_WAYLAND_CLIENT_PROXY_META_HPP*/
