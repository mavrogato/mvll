/**
 * @file tuple_support.hpp
 * @brief Provides C++20 concepts and stream insertion operator for tuple-like types.
 *
 * This header defines the `mvll::tuple_like` concept, which can be used to constrain
 * generic functions to work with standard tuples, std::pair, and user-defined
 * types that support std::tuple_size and ADL-enabled get<I>().
 *
 * It also provides a generic `operator<<` implementation for these types,
 * outputting them in the format (e1, e2, ...).
 *
 * @author Nakashima, Terumi
 * @date 2025-11-16
 * @version 1.0
 */
#ifndef INCLUDE_MVLL_CPP2X_TUPLE_SUPPORT_HPP
#define INCLUDE_MVLL_CPP2X_TUPLE_SUPPORT_HPP

#include <concepts>
#include <iosfwd>
#include <tuple>
#include <utility>

#include <cstddef>

namespace mvll::inline cpp2x
{
    namespace internals
    {
        /**
         * @brief Concept to check for valid tuple element access via get<I>(t).
         *
         * This relies on Argument-Dependent Lookup (ADL) to find the appropriate
         * `get` function (e.g., in std namespace for std::tuple/std::pair, or
         * in the user's namespace for custom types).
         */
        template <class T, std::size_t I>
        concept has_tuple_element_access = requires(T t) {
            typename std::tuple_element_t<I, std::remove_const_t<T>>;
            requires std::is_reference_v<decltype (get<I>(t))>;
            requires std::same_as<
                std::remove_reference_t<decltype (get<I>(t))>,
                std::tuple_element_t<I, std::remove_const_t<T>>>;
        };
    } // ::internals

    /**
     * @brief Concept defining a type that behaves like a std::tuple (C++20 style).
     *
     * This implementation anticipates the C++23 std::tuple_like concept,
     * ensuring that the type provides std::tuple_size and element access
     * for all indices [0, N-1] via ADL-enabled get<I>().
     */
    template <class T>
    concept tuple_like = requires(T) {
        std::tuple_size<T>::value;
        requires std::derived_from<
            std::tuple_size<T>,
            std::integral_constant<std::size_t, std::tuple_size_v<T>>
            >;
    } && []<std::size_t... I>(std::index_sequence<I...>) noexcept {
        return (internals::has_tuple_element_access<T, I>&& ...);
    }(std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<T>>>());

    /**
     * @brief Stream insertion operator for mvll::cpp2x::tuple_like types.
     *
     * Uses C++20 template syntax (concepts, generic lambda) for generic access.
     * The return type uses 'auto&' for conciseness over the traditional explicit type.
     * Output format is a lisp s-expression likes: (element1 element2 ...).
     */
    template <class Ch, class Tr, mvll::tuple_like T>
    auto& operator<<(std::basic_ostream<Ch, Tr>& output, T const& t) {
        output.put('(');
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            using std::get;
            auto sep = "";
            ((output << sep << get<I>(t), sep = " "), ...);
        }(std::make_index_sequence<std::tuple_size_v<T>>());
        output.put(')');
        return output;
    }
} // ::mvll::cpp2x

#endif // INCLUDE_MVLL_CPP2X_TUPLE_SUPPORT_HPP
