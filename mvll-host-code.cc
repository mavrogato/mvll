
#include <generator>
#include <iostream>
#include <map>
#include <memory>
#include <string_view>
#include <thread>
#include <tuple>

#include <wayland-client-protocol.h>
#include <wayland-client.h>

#include <mvll/cpp2x/functional.hpp>
#include <mvll/cpp2x/tuple-support.hpp>
#include <mvll/cpp2x/generator.hpp>
#include <mvll/error-handling.hpp>
#include <mvll/platform/linux.hpp>

#include <wp-fractional-scale-v1-client.h>
#include <xdg-shell-client.h>
#include <zwp-pointer-constraints-v1-client.h>
#include <zwp-tablet-v2-client.h>

// #include "zwp-tablet-v2-client.h"
// #include "zwp-linux-dmabuf-v1-client.h"
// #include "wp-fractional-scale-v1-client.h"

#if 0
using mvll::cpp2x::operator<<;

int display_run() MVLL_NOEXCEPT {
    auto display = mvll::make_unique<wl_display_connect, wl_display_disconnect>(nullptr);
    MVLL_CHECK(display);
    auto registry = mvll::make_unique<wl_display_get_registry, wl_registry_destroy>(display.get());
    MVLL_CHECK(registry);

    struct constructor_ctx {
        std::uint32_t name;
        wl_interface const* interface_ptr;
        std::uint32_t version;
    };

    auto registry_fiblet = [&]<class REC>(this REC rec)
        -> mvll::cpp2x::generator<constructor_ctx&> {
        constructor_ctx ctx;
        co_yield ctx;
        if (ctx.interface_ptr) {
            std::cout << ctx.interface_ptr->name << std::endl;
        }
        co_yield mvll::cpp2x::elements_of_adaptor{rec()};
    }();

    auto iter = registry_fiblet.begin();
    auto listener = wl_registry_listener {
        .global = [](void* data, wl_registry*, std::uint32_t name, char const* interface, std::uint32_t version) {
            auto& p = *reinterpret_cast<decltype (iter)*>(data);
            if (std::string_view(wl_compositor_interface.name) == interface) {
                *p = constructor_ctx{name, &wl_compositor_interface, version};
                ++p;
            }
            else if (std::string_view(xdg_wm_base_interface.name) == interface) {
                *p = constructor_ctx{name, &xdg_wm_base_interface, version};
                ++p;
            }
            else if (std::string_view(wl_shm_interface.name) == interface) {
                *p = constructor_ctx{name, &wl_shm_interface, version};
                ++p;
            }
            else if (std::string_view(wl_seat_interface.name) == interface) {
                *p = constructor_ctx{name, &wl_seat_interface, version};
                ++p;
            }
            else if (std::string_view(wl_output_interface.name) == interface) {
                *p = constructor_ctx{name, &wl_output_interface, version};
                ++p;
            }
            else if (std::string_view(zwp_pointer_constraints_v1_interface.name) == interface) {
                *p = constructor_ctx{name, &zwp_pointer_constraints_v1_interface, version};
                ++p;
            }
            else if (std::string_view(zwp_tablet_manager_v2_interface.name) == interface) {
                *p = constructor_ctx{name, &zwp_tablet_manager_v2_interface, version};
                ++p;
            }
            else if (std::string_view(wp_fractional_scale_manager_v1_interface.name) == interface) {
                *p = constructor_ctx{name, &wp_fractional_scale_manager_v1_interface, version};
                ++p;
            }
        },
        .global_remove = [](void* data, wl_registry*, std::uint32_t name) {
            auto& p = *reinterpret_cast<decltype (iter)*>(data);
            *p = constructor_ctx{name, {}, {}};
            p++;
        },
    };
    wl_registry_add_listener(registry.get(), &listener, &iter);
    wl_display_roundtrip(display.get());

    return 0;
}
#endif

#if 0
enum class context : std::uint32_t {
    ignore = 0,
    construct,
    destruct,
    command,
    done,
    quit = 0xffffffff,
};

template <class WL_CLIENT>
inline auto global_fiblet(std::uint32_t name, void* bound, bool& is_alive) -> mvll::cpp2x::generator<WL_CLIENT*> {
    while (is_alive) {
        co_yield reinterpret_cast<WL_CLIENT*>(bound);
    }
    wl_proxy_destroy(reinterpret_cast<wl_proxy*>(bound));
}

