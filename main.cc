
#include "mvll/error-handling.hpp"

#include <forward_list>
#include <iostream>
#include <type_traits>

#include <mvll/cpp2x/tuple-support.hpp>
#include <mvll/fiblet.hpp>
#include <mvll/platform/linux.hpp>

#include <mvll/wayland/client/proxy-pre.hpp>
#include <wayland-client-protocol.h>
#include <wp-fractional-scale-v1-client.h>
#include <wp-presentation-client.h>
#include <wp-viewporter-client.h>
#include <xdg-shell-client.h>
#include <zwp-tablet-v2-client.h>
#define MVLL_PROXY_LIST(V)                                              \
    V(wp_fractional_scale_manager_v1, PROXY_ATTR_NONE)                  \
    V(wp_fractional_scale_v1,         PROXY_ATTR_HAS_LISTENER)          \
    V(wp_presentation,                PROXY_ATTR_HAS_LISTENER)          \
    V(wp_presentation_feedback,       PROXY_ATTR_HAS_LISTENER)          \
    V(wp_viewport,                    PROXY_ATTR_NONE)                  \
    V(wp_viewporter,                  PROXY_ATTR_NONE)                  \
    V(xdg_surface,                    PROXY_ATTR_HAS_LISTENER)          \
    V(xdg_toplevel,                   PROXY_ATTR_HAS_LISTENER)          \
    V(xdg_wm_base,                    PROXY_ATTR_HAS_LISTENER)          \
    V(zwp_tablet_manager_v2,          PROXY_ATTR_NONE)                  \
    V(zwp_tablet_pad_v2,              PROXY_ATTR_HAS_LISTENER)          \
    V(zwp_tablet_seat_v2,             PROXY_ATTR_HAS_LISTENER)          \
    V(zwp_tablet_tool_v2,             PROXY_ATTR_HAS_LISTENER)          \
    V(zwp_tablet_v2,                  PROXY_ATTR_HAS_LISTENER)
#include <mvll/wayland/client/proxy.hpp>

