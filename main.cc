
#include "mvll/error-handling.hpp"

#include <forward_list>
#include <iostream>
#include <type_traits>

#include <mvll/cpp2x/tuple-support.hpp>
#include <mvll/fiblet.hpp>
#include <mvll/platform/linux.hpp>

#include <mvll/wayland/client/proxy-pre.hpp>
#include <xdg-shell-client.h>
#include <zwp-tablet-v2-client.h>
#include <wp-fractional-scale-v1-client.h>
#define MVLL_PROXY_LIST(V)                                              \
    V(xdg_wm_base,            PROXY_ATTR_HAS_LISTENER)                  \
    V(xdg_surface,            PROXY_ATTR_HAS_LISTENER)                  \
    V(xdg_toplevel,           PROXY_ATTR_HAS_LISTENER)                  \
    V(zwp_tablet_manager_v2,  PROXY_ATTR_NONE)                          \
    V(zwp_tablet_seat_v2,     PROXY_ATTR_HAS_LISTENER)                  \
    V(zwp_tablet_tool_v2,     PROXY_ATTR_HAS_LISTENER)                  \
    V(wp_fractional_scale_v1, PROXY_ATTR_HAS_LISTENER)                  
#include <mvll/wayland/client/proxy.hpp>

int main() {
    using namespace mvll;
    auto display = proxy{wl_display_connect(nullptr)};
    auto registry = proxy{wl_display_get_registry(display.get())};
    proxy<wl_compositor> compositor;
    std::forward_list<proxy<wl_seat>> seats;
    proxy<wl_shm> shm;
    proxy<xdg_wm_base> shell;
    proxy<zwp_tablet_manager_v2> tablet_manager;
    registry.on<&wl_registry_listener::global>([&](wl_registry*  registry,
                                                   std::uint32_t name,
                                                   char const*   interface,
                                                   std::uint32_t version) {
        if (interface_name<wl_compositor> == interface) {
            compositor = registry_bind<wl_compositor>(registry, name, version);
        }
        else if (interface_name<wl_seat> == interface) {
            seats.emplace_front(registry_bind<wl_seat>(registry, name, version));
        }
        else if (interface_name<wl_shm> == interface) {
            shm = registry_bind<wl_shm>(registry, name, version);
        }
        else if (interface_name<xdg_wm_base> == interface) {
            shell = registry_bind<xdg_wm_base>(registry, name, version);
        }
        // else if (interface_name<zwp_tablet_manager_v2> == interface) {
        //     tablet_manager = registry_bind<zwp_tablet_manager_v2>(registry, name, version);
        // }
    });
    registry.on<&wl_registry_listener::global_remove>([&](wl_registry*, std::uint32_t name) noexcept {
        std::erase_if(seats, [name](auto const& s) { return s.id() == name; });
    });
    wl_display_roundtrip(display);

    struct stroke {
        std::int32_t id;
        fiblet<versor<wl_fixed_t, 2>> coro;
    };
    std::forward_list<stroke> strokes;

    for (auto& seat : seats) {
        // MVLL_CHECK(tablet_manager);
        // if (tablet_manager) {
        //     auto tablet_seat = proxy{zwp_tablet_manager_v2_get_tablet_seat(tablet_manager, seat)};
        //     MVLL_CHECK(tablet_seat);
        //     tablet_seat.on([&] -> listener_fiblet<&zwp_tablet_seat_v2_listener::pad_added> {
        //             std::cout << "pad added!" << std::endl;
        //         });
        //     tablet_seat.on<&zwp_tablet_seat_v2_listener::tool_added>([&](auto...) {
        //         std::cout << "tool added!" << std::endl;
        //     });
        // }
        seat.on([&] -> listener_fiblet<&wl_seat_listener::capabilities> {
            proxy<wl_keyboard> keyboard;
            proxy<wl_pointer> pointer;
            proxy<wl_touch> touch;
            for (;;) {
                auto const& [seat, caps] = co_await wait_current;
                if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
                    if (!keyboard) {
                        keyboard = proxy{wl_seat_get_keyboard(seat)};
                    }
                    keyboard.on([] -> listener_fiblet<&wl_keyboard_listener::key> {
                        for (;;) {
                            [[maybe_unused]] auto const& args = co_await wait_current;
                        }
                    });
                    keyboard.on([] -> listener_fiblet<&wl_keyboard_listener::modifiers> {
                        for (;;) {
                            [[maybe_unused]] auto const& args = co_await wait_current;
                        }
                    });
                    keyboard.on([] -> listener_fiblet<&wl_keyboard_listener::repeat_info> {
                        for (;;) {
                            [[maybe_unused]] auto const& args = co_await wait_current;
                        }
                    });
                }
                else {
                    keyboard = {};
                }
                if (caps & WL_SEAT_CAPABILITY_POINTER) {
                    if (!pointer) {
                        pointer = proxy{wl_seat_get_pointer(seat)};
                    }
                    pointer.on([] -> listener_fiblet<&wl_pointer_listener::axis_value120> {
                        for (;;) {
                            [[maybe_unused]] auto const& args = co_await wait_current;
                        }
                    });
                }
                else {
                    pointer = {};
                }
                if (caps & WL_SEAT_CAPABILITY_TOUCH) {
                    if (!touch) {
                        touch = proxy{wl_seat_get_touch(seat)};
                    }
                    touch.on<&wl_touch_listener::down>([&](wl_touch*,
                                                           std::uint32_t,
                                                           std::uint32_t,
                                                           wl_surface*,
                                                           std::int32_t id,
                                                           wl_fixed_t x,
                                                           wl_fixed_t y) noexcept {
                        strokes.emplace_front(stroke {
                                id,
                                [id]() -> fiblet<versor<wl_fixed_t, 2>> {
                                    for (;;) {
                                        [[maybe_unused]] auto ret = co_yield {};
                                        auto [x, y] = *static_cast<versor<wl_fixed_t, 2> const*>(ret);
                                        std::cout << id << ": " << x << ',' << y << std::endl;
                                    }
                                }(),
                            });
                        std::cout << "start stroke #" << id << std::endl;
                        versor<wl_fixed_t, 2> cur{x, y};
                        strokes.front().coro.handle().resume();
                        strokes.front().coro.push(&cur);
                    });
                    touch.on<&wl_touch_listener::up>([&](wl_touch*,
                                                         std::uint32_t,
                                                         std::uint32_t,
                                                         std::int32_t id) noexcept {
                        std::erase_if(strokes, [id](auto const& s) { return s.id == id; });
                        std::cout << "remove stroke #" << id << std::endl;
                    });
                    touch.on<&wl_touch_listener::motion>([&](wl_touch*,
                                                             std::uint32_t,
                                                             std::int32_t id,
                                                             wl_fixed_t x,
                                                             wl_fixed_t y) noexcept {
                        for (auto& stroke : strokes) {
                            if (stroke.id == id) {
                                versor<wl_fixed_t, 2> cur{x, y};
                                stroke.coro.push(&cur);
                            }
                        }
                    });
                }
                else {
                    touch = {};
                }
            }
        });
    }
    wl_display_roundtrip(display);

    MVLL_CHECK(compositor);
    MVLL_CHECK(shm);
    MVLL_CHECK(shell);
    shell.on<&xdg_wm_base_listener::ping>([](xdg_wm_base* shell, std::uint32_t serial) noexcept {
        xdg_wm_base_pong(shell, serial);
    });
    auto surface = proxy{wl_compositor_create_surface(compositor)};
    auto xsurface = proxy{xdg_wm_base_get_xdg_surface(shell, surface)};
    xsurface.on<&xdg_surface_listener::configure>([](xdg_surface* xsurface, std::uint32_t serial) noexcept {
        xdg_surface_ack_configure(xsurface, serial);
    });

    auto toplevel = proxy{xdg_surface_get_toplevel(xsurface)};
    toplevel.on([&] MVLL_NOEXCEPT -> listener_fiblet<&xdg_toplevel_listener::configure> {
        std::size_t scale = 1;
        std::size_t cx = 640 * scale;
        std::size_t cy = 480 * scale;
        auto primary = shm_allocate_buffer(shm, cx, cy);
        auto secondary = shm_allocate_buffer(shm, cx, cy);
        auto release_callback = [&primary, &secondary](wl_buffer*) {
            std::swap(primary, secondary);
        };
        auto& primary_buffer = std::get<1>(primary);
        auto& secondary_buffer = std::get<1>(secondary);
        primary_buffer.on<&wl_buffer_listener::release>(release_callback);
        secondary_buffer.on<&wl_buffer_listener::release>(release_callback);
        auto frame = proxy{wl_surface_frame(surface)}; 
        // auto que = sycl::queue();
        // std::cout << que.get_device().get_info<sycl::info::device::name>() << std::endl;
        for (;;) {
            auto const& args = co_await wait_current;
            auto const& [toplevel, h, w, states] = args;
            cx = h * scale;
            cy = w * scale;
            if (cx * cy > 0) {
                primary = shm_allocate_buffer(shm, cx, cy);
                secondary = shm_allocate_buffer(shm, cx, cy);
                primary_buffer.on<&wl_buffer_listener::release>(release_callback);
                secondary_buffer.on<&wl_buffer_listener::release>(release_callback);
            }
            else { // the initial configuration
                wl_surface_attach(surface, primary_buffer, 0, 0);
                wl_surface_commit(surface);
            }
            frame.on<&wl_callback_listener::done>([&](wl_callback*, std::uint32_t) MVLL_NOEXCEPT {
                frame.rebind(wl_surface_frame(surface));
                wl_surface_attach(surface, primary_buffer, 0, 0);
                wl_surface_damage_buffer(surface, 0, 0, cx, cy);
                wl_surface_commit(surface);
                wl_display_flush(display);
            });
        }
    });
    bool quit = false;
    toplevel.on<&xdg_toplevel_listener::close>([&](xdg_toplevel*) noexcept {
        quit = true;
    });
    xdg_toplevel_set_app_id(toplevel, "mvll");

    wl_surface_commit(surface);
    while (-1 != wl_display_dispatch(display)) {
        if (quit) break;
    }
    return 0;
}
