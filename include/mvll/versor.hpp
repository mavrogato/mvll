#ifndef INCLUDE_MVLL_VERSOR_HPP
#define INCLUDE_MVLL_VERSOR_HPP

#include <mvll/cpp2x/tuple-support.hpp>

#include <array>
#include <bit>
#include <functional>
#include <stdexcept>
#include <utility>

#include <cmath>
#include <cstddef>
#include <cstdint>


namespace mvll::inline algebra
{
    // Default ULP tolerance for floating point comparisons
    template <class T>
    struct default_ulp_tolerance {
        static constexpr std::size_t value = 4;
    };

    // Get the underlying integer representation of a floating point number
    template <std::floating_point T>
    constexpr auto backing_int(T s) noexcept {
        if constexpr (std::is_same_v<T, float>) {
            return std::bit_cast<std::int32_t>(s);
        }
        else if constexpr (std::is_same_v<T, double>) {
            return std::bit_cast<std::int64_t>(s);
        }
        else if constexpr (std::is_same_v<T, long double>) {
            if constexpr (sizeof(long double) == sizeof(uint64_t)) {
                return std::bit_cast<std::int64_t>(s);
            }
            static_assert(sizeof (T) == 0, "Unsupported long double size");
        }
    }

    // Compare two floating point numbers for almost equality within a given ULP tolerance
    template <std::floating_point T, std::size_t ULP_TORLERANCE = default_ulp_tolerance<T>::value>
    constexpr bool almost_equal(T x, T y) noexcept {
        if (std::isinf(x) || std::isinf(y)) {
            return x == y; // both must be the same infinity
        }
        if (std::isnan(x) || std::isnan(y)) {
            return false; // Nans are always unequal
        }
        if (std::signbit(x) != std::signbit(y)) {
            return x == y; // handle +0.0 and -0.0 as equal
        }

        // Get the integer representation of the floating point numbers
        auto xx = backing_int(x);
        auto yy = backing_int(y);
        using Int = decltype (xx);
        using Uint = std::make_unsigned_t<Int>;

        // Make lexicographical ordering of negative numbers work
        if (xx < 0) xx = std::rotr(Uint(1), 1) - xx;
        if (yy < 0) yy = std::rotr(Uint(1), 1) - yy;

        Uint delta = (xx > yy) ? (xx - yy) : (yy - xx); // We want the constant std::abs...

        return delta <= ULP_TORLERANCE;
    }

    template <class T, std::size_t N>
    struct versor : versor<T, N-1> {
    public:
        using base_type = versor<T, N-1>;
        using value_type = typename base_type::value_type;
        using iterator = typename base_type::iterator;
        using const_iterator = typename base_type::const_iterator;
        using reference = typename base_type::reference;
        using const_reference = typename base_type::const_reference;

        template <std::size_t NN> requires (NN <= N) using sub_type = versor<T, NN>;

    public:
        constexpr friend std::size_t size(versor) noexcept { return N; }
        constexpr std::size_t size() const noexcept { return N; }
        static constexpr std::size_t total_extent = N;

    public:
        value_type last;

    public:
        constexpr versor(versor const&) = default;
        constexpr versor(versor&&) = default;
        constexpr versor& operator=(versor const&) = default;
        constexpr versor& operator=(versor&&) = default;

    public:
        constexpr versor(auto... args) noexcept
            : versor{std::array<T, N>{static_cast<T>(args)...}, std::make_index_sequence<N-1>()}
        {
            static_assert(sizeof... (args) <= N);
        }

    private:
        constexpr static auto array_at(const std::array<T, N>& arr, std::size_t i) noexcept {
            return (i < arr.size()) ? arr[i] : T();
        }

        template <std::size_t... I>
        constexpr versor(std::array<T, N>&& args_array, std::index_sequence<I...>) noexcept
            : base_type{array_at(args_array, I)...}, last{array_at(args_array, N-1)}
        {
        }

