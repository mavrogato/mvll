#ifndef INCLUDE_MVLL_WAYLAND_CLIENT_PROXY_META_HPP
#define INCLUDE_MVLL_WAYLAND_CLIENT_PROXY_META_HPP

#include <algorithm>
#include <array>
#include <string_view>

#include <cstddef>
#include <cstdint>

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>


#define MVLL_PROXY_LIST_BUILTIN(V)\
    V(wl_registry)                              \
    V(wl_compositor)                            \
    V(wl_shm)

#ifdef MVLL_PROXY_LIST
#define MVLL_PROXY_LIST_MASTER(V)               \
    MVLL_PROXY_LIST_BUILTIN(V)                  \
    MVLL_PROXY_LIST(V)
#else
#define MVLL_PROXY_LIST_MASTER(V)               \
    MVLL_PROXY_LIST_BUILTIN(V)
#endif

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
        INVALID_PROXY_ID,
    };
    constexpr std::size_t NOF_PROXIES = static_cast<std::size_t>(proxy_id::NOF_PROXIES);

    using erased_dsig = void (*)(void*);
    using erased_asig = int (*)(void*, void*, void*);

    struct proxy_metainfo {
        proxy_id id = proxy_id::INVALID_PROXY_ID;
        char const *const name = "unknown";
        wl_interface const *const interface_ptr = nullptr;
        bool has_listener = false;
        erased_dsig erased_deleter = nullptr;
        erased_asig erased_listener_adder = nullptr;
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
                wl_proxy_destroy(reinterpret_cast<wl_proxy*>(p));       \
            },                                                          \
            (is_defined<CLASS##_listener> ?                             \
            [](void* prx, void* cba, void* dat) noexcept {              \
                return wl_proxy_add_listener(                           \
                    reinterpret_cast<wl_proxy*>(prx),                   \
                    reinterpret_cast<void(**)(void)>(cba),              \
                    dat);                                               \
            } : nullptr),                                               \
        },
        MVLL_PROXY_LIST_MASTER(MVLL_INTERN_PROXY_METAINFO)
#undef MVLL_INTERN_PROXY_META_INFO
    };

    template <class T> inline constexpr proxy_id identifier = proxy_id::INVALID_PROXY_ID;
    template <> inline constexpr proxy_id identifier<wl_display> = proxy_id::wl_display_id;
#define MVLL_INTERN_PROXY_ID_VALUE(CLASS) \
    template <> constexpr inline proxy_id identifier<CLASS> = proxy_id::CLASS##_id;
    MVLL_PROXY_LIST_MASTER(MVLL_INTERN_PROXY_ID_VALUE)
#undef MVLL_INTERN_PROXY_ID_VALUE

    template <class T> concept is_proxy = ((identifier<T>) < proxy_id::NOF_PROXIES);
    template <is_proxy T> constexpr proxy_metainfo const& metainfo =
        metadb[static_cast<std::size_t>(identifier<T>)];

    template <class T> concept is_proxy_observable = (metainfo<T>.has_listener);
    template <is_proxy T> struct listener_impl { using type = void; };
#define MVLL_INTERN_PROXY_LISTENER(CLASS) \
    template <> struct listener_impl<CLASS> { using type = CLASS##_listener; };
    MVLL_PROXY_LIST_MASTER(MVLL_INTERN_PROXY_LISTENER)
#undef MVLL_INTERN_PROXY_LISTENER
    template <is_proxy_observable T> using listener_type = listener_impl<T>::type;

} // ::mvll::wayland::client



#endif /*INCLUDE_MVLL_WAYLAND_CLIENT_PROXY_META_HPP*/
