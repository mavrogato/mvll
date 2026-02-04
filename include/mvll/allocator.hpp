#ifndef INCLUDE_MVLL_ALLOCATOR_HPP
#define INCLUDE_MVLL_ALLOCATOR_HPP

#include <concepts>
#include <cstddef>

namespace mvll
{
    template <class T>
    struct pseudo_pointer {
        constexpr pseudo_pointer() = default;
    };
} // ::mvll

#endif /*INCLUDE_MVLL_ALLOCATOR_HPP*/
