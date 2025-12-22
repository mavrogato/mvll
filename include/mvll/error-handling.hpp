#ifndef INCLUDE_MVLL_ERROR_HANDLING_HPP
#define INCLUDE_MVLL_ERROR_HANDLING_HPP

#include <source_location>
#include <cassert>


#ifdef MVLL_ALLOW_EXCEPTIONS
# define MVLL_NOEXCEPT
#else
# define MVLL_NOEXCEPT noexcept
#endif

/// Note: you can overwrite this in your local scope
constexpr bool fatal_handler([[maybe_unused]] char const *const expr,
                             [[maybe_unused]] std::source_location loc = std::source_location::current())
    MVLL_NOEXCEPT
{
    return true;
}

/// Note: side effect free, and customizable by overwriting fatal_handler
#define MVLL_CHECK(expr)                                      \
    (static_cast<bool>(expr) ? true :                         \
     (fatal_handler(#expr) ? \
      assert(!#expr), false : false))

#endif /* INCLUDE_MVLL_ERROR_HANDLING_HPP */
