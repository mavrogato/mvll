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


#ifdef MVLL_PROXY_LIST
#define MVLL_PROXY_LIST_MASTER(V)  \
    MVLL_PROXY_LIST_BUILTIN(V)     \
    MVLL_PROXY_LIST(V)
#else
#define MVLL_PROXY_LIST_MASTER(V)  \
    MVLL_PROXY_LIST_BUILTIN(V)
#endif

#define MVLL_CONCAT_EVAL(a, b)            a##b
#define MVLL_CONCAT(a, b)                 MVLL_CONCAT_EVAL(a, b)
#define MVLL_EXPAND(x)                    x
#define MVLL_BOOL_1                       _YES
#define MVLL_BOOL_0                       _NO
#define MVLL_BOOL_PROXY_ATTR_HAS_LISTENER _YES
#define MVLL_BOOL_PROXY_ATTR_NONE         _NO
#define MVLL_IF__YES(THEN, ELSE)          THEN
#define MVLL_IF__NO(THEN, ELSE)           ELSE
#define MVLL_IF(COND, THEN, ELSE)                                       \
    MVLL_EXPAND(MVLL_CONCAT(MVLL_IF_, MVLL_CONCAT(MVLL_BOOL_, COND)))(THEN, ELSE)
#define MVLL_WHEN__YES(...)  __VA_ARGS__
#define MVLL_WHEN__NO(...) 
#define MVLL_WHEN(COND, ...)                                            \
    MVLL_EXPAND(MVLL_CONCAT(MVLL_WHEN_, MVLL_CONCAT(MVLL_BOOL_, COND)))(__VA_ARGS__)
#define MVLL_UNLESS(COND, ...)                                          \
    MVLL_EXPAND(MVLL_CONCAT(MVLL_WHEN_, MVLL_CONCAT(MVLL_BOOL_, COND)))( /* empty */, __VA_ARGS__ )

namespace mvll::inline wayland::inline client
{
    template <class T> concept is_defined = requires { sizeof (T); };

    enum class proxy_id : std::uint32_t {
#define MVLL_INTERN_PROXY_ID(CLASS, ATTR)       \
        CLASS##_id,
        MVLL_PROXY_LIST_MASTER(MVLL_INTERN_PROXY_ID)
#undef MVLL_INTERN_PROXY_ID
        NOF_PROXIES,
        INVALID_PROXY_ID = static_cast<std::uint32_t>(-1),
    };
    constexpr std::size_t NOF_PROXIES = static_cast<std::size_t>(proxy_id::NOF_PROXIES);

    namespace internals
    {
        template <proxy_id ID> struct proxy_type_impl;
#define MVLL_INTERN_PROXY_TYPE(CLASS, ATTR)                         \
        template <> struct proxy_type_impl<proxy_id::CLASS##_id> {  \
            using type = CLASS;                                     \
        };
        MVLL_PROXY_LIST_MASTER(MVLL_INTERN_PROXY_TYPE)
#undef MVLL_INTERN_PROXY_TYPE
    }
    template <proxy_id ID> using proxy_type = internals::proxy_type_impl<ID>::type;

    struct proxy_metainfo {
        proxy_id id = proxy_id::INVALID_PROXY_ID;
        std::uint32_t attr = 0;
        char const *const name = "invalid";
        wl_interface const *const interface_ptr = nullptr;

        constexpr bool has_listener() const noexcept {
            return 0 != (attr & PROXY_ATTR_HAS_LISTENER);
        }
    };
    constexpr std::array<proxy_metainfo, NOF_PROXIES> metadb {
#define MVLL_INTERN_PROXY_METAINFO(CLASS, ATTR)                         \
        proxy_metainfo{                                                 \
            proxy_id::CLASS##_id,                                       \
            ATTR,                                                       \
            #CLASS,                                                     \
            &CLASS##_interface,                                         \
        },
        MVLL_PROXY_LIST_MASTER(MVLL_INTERN_PROXY_METAINFO)
#undef MVLL_INTERN_PROXY_METAINFO
    };
    template <class F>
    constexpr auto dispatch_by_id(proxy_id id, F&& func) {
        switch (id) {
#define MVLL_INTERN_DISPATCH_CASE(CLASS, ATTR)                          \
            case proxy_id::CLASS##_id: return func.template operator()<CLASS>();
            MVLL_PROXY_LIST_MASTER(MVLL_INTERN_DISPATCH_CASE)
#undef MVLL_INTERN_DISPATCH_CASE
        default: return func.template operator()<void>();
        }
    }

    template <class T> inline constexpr proxy_id identifier = proxy_id::INVALID_PROXY_ID;
