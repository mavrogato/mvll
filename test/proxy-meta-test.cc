
#include <xdg-shell-client.h>
#define MVLL_PROXY_LIST(V) \
    V(xdg_wm_base)
#include <mvll/wayland/client/proxy-meta.hpp>

#include <iostream>

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("enumeration", "[mvll][wayland][client][proxy-meta]") {
    static_assert(static_cast<std::uint32_t>(mvll::proxy_id::wl_display_id) == 0);
    static_assert(static_cast<std::uint32_t>(mvll::proxy_id::wl_registry_id) == 1);
    static_assert(static_cast<std::uint32_t>(mvll::proxy_id::wl_compositor_id) == 2);
    static_assert(static_cast<std::uint32_t>(mvll::proxy_id::wl_shm_id) == 3);
    static_assert(static_cast<std::uint32_t>(mvll::proxy_id::xdg_wm_base_id) == 4);

    REQUIRE(static_cast<std::uint32_t>(mvll::proxy_id::wl_display_id) == 0);
    REQUIRE(static_cast<std::uint32_t>(mvll::proxy_id::wl_registry_id) == 1);
    REQUIRE(static_cast<std::uint32_t>(mvll::proxy_id::wl_compositor_id) == 2);
    REQUIRE(static_cast<std::uint32_t>(mvll::proxy_id::wl_shm_id) == 3);
    REQUIRE(static_cast<std::uint32_t>(mvll::proxy_id::xdg_wm_base_id) == 4);

    std::cout << "----------" << std::endl;
    for (auto const& item : mvll::metadb) {
        std::cout << item.name << '\t' << item.has_listener << std::endl;
    }
    std::cout << "----------" << std::endl;

    std::cout << mvll::metainfo<wl_registry>.name << std::endl;
}