int display_run() {
    auto display = mvll::make_unique<wl_display_connect, wl_display_disconnect>(nullptr);
    auto registry = mvll::make_unique<wl_display_get_registry, wl_registry_destroy>(display.get());

    auto registry_args = std::tuple<std::uint32_t, std::string, std::uint32_t>{};

    auto registry_fiblet = [&]<class REC>(this REC rec)
        -> mvll::cpp2x::generator<context&> {
        bool is_alive = false;
        
        context ctx = context::ignore;
        for (;;) {
            co_yield ctx;
            switch (ctx) {
            case context::ignore:
                break;
            case context::construct: {
                auto [name, interface, version] = registry_args;
                if (interface == wl_compositor_interface.name) {
                    void* bound = wl_registry_bind(registry.get(), name, &wl_compositor_interface, version);
                    MVLL_CHECK(bound);
                    auto compositor = global_fiblet<wl_compositor>(name, bound, is_alive);
                    is_alive = true;
                    co_yield mvll::cpp2x::elements_of_adaptor{rec()};
                }
                if (interface == xdg_wm_base_interface.name) {
                    is_alive = true;
                    co_yield mvll::cpp2x::elements_of_adaptor{rec()};
                }
                break;
            }
            case context::destruct: {
                is_alive = false;
                break;
            }
            case context::command:
                break;
            case context::done:
                break;
            case context::quit:
                co_return;
            }
        }
    }();
    auto iter = registry_fiblet.begin();
    auto datum = std::tuple<decltype (iter)&, decltype (registry_args)&>{iter, registry_args};
    auto listener = wl_registry_listener {
        .global = [](void* data, wl_registry*, auto... rest) {
            auto& [iter, args] = *reinterpret_cast<decltype (datum)*>(data);
            args = {rest...};
            *iter = context::construct;
            iter++;
        },
        .global_remove = [](void* data, wl_registry*, uint32_t name) {
            auto& [iter, args] = *reinterpret_cast<decltype (datum)*>(data);
            args = {name, {}, {}};
            *iter = context::destruct;
            iter++;
        },
    };
    wl_registry_add_listener(registry.get(), &listener, &datum);
    wl_display_roundtrip(display.get());

    return 0;
}
#endif

#if 0
int display_run() {
    using mvll::cpp2x::operator<<;
    auto display = mvll::make_unique<wl_display_connect, wl_display_disconnect>(nullptr);
    auto registry = mvll::make_unique<wl_display_get_registry, wl_registry_destroy>(display.get());

    auto args = std::tuple<std::uint32_t, std::string, std::uint32_t>{};
    auto globals_fiblet = [&]() -> mvll::cpp2x::generator<bool&> {
        for (;;) {
            bool ret = true;
            co_yield ret;
            std::cout << ret << std::endl;
            std::cout << args << std::endl;
        }
    }();
    auto iter = globals_fiblet.begin();
    auto datum = std::tuple<decltype (iter)&, decltype (args)&>{iter, args};
    auto listener = wl_registry_listener {
        .global = [](void* data, wl_registry*, [[maybe_unused]] auto... rest) {
            auto& [iter, args] = *reinterpret_cast<decltype (datum)*>(data);
            if (*iter == true) {
                args = std::tuple{rest...};
                *iter = false;
            }
            iter++;
        },
        .global_remove = [](auto...) { },
    };
    wl_registry_add_listener(registry.get(), &listener, &datum);
    wl_display_roundtrip(display.get());

    return 0;
}
#endif

#if 0
int display_run() {
    using mvll::cpp2x::operator<<;
    auto display = mvll::make_unique<wl_display_connect, wl_display_disconnect>(nullptr);
    auto registry = mvll::make_unique<wl_display_get_registry, wl_registry_destroy>(display.get());

    auto globals_fiber = []() -> mvll::cpp2x::generator<bool> {
        std::cout << "begins..." << std::endl;
        std::cout << "move 1" << std::endl;
        for (;;) {
            co_yield true;
        }
    };
    auto iter = globals_fiber().begin();
    //iter++;

    /////////////////////////////////////////////////////////////////////////////
    auto listener = wl_registry_listener {
        .global = [](void* data, [[maybe_unused]] auto... rest) {
            auto& p = *reinterpret_cast<decltype (iter)*>(data);
            if (*p == true) {
                std::cout << "Hi" << std::endl;
            }
        },
        .global_remove = [](void* data, [[maybe_unused]] auto... rest) {
            [[maybe_unused]] auto& p = *reinterpret_cast<decltype (iter)*>(data);
        },
    };
    wl_registry_add_listener(registry.get(), &listener, &iter);
    wl_display_roundtrip(display.get());

    return 0;
}
#endif

#if 0
int display_run() {
    using mvll::cpp2x::operator<<;
    auto display = mvll::make_unique<wl_display_connect, wl_display_disconnect>(nullptr);
    auto registry = mvll::make_unique<wl_display_get_registry, wl_registry_destroy>(display.get());

    /////////////////////////////////////////////////////////////////////////////
    auto datum = std::tuple<uint32_t, std::string, uint32_t>{};
    auto global_gen = [&]()
        -> mvll::cpp2x::generator<void (*)(void*,
                                           wl_registry*,
                                           uint32_t,
                                           char const*,
                                           uint32_t)> {
        for (int i = 0;; ++i) {
            std::cout << i << ":" << datum << std::endl;
            static constexpr auto cb = [](void* data, wl_registry*, auto... rest) {
            };
            co_yield cb;
            std::cout << datum << std::endl;
        }
    };
    auto iter = global_gen().begin();
    auto listener = wl_registry_listener {
        .global = [](void* data, auto... rest) {
            (*(++*reinterpret_cast<decltype (iter)*>(data)))(data, rest...);
        },
        .global_remove = [](auto...) { },
    };
    wl_registry_add_listener(registry.get(), &listener, &iter);
    wl_display_roundtrip(display.get());

    return 0;
}
#endif