#define MVLL_INTERN_PROXY_ID_VALUE(CLASS, ATTR)                         \
    template <> constexpr inline proxy_id identifier<CLASS> = proxy_id::CLASS##_id;
    MVLL_PROXY_LIST_MASTER(MVLL_INTERN_PROXY_ID_VALUE)
#undef MVLL_INTERN_PROXY_ID_VALUE

    template <class T> concept is_proxy = ((identifier<T>) < proxy_id::NOF_PROXIES);

    namespace internals
    {
        template <is_proxy T> struct proxy_to_listener_impl;
        template <class L> struct listener_to_proxy_impl;
#define MVLL_INTERN_PROXY_LISTENER(CLASS, ATTR)                         \
        MVLL_WHEN(                                                      \
            ATTR,                                                       \
            template <> struct proxy_to_listener_impl<CLASS> {          \
                using type = CLASS##_listener;                          \
            };                                                          \
            template <> struct listener_to_proxy_impl<CLASS##_listener> { \
                using type = CLASS;                                     \
            };                                                          \
        )
        MVLL_PROXY_LIST_MASTER(MVLL_INTERN_PROXY_LISTENER)
#undef MVLL_INTERN_PROXY_LISTENER
    }
    template <class T> concept is_proxy_observable = is_proxy<T> && requires {
        typename internals::proxy_to_listener_impl<T>::type;
    };

    template <is_proxy_observable T> using listener_type = internals::proxy_to_listener_impl<T>::type;
    template <class L> concept is_listener = requires {
        typename internals::listener_to_proxy_impl<L>::type;
    };

    template <class T> constexpr inline void (*delete_proxy)(T*) = nullptr;
    template <> constexpr inline void (*delete_proxy<wl_display>)(wl_display*) = wl_display_disconnect;
    template <is_proxy T> constexpr inline void (*delete_proxy<T>)(T*) noexcept = [](T* raw) noexcept {
        wl_proxy_destroy(reinterpret_cast<wl_proxy*>(raw));
    };
    template <is_proxy_observable T>
    constexpr inline int add_listener(T* raw, listener_type<T> const* listener, void* data) noexcept {
        return wl_proxy_add_listener(reinterpret_cast<wl_proxy*>(raw),
                                     reinterpret_cast<void (**)(void)>(
                                         const_cast<listener_type<T>*>(listener)),
                                     data);
    }

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

    namespace internals
    {
        template <class, auto> struct event_traits_impl;
        template <is_listener L, is_event_signature M, M L::*Member>
        struct event_traits_impl<M L::*, Member> {
            using proxy_type = internals::listener_to_proxy_impl<L>::type;
            using listener_type = std::remove_pointer_t<L>;
            using member_type = M;
            using rest_args_tuple = typename event_signature_traits<M>::rest_args_tuple;
            static inline constexpr std::uint32_t ordinal = [] noexcept {
                return pfr::get_ordinal<Member, [](auto...){}>();
            }();
        };
    }
    template <auto Member> using event_traits = internals::event_traits_impl<decltype (Member), Member>;

} // ::mvll::wayland::client

#endif /*INCLUDE_MVLL_WAYLAND_CLIENT_PROXY_META_HPP*/
