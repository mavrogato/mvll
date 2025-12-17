
#include <iostream>
#include <map>
#include <string_view>

#include <wayland-client-protocol.h>
#include <wayland-client.h>

#include <mvll/cpp2x/generator.hpp>
#include <mvll/error-handling.hpp>
#include <mvll/unique.hpp>
#include <mvll/platform/linux.hpp>

#include <xdg-shell-client.h>

//#include "zwp-tablet-v2-client.h"
//#include "zwp-linux-dmabuf-v1-client.h"
//#include "wp-fractional-scale-v1-client.h"


int display_run() {
    auto display = mvll::make_unique<wl_display_connect, wl_display_disconnect>(nullptr);
    auto registry = mvll::make_unique<wl_display_get_registry, wl_registry_destroy>(display.get());

    /////////////////////////////////////////////////////////////////////////////
    std::map<std::uint32_t, std::tuple<std::string, std::uint32_t>> globals;
    {
        static constexpr auto listener = wl_registry_listener {
            .global = [](void* data,
                         [[maybe_unused]] wl_registry* registry,
                         std::uint32_t name,
                         char const* interface,
                         std::uint32_t version) MVLL_NOEXCEPT
            {
                reinterpret_cast<decltype (globals)*>(data)->insert({name, {interface, version}});
            },
            .global_remove = [](void* data,
                                [[maybe_unused]] wl_registry* registry,
                                std::uint32_t name) MVLL_NOEXCEPT
            {
                reinterpret_cast<decltype (globals)*>(data)->erase(name);
            },
        };
        wl_registry_add_listener(registry.get(), &listener, &globals);
        wl_display_roundtrip(display.get());
    }
    for (auto entry : globals) {
        std::cout << entry.first << ':'
                  << std::get<0>(entry.second) << ':'
                  << std::get<1>(entry.second) << std::endl;
    }

    return 0;
}
