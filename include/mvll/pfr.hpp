#ifndef INCLUDE_MVLL_PFR_HPP
#define INCLUDE_MVLL_PFR_HPP

#include <mvll/error-handling.hpp>

#include <tuple>
#include <type_traits>
#include <utility>

#include <cstddef>
#include <cstdint>

namespace mvll::inline pfr
{
    namespace internals
    {
        struct any_type {
            template <class T> operator T&() const&& noexcept;
        };

        template <class T> concept aggregate = std::is_aggregate_v<T>;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
        template <class T, std::size_t... I> auto check_aggregate_init(std::index_sequence<I...>)
            -> decltype(T{ ((void)I, any_type{})... }, std::true_type{});
#pragma GCC diagnostic pop
        template <class T, std::size_t... I> std::false_type check_aggregate_init(...);

        template <class T, std::size_t N>
        concept aggregate_initializable = requires {
            { check_aggregate_init<T>(std::make_index_sequence<N>{}) } -> std::same_as<std::true_type>;
        };
    } // ::internals

    template <class T, std::size_t Min, std::size_t Max>
    [[nodiscard]] consteval std::size_t count_members_binary() noexcept {
        if constexpr (Min == Max) return Min;
        else {
            constexpr std::size_t Mid = Min + (Max - Min + 1) / 2;
            if constexpr (internals::aggregate_initializable<T, Mid>)
                return count_members_binary<T, Mid, Max>();
            else
                return count_members_binary<T, Min, Mid - 1>();
        }
    }
    template <class T>
    [[nodiscard]] consteval std::size_t count_members() noexcept {
        return count_members_binary<T, 0, 64>();
    }

    template <class T>
    constexpr auto to_tuple(T&& s) noexcept {
        constexpr std::uint32_t count = count_members<std::decay_t<T>>();
#define MVLL_TO_TUPLE_BRANCH(N)                                         \
        if constexpr (count == N) {                                     \
            auto [... args] = std::forward<T>(s);                       \
            return std::make_tuple(args...);                            \
        }
        MVLL_TO_TUPLE_BRANCH     ( 1)
        else MVLL_TO_TUPLE_BRANCH( 2)
        else MVLL_TO_TUPLE_BRANCH( 3)
        else MVLL_TO_TUPLE_BRANCH( 4)
        else MVLL_TO_TUPLE_BRANCH( 5)
        else MVLL_TO_TUPLE_BRANCH( 6)
        else MVLL_TO_TUPLE_BRANCH( 7)
        else MVLL_TO_TUPLE_BRANCH( 8)
        else MVLL_TO_TUPLE_BRANCH( 9)
        else MVLL_TO_TUPLE_BRANCH(10)
        else MVLL_TO_TUPLE_BRANCH(11)
        else MVLL_TO_TUPLE_BRANCH(12)
        else MVLL_TO_TUPLE_BRANCH(13)
        else MVLL_TO_TUPLE_BRANCH(14)
        else MVLL_TO_TUPLE_BRANCH(15)
        else MVLL_TO_TUPLE_BRANCH(16)
        else MVLL_TO_TUPLE_BRANCH(17)
        else MVLL_TO_TUPLE_BRANCH(18)
        else MVLL_TO_TUPLE_BRANCH(19)
        else MVLL_TO_TUPLE_BRANCH(20)
        else MVLL_TO_TUPLE_BRANCH(21)
        else MVLL_TO_TUPLE_BRANCH(22)
        else MVLL_TO_TUPLE_BRANCH(23)
        else MVLL_TO_TUPLE_BRANCH(24)
        else MVLL_TO_TUPLE_BRANCH(25)
        else MVLL_TO_TUPLE_BRANCH(26)
        else MVLL_TO_TUPLE_BRANCH(27)
        else MVLL_TO_TUPLE_BRANCH(28)
        else MVLL_TO_TUPLE_BRANCH(29)
        else MVLL_TO_TUPLE_BRANCH(30)
        else MVLL_TO_TUPLE_BRANCH(31)
        else MVLL_TO_TUPLE_BRANCH(32)
        else MVLL_TO_TUPLE_BRANCH(33)
        else MVLL_TO_TUPLE_BRANCH(34)
        else MVLL_TO_TUPLE_BRANCH(35)
        else MVLL_TO_TUPLE_BRANCH(36)
        else MVLL_TO_TUPLE_BRANCH(37)
        else MVLL_TO_TUPLE_BRANCH(38)
        else MVLL_TO_TUPLE_BRANCH(39)
        else MVLL_TO_TUPLE_BRANCH(40)
        else MVLL_TO_TUPLE_BRANCH(41)
        else MVLL_TO_TUPLE_BRANCH(42)
        else MVLL_TO_TUPLE_BRANCH(43)
        else MVLL_TO_TUPLE_BRANCH(44)
        else MVLL_TO_TUPLE_BRANCH(45)
        else MVLL_TO_TUPLE_BRANCH(46)
        else MVLL_TO_TUPLE_BRANCH(47)
        else MVLL_TO_TUPLE_BRANCH(48)
        else MVLL_TO_TUPLE_BRANCH(49)
        else MVLL_TO_TUPLE_BRANCH(50)
        else MVLL_TO_TUPLE_BRANCH(51)
        else MVLL_TO_TUPLE_BRANCH(52)
        else MVLL_TO_TUPLE_BRANCH(53)
        else MVLL_TO_TUPLE_BRANCH(54)
        else MVLL_TO_TUPLE_BRANCH(55)
        else MVLL_TO_TUPLE_BRANCH(56)
        else MVLL_TO_TUPLE_BRANCH(57)
        else MVLL_TO_TUPLE_BRANCH(58)
        else MVLL_TO_TUPLE_BRANCH(59)
        else MVLL_TO_TUPLE_BRANCH(60)
        else MVLL_TO_TUPLE_BRANCH(61)
        else MVLL_TO_TUPLE_BRANCH(62)
        else MVLL_TO_TUPLE_BRANCH(63)
        else MVLL_TO_TUPLE_BRANCH(64)
        else { static_assert(count <= 64, "count <= 64"); }
#undef MVLL_TO_TUPLE_BRANCH
    }

    template <class T> struct member_pointer_traits;
    template <class M, class S>
    struct member_pointer_traits<M S::*> {
        using struct_type = S;
        using member_type = std::remove_pointer_t<M>;
    };

    template <auto Member,
        typename member_pointer_traits<decltype (Member)>::member_type Mark
    > requires (std::is_member_pointer_v<decltype (Member)> &&
                std::is_aggregate_v<typename member_pointer_traits<decltype (Member)>::struct_type>)
    [[nodiscard]] consteval std::uint32_t get_ordinal() noexcept {
        using struct_type = typename member_pointer_traits<decltype (Member)>::struct_type;
        constexpr struct_type prototype = []() consteval {
            struct_type t{};
            t.*Member = Mark;
            return t;
        }();
        auto [...args] = prototype;
        std::size_t index = 0;
        std::size_t found_index = 0;
        bool found = false;
        (void) ((args != nullptr ? (found = true, found_index = index, true) : (++index, false)) || ...);
        MVLL_CHECK(found);
        return found_index;
    }

    template <auto Member,
        typename member_pointer_traits<decltype (Member)>::member_type Mark
    > requires (std::is_member_pointer_v<decltype (Member)> &&
                std::is_aggregate_v<typename member_pointer_traits<decltype (Member)>::struct_type>)
    inline constexpr std::uint32_t ordinal = [] consteval noexcept {
        return get_ordinal<Member, Mark>();
    }();

} // ::mvll::pfr

#endif /*INCLUDE_MVLL_PFR_HPP*/
