
#include <mvll/unique.hpp>

#include <catch2/catch_all.hpp>


TEST_CASE("basic usage", "[mvll][unque]") {
    static std::size_t count = 0;
    auto create = [](std::size_t sz) {
        ++count;
        return std::malloc(sz);
    };
    auto release = [](void* p) {
        std::free(p);
        --count;
    };
    {
        auto p = mvll::make_unique<create, release>(128);
        REQUIRE(p);
        REQUIRE(count == 1);
    }
    REQUIRE(count == 0);
    {
        auto p = mvll::make_unique<release>(create(128));
        REQUIRE(p);
        REQUIRE(count == 1);
    }
    REQUIRE(count == 0);
}