    public:
        template <std::size_t I>
        constexpr auto get() const noexcept {
            static_assert(I < N);
            // We use 'I + 1 == N || N == 1' to handle two cases:
            // 1. We've reached the target index 'last' in the current derived class.
            // 2. We are in the base case N == 1, where 'last' is the only element,
            //    avoiding a call to get() on the empty versor<T, 0> base.
            if constexpr (I + 1 == N || N == 1)
                return this->last;
            else
                return base_type::template get<I>();
        }
        template <std::size_t I>
        constexpr auto& get() noexcept {
            static_assert(I < N);
            // See the comments in the const version of get<I>() for the rationale behind this condition.
            if constexpr (I + 1 == N || N == 1)
                return this->last;
            else
                return base_type::template get<I>();
        }
        template <std::size_t I>
        constexpr friend auto get(versor const& v) noexcept { return v.get<I>(); }
        template <std::size_t I>
        constexpr friend auto& get(versor& v) noexcept { return v.get<I>(); }

    public:
        constexpr auto begin() const noexcept { return &static_cast<versor<T, 1> const*>(this)->last; }
        constexpr auto begin() noexcept { return &static_cast<versor<T, 1>*>(this)->last; }
        constexpr auto end() const noexcept { return &this->last + 1; }
        constexpr auto end() noexcept { return &this->last + 1; }

        constexpr auto front() const noexcept { return *(this->begin()); }
        constexpr auto& front() noexcept { return *(this->begin()); }
        constexpr auto back() const noexcept { return this->last; }
        constexpr auto& back() noexcept { return this->last; }

        constexpr auto& operator[](std::size_t i) noexcept { return *(begin() + i); }
        constexpr auto operator[](std::size_t i) const noexcept { return *(begin() + i); }

        auto& at(std::size_t i) {
            if (this->size() <= i)
                throw std::range_error("versor index");
            return (*this)[i];
        }
        auto at(std::size_t i) const {
            if (this->size() <= i)
                throw std::range_error("versor index");
            return (*this)[i];
        }

    public:
        template <class Func, class... Rest>
        constexpr auto& apply(Func&& func, Rest&&... rest) noexcept {
            // Determine the minimum extent among all the versors
            constexpr auto NN = std::min({versor::total_extent, (std::decay_t<Rest>::total_extent)...});
            // Common type for the first NN elements
            using common_type = sub_type<NN>;
            // Apply the function to the all last elements
            common_type::last = func(common_type::last, static_cast<common_type const&>(rest).last...);

            if (NN > 1) {
                // Recurse into the common base type
                using recursive_type = common_type::base_type;
                if constexpr ((std::is_rvalue_reference_v<Rest> && ...)) {
                    recursive_type::apply(std::forward<Func>(func), std::move(static_cast<recursive_type&>(rest))...);
                }
                else {
                    recursive_type::apply(std::forward<Func>(func), static_cast<recursive_type const&>(rest)...);
                }
            }
            return *this;
        }

        constexpr auto& negate() noexcept { return apply(std::negate<T>()); }
        constexpr auto& lognot() noexcept { return apply(std::bit_not<T>()); }

    public:
        constexpr auto operator+() const noexcept { return *this; }
        constexpr auto operator-() const noexcept { return (+(*this)).negate(); }

        // Arithmetic complex assignments (simple vectorized operations)
        constexpr auto& operator+=(auto&& rhs) noexcept {
            return apply(std::plus<T>(), std::forward<decltype (rhs)>(rhs));
        }
        constexpr auto& operator-=(auto&& rhs) noexcept {
            return apply(std::minus<T>(), std::forward<decltype (rhs)>(rhs));
        }
        constexpr auto& operator*=(auto&& rhs) noexcept {
            return apply(std::multiplies<T>(), std::forward<decltype (rhs)>(rhs));
        }
        constexpr auto& operator/=(auto&& rhs) noexcept {
            return apply(std::divides<T>(), std::forward<decltype (rhs)>(rhs));
        }

        // Binary arithmetics (returning new instance)
        constexpr auto operator+(auto&& rhs) const noexcept {
            return (+(*this)) += std::forward<decltype (rhs)>(rhs);
        }
        constexpr auto operator-(auto&& rhs) const noexcept {
            return (+(*this)) -= std::forward<decltype (rhs)>(rhs); }
        constexpr auto operator*(auto&& rhs) const noexcept {
            return (+(*this)) *= std::forward<decltype (rhs)>(rhs);
        }
        constexpr auto operator/(auto&& rhs) const noexcept {
            return (+(*this)) /= std::forward<decltype (rhs)>(rhs);
        }

