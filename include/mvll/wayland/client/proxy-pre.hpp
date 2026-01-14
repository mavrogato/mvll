#ifndef INCLUDE_MVLL_WAYLAND_CLIENT_PROXY_PRE_HPP
#define INCLUDE_MVLL_WAYLAND_CLIENT_PROXY_PRE_HPP

#include <wayland-client-protocol.h>

#define PROXY_ATTR_NONE 0
#define PROXY_ATTR_HAS_LISTENER 1

#define MVLL_PROXY_LIST_BUILTIN(V)                      \
    V(wl_display,             PROXY_ATTR_NONE)          \
    V(wl_registry,            PROXY_ATTR_HAS_LISTENER)  \
    V(wl_compositor,          PROXY_ATTR_NONE)          \
    V(wl_shm,                 PROXY_ATTR_HAS_LISTENER)  \
    V(wl_shm_pool,            PROXY_ATTR_NONE)          \
    V(wl_buffer,              PROXY_ATTR_HAS_LISTENER)  \
    V(wl_callback,            PROXY_ATTR_HAS_LISTENER)  \
    V(wl_output,              PROXY_ATTR_HAS_LISTENER)  \
    V(wl_seat,                PROXY_ATTR_HAS_LISTENER)  \
    V(wl_pointer,             PROXY_ATTR_HAS_LISTENER)  \
    V(wl_keyboard,            PROXY_ATTR_HAS_LISTENER)  \
    V(wl_touch,               PROXY_ATTR_HAS_LISTENER)  \
    V(wl_surface,             PROXY_ATTR_HAS_LISTENER)  \
    V(wl_region,              PROXY_ATTR_NONE)          \
    V(wl_subsurface,          PROXY_ATTR_NONE)          \
    V(wl_subcompositor,       PROXY_ATTR_NONE)          \
    V(wl_data_offer,          PROXY_ATTR_HAS_LISTENER)  \
    V(wl_data_source,         PROXY_ATTR_HAS_LISTENER)  \
    V(wl_data_device,         PROXY_ATTR_HAS_LISTENER)  \
    V(wl_data_device_manager, PROXY_ATTR_NONE)


#endif /*INCLUDE_MVLL_WAYLAND_CLIENT_PROXY_PRE_HPP*/
