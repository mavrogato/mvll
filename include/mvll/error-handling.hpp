#ifndef INCLUDE_MVLL_ERROR_HANDLING_HPP
#define INCLUDE_MVLL_ERROR_HANDLING_HPP

#include <source_location>
#include <stdexcept>
#include <cassert>
#include <cstdlib>


#ifdef MVLL_ALLOW_EXCEPTIONS
# define MVLL_NOEXCEPT
#else
# define MVLL_NOEXCEPT noexcept
#endif

namespace mvll
{
    struct fatal_error : std::runtime_error {
    fatal_error(char const *const expr, std::source_location loc)
        : std::runtime_error{"fatal error"}
        , expression{expr}
        , location{loc}
        {}
        char const *const expression;
        std::source_location location;
    };

/// Note: you can overwrite this in your local scope
    inline void fatal_handler([[maybe_unused]] char const *const expr,
                              [[maybe_unused]] std::source_location loc)
        MVLL_NOEXCEPT
    {
#ifdef MVLL_ALLOW_EXCEPTIONS
        throw fatal_error(expr, loc);
#endif
    }
} // ::mvll

/// Note: side effect free, and customizable by overwriting fatal_handler
#define MVLL_CHECK(expr)                                                \
    ([](bool ret,                                                       \
        char const *const expr_str,                                     \
        std::source_location loc) MVLL_NOEXCEPT {                       \
        if (!ret) [[unlikely]] {                                        \
            mvll::fatal_handler(expr_str, loc);                         \
        }                                                               \
        return ret;                                                     \
    }(static_cast<bool>(expr),                                          \
      #expr,                                                            \
      std::source_location::current()) ? true : (assert(!#expr), false))

#endif /* INCLUDE_MVLL_ERROR_HANDLING_HPP */