        // Scalar multiplication/division
        constexpr auto& operator*=(value_type s) noexcept {
            return apply([s](value_type x) noexcept {
                return x * s;
            });
        }
        constexpr auto& operator/=(value_type s) noexcept { return (*this) *= (1/s); }
        constexpr auto operator*(value_type s) const noexcept { return (+(*this)) *= s; }
        constexpr auto operator/(value_type s) noexcept { return (+(*this)) /= s; }
        constexpr friend auto operator*(value_type s, versor v) noexcept { return v * s; }

        // Bitwise unary operation
        constexpr auto operator~() const noexcept { return (+(*this)).lognot(); }

        // Bitwise complex assignments (simple vectorized operations)
        constexpr auto& operator^=(auto&& rhs) noexcept {
            return apply(std::bit_xor<T>(), std::forward<decltype (rhs)>(rhs));
        }
        constexpr auto& operator|=(auto&& rhs) noexcept {
            return apply(std::bit_or<T>(), std::forward<decltype (rhs)>(rhs));
        }
        constexpr auto& operator&=(auto&& rhs) noexcept {
            return apply(std::bit_and<T>(), std::forward<decltype (rhs)>(rhs));
        }

        // Bitwise binary operations (returning new instance)
        constexpr auto operator^(auto&& rhs) const noexcept {
            return (+(*this)) ^= std::forward<decltype (rhs)>(rhs);
        }
        constexpr auto operator|(auto&& rhs) const noexcept {
            return (+(*this)) |= std::forward<decltype (rhs)>(rhs);
        }
        constexpr auto operator&(auto&& rhs) const noexcept {
            return (+(*this)) &= std::forward<decltype (rhs)>(rhs);
        }

    public:
        constexpr bool operator==(versor const& rhs) const noexcept {
            if constexpr (std::floating_point<T>) {
                return almost_equal(this->last, rhs.last) &&
                    base_type::operator==(static_cast<base_type const&>(rhs));
            }
            else {
                return this->last == rhs.last &&
                    base_type::operator==(static_cast<base_type const&>(rhs));
            }
        }
    };

    template <class T>
    struct versor<T, 0> {
    public:
        using value_type = T;
        using iterator = value_type*;
        using const_iterator = value_type const*;
        using reference = value_type&;
        using const_reference = value_type const&;
        using size_type = std::size_t;

    public:
        constexpr friend std::size_t size(versor) noexcept { return 0; }
        constexpr std::size_t size() const noexcept { return 0; }

        constexpr bool operator==(versor&&) const noexcept { return true; }
        constexpr bool operator==(versor const&) const noexcept { return true; }

    protected:
        constexpr auto& apply(auto&&...) noexcept { return *this; }
        constexpr friend T inner(versor, versor) noexcept { return T(); }
    };

    template <class T, std::size_t N>
    constexpr auto inner(versor<T, N> lhs, versor<T, N> rhs) noexcept {
        return lhs.back() * rhs.back() + inner(static_cast<versor<T, N-1> const&>(lhs),
                                               static_cast<versor<T, N-1> const&>(rhs));
    }

    template <class T>
    constexpr auto cross(versor<T, 3> lhs, versor<T, 3> rhs) noexcept {
        return versor<T, 3>{
            lhs[1] * rhs[2] - lhs[2] * rhs[1],
            lhs[2] * rhs[0] - lhs[0] * rhs[2],
            lhs[0] * rhs[1] - lhs[1] * rhs[0],
        };
    }

    using vec2s = versor<short,  2>;
    using vec2i = versor<int,    2>;
    using vec2f = versor<float,  2>;
    using vec2d = versor<double, 2>;
    using vec3s = versor<short,  3>;
    using vec3i = versor<int,    3>;
    using vec3f = versor<float,  3>;
    using vec3d = versor<double, 3>;
    using vec4s = versor<short,  4>;
    using vec4i = versor<int,    4>;
    using vec4f = versor<float,  4>;
    using vec4d = versor<double, 4>;

    using color = versor<uint8_t, 4>;
} // ::mvll::algebra

// tuple support
namespace std
{
    template <class T, size_t N>
    struct tuple_size<mvll::versor<T, N>> {
        static constexpr auto value = N;
    };
    template <class T, size_t N>
    constexpr size_t tuple_size_v<mvll::versor<T, N>> = tuple_size<mvll::versor<T, N>>::value;

    template <size_t I, class T, size_t N>
    struct tuple_element<I, mvll::versor<T, N>> {
        using type = T;
    };
} // ::std

#endif // INCLUDE_MVLL_VERSOR_HPP