int main() {
    using namespace mvll;
    auto display = proxy{wl_display_connect(nullptr)};
    auto registry = proxy{wl_display_get_registry(display.get())};
    proxy<wl_compositor> compositor;
    std::forward_list<proxy<wl_seat>> seats;
    std::forward_list<proxy<wl_output>> outputs;
    proxy<wl_shm> shm;
    proxy<xdg_wm_base> shell;
    proxy<zwp_tablet_manager_v2> tablet_manager;
    proxy<wp_fractional_scale_manager_v1> scaler;
    proxy<wp_viewporter> viewporter;
    proxy<wp_presentation> presentation;
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
        else if (interface_name<wl_output> == interface) {
            outputs.emplace_front(registry_bind<wl_output>(registry, name, version));
        }
        else if (interface_name<wl_shm> == interface) {
            shm = registry_bind<wl_shm>(registry, name, version);
        }
        else if (interface_name<xdg_wm_base> == interface) {
            shell = registry_bind<xdg_wm_base>(registry, name, version);
        }
        else if (interface_name<zwp_tablet_manager_v2> == interface) {
            tablet_manager = registry_bind<zwp_tablet_manager_v2>(registry, name, version);
        }
        else if (interface_name<wp_fractional_scale_manager_v1> == interface) {
            scaler = registry_bind<wp_fractional_scale_manager_v1>(registry, name, version);
        }
        else if (interface_name<wp_viewporter> == interface) {
            viewporter = registry_bind<wp_viewporter>(registry, name, version);
        }
        else if (interface_name<wp_presentation> == interface) {
            presentation = registry_bind<wp_presentation>(registry, name, version);
        }
    });
    registry.on<&wl_registry_listener::global_remove>([&](wl_registry*, std::uint32_t name) noexcept {
        std::erase_if(seats, [name](auto const& item) { return item.id() == name; });
        std::erase_if(outputs, [name](auto const& item) { return item.id() == name; });
    });
    wl_display_roundtrip(display);
    std::cout << "*** The first roundtrip has done." << std::endl;

    using output_info = std::tuple<
        event_traits<&wl_output_listener::name>::payload_tuple,
        event_traits<&wl_output_listener::description>::payload_tuple,
        event_traits<&wl_output_listener::mode>::payload_tuple,
        event_traits<&wl_output_listener::scale>::payload_tuple,
        event_traits<&wl_output_listener::geometry>::payload_tuple
        >;
    for (auto& output : outputs) {
        output.fiblet() = [&] -> listener_fiblet<&wl_output_listener::done> {
            output_info info;
            for (;;) {
                output.on<&wl_output_listener::name>([&](wl_output*, auto... rest) {
                    std::get<0>(info) = std::tuple{rest...};
                });
                output.on<&wl_output_listener::description>([&](wl_output*, auto... rest) {
                    std::get<1>(info) = std::tuple{rest...};
                });
                output.on<&wl_output_listener::mode>([&](wl_output*, auto... rest) {
                    std::get<2>(info) = std::tuple{rest...};
                });
                output.on<&wl_output_listener::scale>([&](wl_output*, auto... rest) {
                    std::get<3>(info) = std::tuple{rest...};
                });
                output.on<&wl_output_listener::geometry>([&](wl_output*, auto... rest) {
                    std::get<4>(info) = std::tuple{rest...};
                });
                co_await wait_current;
                std::cout << info << std::endl;
            }
        };
    }

    std::uint32_t scale120 = 120;
    std::size_t logical_cx = 640;
    std::size_t logical_cy = 480;
    std::size_t buffer_cx = 640 * scale120 / 120;
    std::size_t buffer_cy = 480 * scale120 / 120;

    for (auto& seat : seats) {
        if (tablet_manager) {
            MVLL_CHECK(!seat.anchor);
            auto& tablet_seat = (seat.anchor = proxy{zwp_tablet_manager_v2_get_tablet_seat(tablet_manager, seat)});
            MVLL_CHECK(tablet_seat);
            tablet_seat.fiblet() = [] -> listener_fiblet<&zwp_tablet_seat_v2_listener::tablet_added> {
                std::forward_list<proxy<zwp_tablet_v2>> tablets;
                for (;;) {
                    auto const& [seat, id] = co_await wait_current;
                    auto tablet = proxy{id};
                    std::cout << "tablet added: " << tablet << std::endl;
                    tablet.on<&zwp_tablet_v2_listener::removed> ([&](zwp_tablet_v2* id) {
                        tablets.remove(id);
                        std::cout << "tablet removed: " << id << std::endl;
                    });
                    tablet.on<&zwp_tablet_v2_listener::name>([&](auto, auto name) {
                        std::cout << "tablet name: " << name << std::endl;
                    });
                    tablet.on<&zwp_tablet_v2_listener::path> ([&](auto, auto path) {
                        std::cout << "tablet path: " << path << std::endl;
                    });
                    tablets.emplace_front(std::move(tablet));
                }
            };
            tablet_seat.fiblet() = [] -> listener_fiblet<&zwp_tablet_seat_v2_listener::pad_added> {
                std::forward_list<proxy<zwp_tablet_pad_v2>> tablet_pads;
                for (;;) {
                    auto const& [seat, id] = co_await wait_current;
                    auto tablet_pad = proxy{id};
                    std::cout << "pad added: " << tablet_pad << std::endl;
                    tablet_pad.on<&zwp_tablet_pad_v2_listener::removed>([&](zwp_tablet_pad_v2* id) {
                        tablet_pads.remove(id);
                        std::cout << "pad removed:" << id << std::endl;
                    });
                    tablet_pad.on<&zwp_tablet_pad_v2_listener::path>([&](auto, auto path) {
                        std::cout << "pad path: " << path << std::endl;
                    });
                    tablet_pads.emplace_front(std::move(tablet_pad));
                }
            };
            tablet_seat.fiblet() = [] -> listener_fiblet<&zwp_tablet_seat_v2_listener::tool_added> {
                std::forward_list<proxy<zwp_tablet_tool_v2>> tablet_tools;
                for (;;) {
                    auto const& [seat, id] = co_await wait_current;
                    auto tablet_tool = proxy{id};
                    std::cout << "tool added: " << tablet_tool << std::endl;
                    tablet_tool.on<&zwp_tablet_tool_v2_listener::removed>([&](zwp_tablet_tool_v2* id) {
                        tablet_tools.remove(id);
                        std::cout << "tool removed: " << id << std::endl;
                    });
                    tablet_tools.emplace_front(std::move(tablet_tool));
                }
            };
        }
        seat.fiblet() = [] -> listener_fiblet<&wl_seat_listener::capabilities> {
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

                struct stroke {
                    std::int32_t id;
                    fiblet<versor<wl_fixed_t, 2>> coro;
                };
                std::forward_list<stroke> strokes;
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
                        strokes.emplace_front(
                            stroke(id,
                                   [](std::int32_t id) -> fiblet<versor<wl_fixed_t, 2>> {
                                       for (;;) {
                                           [[maybe_unused]] auto ret = co_yield {};
                                           auto [x, y] = ret;
                                           std::cout << id << ": " << x << ',' << y << std::endl;
                                       }
                                   }(id)));
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
        };
    }
    wl_display_roundtrip(display);
    std::cout << "*** The second roundtrip has done." << std::endl;

    MVLL_CHECK(compositor);
    MVLL_CHECK(shm);
    MVLL_CHECK(shell);
    MVLL_CHECK(viewporter);
    shell.on<&xdg_wm_base_listener::ping>([](xdg_wm_base* shell, std::uint32_t serial) noexcept {
        std::cout << "base_pong: " << serial << std::endl;
        xdg_wm_base_pong(shell, serial);
    });
    auto surface = proxy{wl_compositor_create_surface(compositor)};
    auto viewport = proxy{wp_viewporter_get_viewport(viewporter, surface)};
    if (scaler) {
        auto& scale = scaler.emplace_anchor(
            proxy{wp_fractional_scale_manager_v1_get_fractional_scale(scaler, surface)});
        scale.on<&wp_fractional_scale_v1_listener::preferred_scale>([&](auto, std::uint32_t scale) {
            std::cout << "preferred_scale changed: " << scale120 << "->" << scale << std::endl;
            scale120 = scale;
            buffer_cx = logical_cx * scale120 / 120;
            buffer_cy = logical_cy * scale120 / 120;
            wp_viewport_set_destination(viewport, logical_cx, logical_cy);
            //wl_surface_commit(surface); // T.B.D.
        });
    }
    auto feedback = proxy{wp_presentation_feedback(presentation, surface)};
    feedback.on<&wp_presentation_feedback_listener::presented>([&](struct wp_presentation_feedback*,
                                                                   [[maybe_unused]] uint32_t tv_sec_hi,
                                                                   [[maybe_unused]]uint32_t tv_sec_lo,
                                                                   [[maybe_unused]]uint32_t tv_nsec,
                                                                   uint32_t refresh,
                                                                   [[maybe_unused]]uint32_t seq_hi,
                                                                   [[maybe_unused]]uint32_t seq_lo,
                                                                   [[maybe_unused]]uint32_t flags) {
        std::cout << "presentation feedback refresh: " << refresh << std::endl;
    });
    feedback.on<&wp_presentation_feedback_listener::discarded>([&](struct wp_presentation_feedback*) {
        std::cout << "presentation feedback discarded." << std::endl;
    });
    auto xsurface = proxy{xdg_wm_base_get_xdg_surface(shell, surface)};
    xsurface.on<&xdg_surface_listener::configure>([](xdg_surface* xsurface, std::uint32_t serial) noexcept {
        std::cout << "xsurface configured: " << serial << std::endl;
        xdg_surface_ack_configure(xsurface, serial);
    });
    auto toplevel = proxy{xdg_surface_get_toplevel(xsurface)};
    toplevel.fiblet() = [&] MVLL_NOEXCEPT -> listener_fiblet<&xdg_toplevel_listener::configure> {
        auto primary = shm_allocate_buffer(shm, buffer_cx, buffer_cy);
        auto secondary = shm_allocate_buffer(shm, buffer_cx, buffer_cy);
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
            auto const& [toplevel, w, h, states] = args;
            logical_cx = w;
            logical_cy = h;
            buffer_cx = logical_cx * scale120 / 120;
            buffer_cy = logical_cy * scale120 / 120;
            if (buffer_cx * buffer_cy > 0) {
                primary = shm_allocate_buffer(shm, buffer_cx, buffer_cy);
                secondary = shm_allocate_buffer(shm, buffer_cx, buffer_cy);
                primary_buffer.on<&wl_buffer_listener::release>(release_callback);
                secondary_buffer.on<&wl_buffer_listener::release>(release_callback);
            }
            else { // the initial configuration
                wl_surface_attach(surface, primary_buffer, 0, 0);
                wl_surface_commit(surface);
            }
            frame.on<&wl_callback_listener::done>([&](wl_callback*, std::uint32_t) MVLL_NOEXCEPT {
                frame.rebind(wl_surface_frame(surface));
                feedback.rebind(wp_presentation_feedback(presentation, surface));
                wl_surface_attach(surface, primary_buffer, 0, 0);
                wl_surface_damage_buffer(surface, 0, 0, buffer_cx, buffer_cy);
                wl_surface_commit(surface);
                wl_display_flush(display);
            });
        }
    };
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
