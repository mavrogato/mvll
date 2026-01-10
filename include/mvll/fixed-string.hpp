#ifndef INCLUDE_MVLL_FIXED_STRING_HPP
#define INCLUDE_MVLL_FIXED_STRING_HPP

#include <cstddef>
#include <algorithm>
#include <string_view>
#include <iosfwd>
#include <functional>
#include <stdexcept>
#include <compare>


namespace mvll::internals
{
    template <typename Base> using value_type_t      = typename Base::value_type;
    template <typename Base> using size_type_t       = typename Base::size_type;
    template <typename Base> using view_type_t       = typename Base::view_type;
    template <typename Base> using iterator_t        = typename Base::iterator;
    template <typename Base> using const_iterator_t  = typename Base::const_iterator;
    template <typename Base> using reference_t       = typename Base::reference;
    template <typename Base> using const_reference_t = typename Base::const_reference;
    template <typename Base>
    class alias_ingector {
    public:
        using value_type      = value_type_t<Base>;
        using size_type       = size_type_t<Base>;
        using view_type       = view_type_t<Base>;
        using iterator        = iterator_t<Base>;
        using const_iterator  = const_iterator_t<Base>;
        using reference       = reference_t<Base>;
        using const_reference = const_reference_t<Base>;
    };
} // ::mvll::internals

namespace mvll
{
    template <class Ch, std::size_t N>
    class basic_fixed_string {
    public:
        using value_type = Ch;
        using size_type = std::size_t;
        using view_type = std::basic_string_view<Ch>;
        using iterator = value_type*;
        using const_iterator = value_type const*;
        using reference = value_type&;
        using const_reference = value_type const&;

    public:
        constexpr basic_fixed_string(basic_fixed_string const&) noexcept = default;
        constexpr basic_fixed_string(basic_fixed_string&&) noexcept = default;
        constexpr basic_fixed_string& operator=(basic_fixed_string const&) noexcept = default;
        constexpr basic_fixed_string& operator=(basic_fixed_string&&) noexcept = default;

        constexpr basic_fixed_string(value_type const *src) noexcept {
            std::copy_n(src, size(), data_);
        }

    public:
        constexpr auto size()  const noexcept { return N - 1; }
        constexpr auto empty() const noexcept { return size() == 0; }
        constexpr auto data()  const noexcept { return this->data_; }
        constexpr auto data()        noexcept { return this->data_; }

    public:
        constexpr auto view() const noexcept {
            return view_type{this->data(), this->size()};
        }
        template <class RHS>
        constexpr auto operator<=>(RHS const& rhs) const noexcept {
            return this->view() <=> rhs;
        }
        constexpr auto cbegin() const noexcept { return this->data_; }
        constexpr auto  begin() const noexcept { return this->data_; }
        constexpr auto  begin()       noexcept { return this->data_; }
        constexpr auto cend  () const noexcept { return this->begin() + this->size(); }
        constexpr auto  end  () const noexcept { return this->begin() + this->size(); }
        constexpr auto  end  ()       noexcept { return this->begin() + this->size(); }

        constexpr auto  operator[](std::size_t i) const noexcept { return *(this->begin() + i); }
        constexpr auto& operator[](std::size_t i)       noexcept { return *(this->begin() + i); }
        constexpr auto  at(std::size_t i) const {
            if (i < this->size()) {
                return this->operator[](i);
            }
            throw std::out_of_range("basic_fixed_string: index out of bounds");
        }
        constexpr auto& at(std::size_t i)       {
            if (i < this->size()) {
                return this->operator[](i);
            }
            throw std::out_of_range("basic_fixed_string: index out of bounds");
        }

    public:
        template <class Xr, class Tr>
        friend auto& operator<<(std::basic_ostream<Xr, Tr>& output, basic_fixed_string const& str) {
            return output << str.view();
        }

    public:
        // Note: This member must be public so that the class can be used 
        // as a non-type template parameter (C++20 NTTP requirement).
        value_type data_[N]{};
    };
    template<class Ch, std::size_t N> basic_fixed_string(Ch const (&)[N]) -> basic_fixed_string<Ch, N>;

    // The template argument deduction defined above cannot fully resolve the 'Ch' 
    // type when using character literals. The derived class below is used as 
    // a workaround to ensure correct type resolution.

    template <std::size_t N>
    class fixed_string
        : public basic_fixed_string<char, N>
        , public internals::alias_ingector<basic_fixed_string<char, N>>
    {
    public:
        using base_type = basic_fixed_string<char, N>;

    public:
        using base_type::base_type; // constructor delegation
    };
    template <std::size_t N> fixed_string(char const (&)[N]) -> fixed_string<N>;

} // ::mvll

namespace std
{
    // std::hash
    template <class Ch, std::size_t N>
    struct hash<mvll::basic_fixed_string<Ch, N>> {
        constexpr std::size_t operator()(mvll::basic_fixed_string<Ch, N> const& s) const noexcept {
            return std::hash<std::basic_string_view<Ch>>{}(s.view());
        }
    };

    // std::tuple requirements
    template <class Ch, std::size_t N>
    struct tuple_size<mvll::basic_fixed_string<Ch,  N>> {
        static constexpr auto value = N - 1;
    };
    template <class Ch, std::size_t N>
    constexpr auto tuple_size_v<mvll::basic_fixed_string<Ch, N>> =
        tuple_size<mvll::basic_fixed_string<Ch, N>>::value;
    template <std::size_t I, class Ch, std::size_t N>
    struct tuple_element<I, mvll::basic_fixed_string<Ch, N>> {
        using type = Ch;
    };

//     // std::formatter
// #if defined(__cpp_lib_format) && __cpp_lib_format >= 202207L
//     template <class Ch, size_t N>
//     struct formatter<mvll::basic_fixed_string<Ch, N>, Ch>
//     {
//         // ViewFormatterをメンバーとして持つ
//         using ViewFormatter = std::formatter<std::basic_string_view<Ch>, Ch>;
//         ViewFormatter view_fmt;
//         // semiregular要件を満たすために、特殊メンバ関数を明示的にデフォルト化する
//         constexpr formatter() = default;
//         constexpr formatter(const formatter&) = default;
//         constexpr formatter(formatter&&) = default;
//         constexpr formatter& operator=(const formatter&) = default;
//         constexpr formatter& operator=(formatter&&) = default;
//         ~formatter() = default;
//         // parse メンバ関数を ViewFormatter に委譲する
//         constexpr auto parse(auto& ctx) {
//             return view_fmt.parse(ctx);
//         }
//         // format メンバ関数を ViewFormatter に委譲する
//         auto format(const mvll::basic_fixed_string<Ch, N>& s, auto& ctx) const {
//             return view_fmt.format(s.view(), ctx);
//         }
//     };
// #endif // C++23 feature check
} // ::std

#endif // INCLUDE_MVLL_FIXED_STRING_HPP
