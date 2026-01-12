
#include <mvll/wayland/client/reflector.hpp>

#include <iostream>

#include <mvll/cpp2x/tuple-support.hpp>

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <type_traits>

    struct any_type {
        template <class T>
        constexpr operator T() const noexcept;
    };

    template <class T>
    concept aggregate = std::is_aggregate_v<T>;

    template <class T, std::size_t N>
    concept aggregate_initializable = aggregate<T> && requires {
        []<std::size_t... I>(std::index_sequence<I...>) -> decltype(T{ ((void)I, any_type{})... }) {
            return {};
        }(std::make_index_sequence<N>{});
    };

    template <class T>
    consteval std::size_t count_members() {
        if constexpr (aggregate_initializable<T, 16>) return 16;
        else if constexpr (aggregate_initializable<T, 15>) return 15;
        else if constexpr (aggregate_initializable<T, 14>) return 14;
        else if constexpr (aggregate_initializable<T, 13>) return 13;
        else if constexpr (aggregate_initializable<T, 12>) return 12;
        else if constexpr (aggregate_initializable<T, 11>) return 11;
        else if constexpr (aggregate_initializable<T, 10>) return 10;
        else if constexpr (aggregate_initializable<T, 9>) return 9;
        else if constexpr (aggregate_initializable<T, 8>) return 8;
        else if constexpr (aggregate_initializable<T, 7>) return 7;
        else if constexpr (aggregate_initializable<T, 6>) return 6;
        else if constexpr (aggregate_initializable<T, 5>) return 5;
        else if constexpr (aggregate_initializable<T, 4>) return 4;
        else if constexpr (aggregate_initializable<T, 3>) return 3;
        else if constexpr (aggregate_initializable<T, 2>) return 2;
        else if constexpr (aggregate_initializable<T, 1>) return 1;
        else return 0;
    }
    
// (defun generate-cpp-to-tuple (max-count)
//   (format t "template <typename T>~%auto to_tuple(T&& s) {~%")
//   (format t "    constexpr std::size_t count = count_members<std::decay_t<T>>();~%")
//   (loop for i from 1 to max-count
//         do (let ((vars (loop for j from 1 to i collect (format nil "~a" (code-char (+ -1 (char-code #\a) j))))))
//              (format t "    ~A MVLL_TO_TUPLE_BRANCH(~D, ~{~A~^, ~})~%"
//                      (if (= i 1) "" "else")
//                      i
//                      vars)))
//   (format t "    else { static_assert(count <= ~D, \"count <= ~D\"); }~%" max-count max-count)
//   (format t "}~%"))
template <class T>
auto to_tuple(T&& s) {
    constexpr std::size_t count = count_members<std::decay_t<T>>();
#define MVLL_TO_TUPLE_BRANCH(N, ...) \
    if constexpr (count == N) { \
        auto&& [__VA_ARGS__] = std::forward<T>(s); \
        return std::make_tuple(__VA_ARGS__); \
    }
    MVLL_TO_TUPLE_BRANCH(1, a)
    else MVLL_TO_TUPLE_BRANCH(2, a, b)
    else MVLL_TO_TUPLE_BRANCH(3, a, b, c)
    else MVLL_TO_TUPLE_BRANCH(4, a, b, c, d)
    else MVLL_TO_TUPLE_BRANCH(5, a, b, c, d, e)
    else MVLL_TO_TUPLE_BRANCH(6, a, b, c, d, e, f)
    else MVLL_TO_TUPLE_BRANCH(7, a, b, c, d, e, f, g)
    else MVLL_TO_TUPLE_BRANCH(8, a, b, c, d, e, f, g, h)
    else MVLL_TO_TUPLE_BRANCH(9, a, b, c, d, e, f, g, h, i)
    else MVLL_TO_TUPLE_BRANCH(10, a, b, c, d, e, f, g, h, i, j)
    else MVLL_TO_TUPLE_BRANCH(11, a, b, c, d, e, f, g, h, i, j, k)
    else MVLL_TO_TUPLE_BRANCH(12, a, b, c, d, e, f, g, h, i, j, k, l)
    else MVLL_TO_TUPLE_BRANCH(13, a, b, c, d, e, f, g, h, i, j, k, l, m)
    else MVLL_TO_TUPLE_BRANCH(14, a, b, c, d, e, f, g, h, i, j, k, l, m, n)
    else MVLL_TO_TUPLE_BRANCH(15, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o)
    else MVLL_TO_TUPLE_BRANCH(16, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)
    else { static_assert(count <= 16, "count <= 16"); }
#undef MVLL_TO_TUPLE_BRANCH
}

TEST_CASE("proxy interner", "[mvll][wayland][client][reflector]") {
    using namespace mvll;
    std::cout << proxy_meta_info<wl_display>::name << std::endl;
    std::cout << proxy_meta_info<wl_display>::deleter << std::endl;
    std::cout << to_tuple(*proxy_meta_info<wl_display>::interface_ptr) << std::endl;
    std::cout << to_tuple(*proxy_meta_info<wl_registry>::interface_ptr) << std::endl;
    std::cout << to_tuple(*proxy_meta_info<wl_compositor>::interface_ptr) << std::endl;
    std::cout << typeid (proxy_meta_info<wl_display>::listener_type).name() << std::endl;
    std::cout << typeid (proxy_meta_info<wl_registry>::listener_type).name() << std::endl;
    std::cout << typeid (proxy_meta_info<wl_compositor>::listener_type).name() << std::endl;
    
}


// struct hoge {
//     int i;
//     char const* s;
//     double d;
// };
// constexpr auto x = hoge{42, "pi", 3.1416};
// // auto [i, s, d] = x;
// static_assert(x.i == 42);

// TEST_CASE("test", "test") {
//     SECTION("1") {
//         if constexpr (requires {
//                 []<class T>(T&&) -> decltype(
//                     [](auto&& t) -> void {
//                         auto&& [a, b, c, d] = t;
//                     }(std::declval<T>())
//                     ) { return {}; }(hoge{});
//             })
//         {
//             std::cout << "Hi." << std::endl;
//         }
//         else {
//             std::cout << "Hogee" << std::endl;
//         }
//     }
// }
