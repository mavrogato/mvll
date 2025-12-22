#ifndef INCLUDE_MVLL_CPP2X_FUNCTIONAL_HPP
#define INCLUDE_MVLL_CPP2X_FUNCTIONAL_HPP

#include <memory>

namespace mvll::inline cpp2x
{
    template<typename Sig> class move_only_function;

    template<typename R, typename... Args>
    class move_only_function<R(Args...)> {
        struct base {
            virtual constexpr R call(Args...) const = 0;
            virtual constexpr ~base() = default;
        };

        template<typename F>
        struct impl : base {
            F f;
            constexpr impl(F f) : f(std::move(f)) {}
            constexpr R call(Args... args) const override { return f(args...); }
        };

        std::unique_ptr<base> ptr;

    public:
        template<typename F>
        constexpr move_only_function(F f) 
            : ptr(std::make_unique<impl<F>>(std::move(f))) {}

        constexpr R operator()(Args... args) const {
            return ptr->call(args...);
        }
    };
} // ::mvll::cpp2x

#endif /* INCLUDE_MVLL_CPP2X_FUNCTIONAL_HPP */
