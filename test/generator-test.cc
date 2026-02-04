
#include <mvll/cpp2x/generator.hpp>

#include <initializer_list>
#include <ranges>
#include <vector>

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>


TEST_CASE("simple generator", "[mvll][cpp2x][generator]") {
    auto enoch = []() -> mvll::cpp2x::generator<int> {
        co_yield 1;
        co_yield 2;
        co_yield 3;
        throw "Daaaaaaah!";
    };
    std::vector<int> buf;
    REQUIRE_THROWS_WITH([&](){
        for (auto&& item : enoch()) {
            buf.push_back(item);
        }
        REQUIRE(false);
    }(), "Daaaaaaah!");
    REQUIRE(buf == std::vector{1,2,3});
}

TEST_CASE("elements generator", "[mvll][cpp2x][generator]") {
    auto squares = [](std::initializer_list<int> src) -> mvll::cpp2x::generator<int> {
        auto squared = src | std::views::transform([](auto item) {
            return item *= item;
        });
        co_yield mvll::cpp2x::elements_of_adaptor{squared};
    };
    std::vector<int> buf;
    for (auto item : squares({10, 20, 30})) {
        buf.push_back(item);
    }
    REQUIRE(buf == std::vector{100, 400, 900});
}

TEST_CASE("recursive generator", "[mvll][cpp2x][generator]") {
    auto fibonacci = []<class Rec>(this Rec&& rec, int n0 = 0, int n1 = 1) -> mvll::cpp2x::generator<int> {
        co_yield n0;
        co_yield mvll::cpp2x::elements_of_adaptor{std::forward<Rec>(rec)(n1, n0 + n1)};
    };
    std::vector<int> buf;
    for (auto item : fibonacci(0, 1)) {
        if (100 < item) break;
        buf.push_back(item);
    }
    REQUIRE(buf == std::vector{0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89});
}

///  Note: Generator reference type must be either a cv-unqualified object type
///        that is trivially constructible or a reference type
TEST_CASE("generator with move-only types", "[mvll][cpp2x][generator]") {
    using Ptr = std::unique_ptr<int>;
    auto ptr_generator = []() -> mvll::cpp2x::generator<Ptr&&> {
        co_yield std::make_unique<int>(1);
        co_yield std::make_unique<int>(2);
        co_yield std::make_unique<int>(3);
    };
    std::vector<int> buf;
    for (auto&& item : ptr_generator()) { 
        buf.emplace_back(*item);
    }
    REQUIRE(buf == std::vector{1, 2, 3});
}

TEST_CASE("deep recursion (stackless check)", "[mvll][cpp2x][generator]") {
    auto deep_gen = []<class Rec>(this Rec&& rec, int count) -> mvll::cpp2x::generator<int> {
        if (count > 0) {
            co_yield count;
            co_yield mvll::cpp2x::elements_of_adaptor{std::forward<Rec>(rec)(count - 1)};
        }
    };
    const std::int64_t depth = 10000;
    int sum = 0;
    for (auto item : deep_gen(depth)) {
        sum += item;
    }
    REQUIRE(sum == depth * (depth + 1) / 2);
}

TEST_CASE("empty generator behavior", "[mvll][cpp2x][generator]") {
    auto empty_gen = []() -> mvll::cpp2x::generator<int> {
        co_return;
    };
    SECTION("empty iteration") {
        std::vector<int> buf;
        for (auto item : empty_gen()) {
            buf.push_back(item);
        }
        REQUIRE(buf.empty());
    }
    SECTION("confirm emptiness") {
        auto gen = empty_gen();
        REQUIRE(gen.begin() == gen.end());
    }
}

TEST_CASE("generator move operations", "[mvll][cpp2x][generator]") {
    auto source_gen = []() -> mvll::cpp2x::generator<int> {
        co_yield 10;
        co_yield 20;
    };
    mvll::cpp2x::generator<int> gen1 = source_gen();
    mvll::cpp2x::generator<int> gen2 = std::move(gen1); 
    std::vector<int> buf;
    for (auto item : gen2) {
        buf.push_back(item);
    }
    REQUIRE(buf == std::vector{10, 20});
}

#include <iostream>
#include <complex>
TEST_CASE("conceptual cooperation", "[mvll][cpp2x][generator]") {
    using namespace mvll::cpp2x;

    std::complex<float> cursor;

    auto event_sequence = [&](std::vector<generator<bool>>&& subevents) -> generator<bool> {
        for (auto& event : subevents) {
            co_yield mvll::elements_of_adaptor(event);
        }
    };
    auto detect_starting_drag = [&]() -> generator<bool> {
        auto pivot = cursor;
        while (std::abs(cursor - pivot) < 16.0f) co_yield false;
        // std::cout << pivot << std::endl;
        // std::cout << cursor << std::endl;
        // std::cout << std::abs(cursor - pivot) << std::endl;
        co_yield true;
    };
    std::vector<generator<bool>> subevents;
    subevents.emplace_back(detect_starting_drag());

    for (int count = 0; bool quit : event_sequence(std::move(subevents))) {
        if (quit) break;
        cursor = {1.0f*count, 2.0f*count};
        ++count;
    }
}

// TEST_CASE("with awaiter", "[mvll][cpp2x][generator]") {
//     std::coroutine_handle<> current_suspending_handle{};
//     void* latest_args_raw{};
//     auto wait = [&]{
//         struct awaiter {
//             std::coroutine_handle<>& handle;
//             void* args_raw;
//             bool await_ready() const noexcept { return false; }
//             void await_suspend(std::coroutine_handle<> h) {
//                 this->handle = h;
//             }
//             void* await_resume() const noexcept {
//                 return this->args_raw;
//             }
//         };
//         return awaiter{current_suspending_handle, latest_args_raw};
//     };
//     auto coro_with_wait = [wait]() -> mvll::cpp2x::generator<int> {
//         co_yield 1;
//         co_yield 2;
//         co_await wait();
//         co_yield 3;
//     };
//     // for (auto item : coro_with_wait()) {
//     //     std::cout << item << std::endl;
//     // }
//     auto gen_with_wait = coro_with_wait();
//     auto driver = [&]<class Rec>(this Rec&& rec, int n = 10) -> mvll::cpp2x::generator<bool> {
//         if (n == 0) {
//             co_return;
//         }
//         co_yield true;
//         for (auto item : gen_with_wait) { std::cout << item << std::endl; }
//         //co_yield mvll::cpp2x::elements_of_adaptor{std::forward<Rec>(rec)(n - 1)};
//     };
//     for (auto item : driver()) {
//         std::cout << std::boolalpha << item << ":" << current_suspending_handle.address() << std::endl;
//     }
// }
