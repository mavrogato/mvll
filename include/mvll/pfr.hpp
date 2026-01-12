#ifndef INCLUDE_MVLL_PFR_HPP
#define INCLUDE_MVLL_PFR_HPP

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

        template <class T, std::size_t N>
        concept aggregate_initializable = aggregate<T> && requires {
            []<std::size_t... I>(std::index_sequence<I...>) -> decltype (T{((void)I, any_type{})...}) {
                return {};
            }(std::make_index_sequence<N>{});
        };
    } // ::internals

    template <class T> consteval std::uint32_t count_members() {
        if constexpr      (internals::aggregate_initializable<T, 64>) return 64;
        else if constexpr (internals::aggregate_initializable<T, 63>) return 63;
        else if constexpr (internals::aggregate_initializable<T, 62>) return 62;
        else if constexpr (internals::aggregate_initializable<T, 61>) return 61;
        else if constexpr (internals::aggregate_initializable<T, 60>) return 60;
        else if constexpr (internals::aggregate_initializable<T, 59>) return 59;
        else if constexpr (internals::aggregate_initializable<T, 58>) return 58;
        else if constexpr (internals::aggregate_initializable<T, 57>) return 57;
        else if constexpr (internals::aggregate_initializable<T, 56>) return 56;
        else if constexpr (internals::aggregate_initializable<T, 55>) return 55;
        else if constexpr (internals::aggregate_initializable<T, 54>) return 54;
        else if constexpr (internals::aggregate_initializable<T, 53>) return 53;
        else if constexpr (internals::aggregate_initializable<T, 52>) return 52;
        else if constexpr (internals::aggregate_initializable<T, 51>) return 51;
        else if constexpr (internals::aggregate_initializable<T, 50>) return 50;
        else if constexpr (internals::aggregate_initializable<T, 49>) return 49;
        else if constexpr (internals::aggregate_initializable<T, 48>) return 48;
        else if constexpr (internals::aggregate_initializable<T, 47>) return 47;
        else if constexpr (internals::aggregate_initializable<T, 46>) return 46;
        else if constexpr (internals::aggregate_initializable<T, 45>) return 45;
        else if constexpr (internals::aggregate_initializable<T, 44>) return 44;
        else if constexpr (internals::aggregate_initializable<T, 43>) return 43;
        else if constexpr (internals::aggregate_initializable<T, 42>) return 42;
        else if constexpr (internals::aggregate_initializable<T, 41>) return 41;
        else if constexpr (internals::aggregate_initializable<T, 40>) return 40;
        else if constexpr (internals::aggregate_initializable<T, 39>) return 39;
        else if constexpr (internals::aggregate_initializable<T, 38>) return 38;
        else if constexpr (internals::aggregate_initializable<T, 37>) return 37;
        else if constexpr (internals::aggregate_initializable<T, 36>) return 36;
        else if constexpr (internals::aggregate_initializable<T, 35>) return 35;
        else if constexpr (internals::aggregate_initializable<T, 34>) return 34;
        else if constexpr (internals::aggregate_initializable<T, 33>) return 33;
        else if constexpr (internals::aggregate_initializable<T, 32>) return 32;
        else if constexpr (internals::aggregate_initializable<T, 31>) return 31;
        else if constexpr (internals::aggregate_initializable<T, 30>) return 30;
        else if constexpr (internals::aggregate_initializable<T, 29>) return 29;
        else if constexpr (internals::aggregate_initializable<T, 28>) return 28;
        else if constexpr (internals::aggregate_initializable<T, 27>) return 27;
        else if constexpr (internals::aggregate_initializable<T, 26>) return 26;
        else if constexpr (internals::aggregate_initializable<T, 25>) return 25;
        else if constexpr (internals::aggregate_initializable<T, 24>) return 24;
        else if constexpr (internals::aggregate_initializable<T, 23>) return 23;
        else if constexpr (internals::aggregate_initializable<T, 22>) return 22;
        else if constexpr (internals::aggregate_initializable<T, 21>) return 21;
        else if constexpr (internals::aggregate_initializable<T, 20>) return 20;
        else if constexpr (internals::aggregate_initializable<T, 19>) return 19;
        else if constexpr (internals::aggregate_initializable<T, 18>) return 18;
        else if constexpr (internals::aggregate_initializable<T, 17>) return 17;
        else if constexpr (internals::aggregate_initializable<T, 16>) return 16;
        else if constexpr (internals::aggregate_initializable<T, 15>) return 15;
        else if constexpr (internals::aggregate_initializable<T, 14>) return 14;
        else if constexpr (internals::aggregate_initializable<T, 13>) return 13;
        else if constexpr (internals::aggregate_initializable<T, 12>) return 12;
        else if constexpr (internals::aggregate_initializable<T, 11>) return 11;
        else if constexpr (internals::aggregate_initializable<T, 10>) return 10;
        else if constexpr (internals::aggregate_initializable<T,  9>) return  9;
        else if constexpr (internals::aggregate_initializable<T,  8>) return  8;
        else if constexpr (internals::aggregate_initializable<T,  7>) return  7;
        else if constexpr (internals::aggregate_initializable<T,  6>) return  6;
        else if constexpr (internals::aggregate_initializable<T,  5>) return  5;
        else if constexpr (internals::aggregate_initializable<T,  4>) return  4;
        else if constexpr (internals::aggregate_initializable<T,  3>) return  3;
        else if constexpr (internals::aggregate_initializable<T,  2>) return  2;
        else if constexpr (internals::aggregate_initializable<T,  1>) return  1;
        else return 0;
    }

    template <class T>
    constexpr auto to_tuple(T&& s) {
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

    template <typename T, auto Member>
    consteval std::uint32_t get_ordinal() {
        constexpr T prototype = []() {
            T t{};
            t.*Member = [](auto...){}; 
            return t;
        }();
        auto [...args] = prototype;
        std::size_t index = 0;
        std::size_t found_index = 0;
        bool found = false;
        (void) ((args != nullptr ? (found = true, found_index = index, true) : (++index, false)) || ...);
        if (!found) throw "Member not found";
        return found_index;
    }

    template <class T> struct member_pointer_traits;
    template <class R, class T>
    struct member_pointer_traits<R T::*> {
        using class_pointer_type = T;
        using class_type = std::remove_pointer_t<T>;
        using member_type = R;
    };

    template <auto Member>
    inline constexpr std::uint32_t ordinal = [] noexcept {
        using T = member_pointer_traits<decltype (Member)>::class_type;
        return get_ordinal<T, Member>();
    }();

} // ::mvll::pfr

#endif /*INCLUDE_MVLL_PFR_HPP*/
