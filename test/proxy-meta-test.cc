
#include <wayland-client-core.h>
#include <xdg-shell-client.h>
#define MVLL_PROXY_LIST(V) \
    V(xdg_wm_base)
#include <mvll/wayland/client/proxy-meta.hpp>

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("enumeration", "[mvll][wayland][client][proxy-meta]") {
    static_assert(static_cast<std::uint32_t>(mvll::proxy_id::wl_display_id) == 0);
    static_assert(static_cast<std::uint32_t>(mvll::proxy_id::xdg_wm_base_id) == mvll::NOF_PROXIES - 1);
    REQUIRE(static_cast<std::uint32_t>(mvll::proxy_id::wl_display_id) == 0);
    REQUIRE(static_cast<std::uint32_t>(mvll::proxy_id::xdg_wm_base_id) == mvll::NOF_PROXIES - 1);
}

TEST_CASE("display connect/disconnect via metadb", "[mvll][wayland][client][proxy-meta]") {
    auto const& info = mvll::metadb[0];
    SECTION("metadata validation") {
        REQUIRE(info.id == mvll::wayland::client::proxy_id::wl_display_id);
        REQUIRE(std::string_view(info.name) == "wl_display");
        REQUIRE(info.interface_ptr == &wl_display_interface);
        REQUIRE(info.deleter != nullptr);
    }
    SECTION("actual execution") {
        auto* display = wl_display_connect(nullptr);
        if (display) {
            info.deleter(display); 
            SUCCEED("Successfully disconnected wl_display via erased deleter");
        }
        else {
            SKIP("Wayland display not available");
        }
    }
}

TEST_CASE("registry via metadb", "[mvll][wayland][client][proxy-meta]") {
    constexpr auto rid = mvll::wayland::client::proxy_id::wl_registry_id;
    const auto& info = mvll::wayland::client::metadb[static_cast<size_t>(rid)];
    if (auto* display = wl_display_connect(nullptr)) {
        auto* registry = wl_display_get_registry(display);
        REQUIRE(registry);
        REQUIRE(info.deleter);
        REQUIRE(info.listener_adder);
        int count = 0;
        wl_registry_listener listener = {
            .global = [](void* data, auto...) {
                int& count = *static_cast<int*>(data);
                ++count;
            },
            .global_remove = [](auto...) {
            }
        };
        info.listener_adder(registry, &listener, &count);
        wl_display_roundtrip(display);
        REQUIRE(0 < count);
        info.deleter(registry);
        wl_display_disconnect(display);
        SUCCEED("Successfully destroyed wl_registry via erased deleter");
    }
    else {
        SKIP("Wayland display not available");
    }
}

TEST_CASE("dispatching", "[mvll][wayland][client][proxy-meta]") {
    mvll::dispatch_by_id(mvll::proxy_id::wl_display_id, []<class T>{
            REQUIRE(std::is_same_v<wl_display, T>);
            REQUIRE_FALSE(std::is_same_v<xdg_wm_base, T>);
        });
    mvll::dispatch_by_id(mvll::proxy_id::xdg_wm_base_id, []<class T>{
            REQUIRE_FALSE(std::is_same_v<wl_display, T>);
            REQUIRE(std::is_same_v<xdg_wm_base, T>);
        });
}

TEST_CASE("event traits", "[mvll][wayland][client][proxy-meta]") {
    //REQUIRE(mvll::event_traits<&wl_pointer_listener::enter>::ordinal == 0);
}
