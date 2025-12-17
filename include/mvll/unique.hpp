#ifndef INCLUDE_MVLL_UNIQUE_HPP
#define INCLUDE_MVLL_UNIQUE_HPP

#include <memory>
#include <type_traits>

namespace mvll
{
    template <auto D, class T>
    [[nodiscard]] constexpr auto make_unique(T* raw = nullptr)  {
        return std::unique_ptr<T, decltype (D)>{raw, D};
    }
    template <auto C, auto D, class... Rest>
    [[nodiscard]] constexpr auto make_unique(Rest&&... args) {
        using T = std::remove_pointer_t<decltype (C(args...))>;
        return make_unique<D, T>(C(std::forward<Rest>(args)...));
    }
} // ::mvll

#endif /* INCLUDE_MVLL_UNIQUE_HPP */
