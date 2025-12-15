/*
 * Copyright (C) 2025 Nakshima, Terumi (nakashima.terumi@gmail.com)
 *
 * This file is a derivative work based on the std::generator implementation
 * from the GNU C++ Library (libstdc++-v3) of the GCC project.
 * The original implementation is primarily authored by Jonathan Wakely
 * and other GCC contributors.
 *
 * Original Source: gcc.gnu.org
 * Original License: GNU Lesser General Public License (LGPL) as noted below.
 *
 * My modifications primarily involve adjustments to coding style, naming conventions,
 * and removal of GCC-specific built-in functions to improve portability.
 * The core logic and technical implementation remain faithful to the original design.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <www.gnu.org>.
 */
#ifndef INCLUDE_MVLL_CPP2X_GENERATOR_HPP
#define INCLUDE_MVLL_CPP2X_GENERATOR_HPP

#include <concepts>
#include <coroutine>
#include <exception>
#include <iterator>
#include <memory>
#include <memory_resource>
#include <new>
#include <ranges>
#include <type_traits>
#include <utility>
#include <variant>

#include <cassert>
#include <cstddef>


namespace mvll::inline cpp2x
{
    template <class R, class Alloc = std::allocator<std::byte>>
    class elements_of_adaptor {
    public:
        [[no_unique_address]] R     range;
        [[no_unique_address]] Alloc allocator = Alloc();
    };
    template <class R, class Alloc = std::allocator<std::byte>>
    elements_of_adaptor(R&&, Alloc = Alloc()) -> elements_of_adaptor<R&&, Alloc>;

    template <class Ref, class Val = void, class Alloc = void> class generator;

    namespace gen
    {
        /// Referene type for a generator whose reference (first argument) and
        /// values (second argument) types are Ref and Val.
        template <class Ref, class Val>
        using reference_t = std::conditional_t<std::is_reference_v<Ref>, Ref&&, Ref>;
        /// Type yielded by a generator whose reference type is Ref.
        template <class Ref>
        using yield_t = std::conditional_t<std::is_reference_v<Ref>,
                                           Ref,
                                           Ref const&>;
        /// yield_t * reference_t
        template <class Ref, class Val>
        using yield2_t = yield_t<reference_t<Ref, Val>>;
        /// type predicator for the generator
        template <class> constexpr bool is_generator = false;
        template<class Val, class Ref, class Alloc>
        constexpr bool is_generator<aux::generator<Val, Ref, Alloc>> = true;
        /// Allocator and value type erased generator promise type.
        /// \tparam Yielded: The corresponding generators yielded type.
        template <class Yielded>
        class promise_erased {
            static_assert(std::is_reference_v<Yielded>);
            using yielded_deref = std::remove_reference_t<Yielded>;
            using yielded_decvref = std::remove_cvref_t<Yielded>;
            using value_ptr = std::add_pointer_t<Yielded>;
            using coro_handle = std::coroutine_handle<promise_erased>;
            template <class, class, class> friend class ::aux::generator;
            template <class Gen> struct recursive_awaiter;
            template <class> friend struct recursive_awaiter;
            struct copy_awaiter;
            struct subyield_state;
            struct final_awaiter;

        public:
            std::suspend_always initial_suspend() const noexcept { return {}; }
            std::suspend_always yield_value(Yielded val) noexcept {
                bottom_value_() = std::addressof(val);
                return {};
            }
            auto yield_value(yielded_deref const& val)
                noexcept (std::is_nothrow_constructible_v<yielded_decvref, yielded_deref const&>)
                requires (std::is_rvalue_reference_v<Yielded> && std::constructible_from<yielded_decvref,
                          yielded_deref const&>) {
                return copy_awaiter(yielded_decvref(val), bottom_value_());
            }
            template <class R2, class V2, class A2, class U2>
                requires std::same_as<yield2_t<R2, V2>, Yielded>
            auto yield_value(elements_of_adaptor<generator<R2, V2, A2>&&, U2> r) noexcept {
                return recursive_awaiter{std::move(r.range)};
            }
            template <class R2, class V2, class A2, class U2>
                requires std::same_as<yield2_t<R2, V2>, Yielded>
            auto yield_value(elements_of_adaptor<generator<R2, V2, A2>&, U2> r) noexcept {
                return recursive_awaiter{std::move(r.range)};
            }
            template <std::ranges::input_range R, class Alloc>
            requires std::convertible_to<std::ranges::range_reference_t<R>, Yielded>
            auto yield_value(elements_of_adaptor<R, Alloc> r) {
                auto n = [](std::allocator_arg_t,
                            Alloc,
                            std::ranges::iterator_t<R> i,
                            std::ranges::sentinel_t<R> s)
                    -> generator<Yielded, std::ranges::range_value_t<R>, Alloc> {
                    for (; i != s; ++i) co_yield static_cast<Yielded>(*i);
                };
                return yield_value(elements_of_adaptor(n(std::allocator_arg,
                                                         r.allocator,
                                                         std::ranges::begin(r.range),
                                                         std::ranges::end(r.range))));
            }
            final_awaiter final_suspend() noexcept { return {}; }
            void unhandled_exception() {
                // To get to this point, this coroutine must have been active.  In that
                // case, it must be the top of the stack.  The current coroutine is
                // the sole entry of the stack iff it is both the top and the bottom.  As
                // it is the top implicitly in this context it will be the sole entry iff
                // it is the bottom.
                if (nest_.is_bottom_())
                    throw;
                else
                    except_ = std::current_exception();
            }
            void await_transform() = delete;
            void return_void() const noexcept {}

        private:
            value_ptr& bottom_value_() noexcept { return nest_.bottom_value_(*this); }
            value_ptr& value_() noexcept { return nest_.value_(*this); }

        private:
            subyield_state nest_;
            std::exception_ptr except_;
        };
        template <class Yielded>
        struct promise_erased<Yielded>::subyield_state {
            struct frame {
                coro_handle bottom_;
                coro_handle parent_;
            };
            struct bottom_frame {
                coro_handle top_;
                value_ptr value_ = nullptr;
            };
            std::variant<bottom_frame, frame> stack_;
            bool is_bottom_() const noexcept {
                return !std::holds_alternative<frame>(this->stack_);
            }
            coro_handle& top_() noexcept {
                if (auto f = std::get_if<frame>(&this->stack_))
                    return f->bottom_.promise().nest_.top_();
                auto bf = std::get_if<bottom_frame>(&this->stack_);
                assert(bf);
                return bf->top_;
            }
            void push_(coro_handle current, coro_handle subyield) noexcept {
                assert(&current.promise().nest_ == this);
                assert(this->top_() == current);
                subyield.promise().nest_.jump_in_(current, subyield);
            }
            std::coroutine_handle<> pop_() noexcept {
                if (auto f = std::get_if<frame>(&this->stack_)) {
                    // We aren't a bottom coroutine. Restore the parent to the top and resume.
                    auto p = this->top_() = f->parent_;
                    return p;
                }
                else
                    // Otherwise, there's nothing to resume.
                    return std::noop_coroutine();
            }
            void jump_in_(coro_handle rest, coro_handle target) noexcept {
                // We're bottom. We're also top if top is unset (note that this is
                // not true if something was added to the coro stack and then popped,
                // but in that case we can't possibly be yielded from, as it would
                // require rerurnning begin()).
                assert(!this->top_());
                auto& rn = rest.promise().nest_;
                rn.top_() = target;
                // Presume we'are the second frame...
                auto bott = rest;
                if (auto f = std::get_if<frame>(&rn.stack_))
                    // But, if we aren't, get the action bottom. We are only the second
                    // frame if our parent is the bottom frame, i.e. it doesn't hav a
                    // frame member.
                    bott = f->bottom_;
                this->stack_ = frame {
                    .bottom_ = bott,
                    .parent_ = rest,
                };
            }
            value_ptr& bottom_value_(promise_erased& current) noexcept {
                assert(&current.nest_ == this);
                if (auto bf = std::get_if<bottom_frame>(&this->stack_))
                    return bf->value_;
                auto f = std::get_if<frame>(&this->stack_);
                assert(f);
                auto& p = f->bottom_.promise();
                return p.nest_.value_(p);
            }
            value_ptr& value_(promise_erased& current) noexcept {
                assert(&current.nest_ == this);
                auto bf = std::get_if<bottom_frame>(&this->stack_);
                assert(bf);
                return bf->value_;
            }
        };

        template <class Yielded>
        struct promise_erased<Yielded>::final_awaiter {
            bool await_ready() noexcept { return false; }
            template <class Promise>
            auto await_suspend(std::coroutine_handle<Promise> c) noexcept {
                //static_assert(std::is_pointer_interconvertible_base_of_v<promise_erased, Promise>);
                static_assert(std::is_standard_layout_v<promise_erased> &&
                              std::is_standard_layout_v<Promise>,
                              "Both types must be standard layout types for safe interconversion in C++20.");
                auto& n = c.promise().nest_;
                return n.pop_();
            }
            void await_resume() noexcept {}
        };

        template <class Yielded>
        struct promise_erased<Yielded>::copy_awaiter {
            yielded_decvref value_;
            value_ptr& bottom_value_;
            constexpr bool await_ready() noexcept { return false; }
            template <class Promise>
            void await_suspend(std::coroutine_handle<Promise>) noexcept {
                //static_assert(std::is_pointer_interconvertible_base_of_v<promise_erased, Promise>);
                static_assert(std::is_standard_layout_v<promise_erased> &&
                              std::is_standard_layout_v<Promise>,
                              "Both types must be standard layout types for safe interconversion in C++20.");
                bottom_value_ = std::addressof(value_);
            }
            constexpr void await_resume() const noexcept {}
        };

        template <class Yielded>
        template <class Gen>
        struct promise_erased<Yielded>::recursive_awaiter {
            Gen gen_;
            static_assert(is_generator<Gen>);
            static_assert(std::same_as<typename Gen::yielded, Yielded>);
            recursive_awaiter(Gen gen) noexcept : gen_{std::move(gen)} {
                this->gen_.mark_as_started_();
            }
            constexpr bool await_ready() const noexcept { return false; }
            template <class Promise>
            std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> p) noexcept {
                //static_assert(std::is_pointer_interconvertible_base_of_v<promise_erased, Promise>);
                static_assert(std::is_standard_layout_v<promise_erased> &&
                              std::is_standard_layout_v<Promise>,
                              "Both types must be standard layout types for safe interconversion in C++20.");
                auto c = coro_handle::from_address(p.address());
                auto t = coro_handle::from_address(this->gen_.coro_.address());
                p.promise().nest_.push_(c, t);
                return t;
            }
            void await_resume() {
                if (auto e = gen_.coro_.promise().except_)
                    std::rethrow_exception(e);
            }
        };

        struct alloc_block {
            constexpr static auto DEFAULT_NEW_ALIGNMENT = alignof (std::max_align_t);
            alignas (DEFAULT_NEW_ALIGNMENT) char data_[DEFAULT_NEW_ALIGNMENT];
            static auto cnt_(std::size_t sz) noexcept {
                auto blksz = sizeof (alloc_block);
                return (sz + blksz - 1) / blksz;
            }
        };

        template <class All>
        concept stateless_alloc = (std::allocator_traits<All>::is_always_equal::value
                                   && std::default_initializable<All>);
        template <class Allocator>
        class promise_alloc {
            using rebound = std::allocator_traits<Allocator>::template rebind_alloc<alloc_block>;
            using rebound_atr = std::allocator_traits<rebound>;
            static_assert(std::is_pointer_v<typename rebound_atr::pointer>,
                          "Must use allocators for true pointers with generators");
            static auto alloc_address_(std::uintptr_t fn, std::uintptr_t fsz) noexcept {
                auto an = fn + fsz;
                auto ba = alignof (rebound);
                return reinterpret_cast<rebound*>(((an + ba - 1) / ba) * ba);
            }
            static auto alloc_size_(std::size_t csz) noexcept {
                auto ba = alignof (rebound);
                // Our desired layout is placing the coroutine frame, then pad out to
                // align, then place the allocator. The total size of that is the
                // size of the coroutine frame, plus up to ba bytes, plus the size
                // of the allocator.
                return csz + ba + sizeof (rebound);
            }
            static void* allocate_(rebound b, std::size_t csz) {
                if constexpr (stateless_alloc<rebound>)
                    // Only need room for the coroutine.
                    return b.allocate(alloc_block::cnt_(csz));
                else {
                    auto nsz = alloc_block::cnt_(alloc_size_(csz));
                    auto f = b.allocate(nsz);
                    auto fn = reinterpret_cast<std::uintptr_t>(f);
                    auto an = alloc_address_(fn, csz);
                    ::new (an) rebound{std::move(b)};
                    return f;
                }
            }

        public:
            void* operator new(std::size_t sz) requires std::default_initializable<rebound> {
                return allocate_({}, sz);
            }
            template <class Alloc, class... Args>
            void* operator new(std::size_t sz,
                               std::allocator_arg_t,
                               Alloc const& a,
                               Args const&...) {
                static_assert(std::convertible_to<Alloc const&, Allocator>,
                              "the allocator argument to the coroutine must be "
                              "convertible to the generator's allocator type");
                return allocate_(rebound(Allocator(a)), sz);
            }
            template <class This, class Alloc, class... Args>
            void* operator new(std::size_t sz,
                               This const&,
                               std::allocator_arg_t,
                               Alloc const& a,
                               Args const&...) {
                static_assert(std::convertible_to<Alloc const&, Allocator>,
                              "the allocator argument to the coroutine must be "
                              "convertible to the generator's allocator type");
                return allocate_(rebound(Allocator(a)), sz);
            }
            void operator delete(void* ptr, std::size_t csz) noexcept {
                if constexpr (stateless_alloc<rebound>) {
                    rebound b;
                    return b.deallocate(reinterpret_cast<alloc_block*>(ptr),
                                        alloc_block::cnt_(csz));
                }
                else {
                    auto nsz = alloc_block::cnt_(alloc_size_(csz));
                    auto fn = reinterpret_cast<std::uintptr_t>(ptr);
                    auto an = alloc_address_(fn, csz);
                    rebound b{std::move(*an)};
                    an->~rebound();
                    b.deallocate(reinterpret_cast<alloc_block*>(ptr), nsz);
                }
            }
        };
        template <>
        class promise_alloc<void> {
            using dealloc_fn = void (*)(void*, std::size_t);
            static auto dealloc_address_(std::uintptr_t fn, std::uintptr_t fsz) noexcept {
                auto an = fn + fsz;
                auto ba = alignof (dealloc_fn);
                auto aligned = ((an + ba - 1) / ba) * ba;
                return reinterpret_cast<dealloc_fn*>(aligned);
            }
            template <class Rebound>
            static auto alloc_address(std::uintptr_t fn, std::uintptr_t fsz) noexcept
                requires (!stateless_alloc<Rebound>)
            {
                auto ba = alignof (Rebound);
                auto da = dealloc_address_(fn, fsz);
                auto aan = reinterpret_cast<std::uintptr_t>(da);
                aan += sizeof (dealloc_fn);
                auto aligned = ((aan + ba - 1) / ba) * ba;
                return reinterpret_cast<Rebound*>(aligned);
            }
            template <class Rebound>
            static auto alloc_size_(std::size_t csz) noexcept {
                // This time, we want the coroutine frame, then the deallocator
                // pointer, then the allocator itself, if any.
                std::size_t aa = 0;
                std::size_t as = 0;
                if constexpr (!std::same_as<Rebound, void>) {
                    aa = alignof (Rebound);
                    as = sizeof (Rebound);
                }
                auto ba = aa + alignof (dealloc_fn);
                return csz + ba + as + sizeof (dealloc_fn);
            }
            template <class Rebound>
            static void deallocator_(void* ptr, std::size_t csz) noexcept {
                auto asz = alloc_size_<Rebound>(csz);
                auto nblk = alloc_block::cnt_(asz);
                if constexpr (stateless_alloc<Rebound>) {
                    Rebound b;
                    b.deallocate(reinterpret_cast<alloc_block*>(ptr), nblk);
                }
                else {
                    auto fn = reinterpret_cast<std::uintptr_t>(ptr);
                    auto an = alloc_address_<Rebound>(fn, csz);
                    Rebound b{std::move(*an)};
                    an->~Rebound();
                    b.deallocate(reinterpret_cast<alloc_block*>(ptr), nblk);
                }
            }
            template <class Alloc>
            static void* allocate_(Alloc const& a, std::size_t csz) {
                using rebound = std::allocator_traits<Alloc>::template rebind_alloc<alloc_block>;
                using rebound_atr = std::allocator_traits<rebound>;
                static_assert(std::is_pointer_v<typename rebound_atr::pointer>,
                              "Must use allocators for true pointers with generators");
                dealloc_fn d = &deallocator_<rebound>;
                auto b = static_cast<rebound>(a);
                auto asz = alloc_size_<rebound>(csz);
                auto nblk = alloc_block::cnt_(asz);
                void* p = b.allocate(nblk);
                auto pn = reinterpret_cast<std::uintptr_t>(p);
                *dealloc_address_(pn, csz) = d;
                if constexpr (!stateless_alloc<rebound>) {
                    auto an = alloc_address_<rebound>(pn, csz);
                    ::new (an) rebound{std::move(b)};
                }
                return p;
            }

        public:
            void* operator new(std::size_t sz) {
                auto nsz = alloc_size_<void>(sz);
                dealloc_fn d = [](void* ptr, std::size_t sz) {
                    // TBD.
                    //::operator delete(__ptr, _M_alloc_size<void>(__sz));
                    ::operator delete(ptr);
                };
                auto p = ::operator new(nsz);
                auto pn = reinterpret_cast<std::uintptr_t>(p);
                *dealloc_address_(pn, sz) = d;
                return p;
            }
            template <class Alloc, class... Args>
            void* operator new(std::size_t sz,
                               std::allocator_arg_t,
                               Alloc const& a,
                               Args const&...) {
                return allocate_(a, sz);
            }
            void operator delete(void* ptr, std::size_t sz) noexcept {
                dealloc_fn d;
                auto pn = reinterpret_cast<std::uintptr_t>(ptr);
                d = *dealloc_address_(pn, sz);
                d(ptr, sz);
            }
        };

        template <class T>
        concept cv_unqualified_object = std::is_object_v<T> && std::same_as<T, std::remove_cv_t<T>>;
    } // ::gen

    template <class Ref, class Val, class Alloc>
    class generator : public std::ranges::view_interface<generator<Ref, Val, Alloc>>
    {
        using value = std::conditional_t<std::is_void_v<Val>,
                                         std::remove_cvref_t<Ref>,
                                         Val>;
        static_assert(gen::cv_unqualified_object<value>,
                      "Generator value must be a cv-unqualified object type");
        using reference = gen::reference_t<Ref, Val>;
        static_assert(std::is_reference_v<reference>
                      || (gen::cv_unqualified_object<reference> &&
                          std::copy_constructible<reference>),
                      "Generator reference type must be either a cv-unqualified "
                      "object type that is trivially constructible or a "
                      "reference type");
        using rref = std::conditional_t<
            std::is_reference_v<reference>,
            std::remove_reference_t<reference>&&,
            reference>;
        // Required to model indirectly_readable, and input_iterator.
        static_assert(std::common_reference_with<reference&&, value&&>);
        static_assert(std::common_reference_with<reference&&, rref&&>);
        static_assert(std::common_reference_with<rref&&, value const&>);
        using yielded = gen::yield_t<reference>;
        using erased_promise = gen::promise_erased<yielded>;
        struct iterator;
        friend erased_promise;
        friend struct erased_promise::subyield_state;

    public:
        struct promise_type : erased_promise, gen::promise_alloc<Alloc>
        {
            generator get_return_object() noexcept {
                return {std::coroutine_handle<promise_type>::from_promise(*this)};
            }
        };
        //static_assert(std::is_pointer_interconvertible_base_of_v<promise_erased, Promise>);
        static_assert(std::is_standard_layout_v<erased_promise> &&
                      std::is_standard_layout_v<promise_type>,
                      "Both types must be standard layout types for safe interconversion in C++20.");
        generator(generator const&) = delete;
        generator(generator&& other) noexcept
            : coro_{std::exchange(other.coro_, nullptr)}
            , began_{std::exchange(other.began_, false)}
        {}
        ~generator() {
            if (auto& c = this->coro_)
                c.destroy();
        }
        generator& operator=(generator other) noexcept {
            std::swap(other.coro_, this->coro_);
            std::swap(other.began_, this->began_);
            return *this;
        }
        iterator begin() {
            this->mark_as_started_();
            auto h = coro_handle::from_promise(coro_.promise());
            h.promise().nest_.top_() = h;
            return {h};
        }
        std::default_sentinel_t end() const noexcept { return std::default_sentinel; }

    private:
        using coro_handle = std::coroutine_handle<erased_promise>;
        generator(std::coroutine_handle<promise_type> coro) noexcept
            : coro_{std::move(coro)}
        {}
        void mark_as_started_() noexcept {
            assert(!this->began_);
            this->began_ = true;
        }
        std::coroutine_handle<promise_type> coro_;
        bool began_ = false;
    };
    template <class Ref, class Val, class Alloc>
    struct generator<Ref, Val, Alloc>::iterator {
        using value_type = value;
        using difference_type = std::ptrdiff_t;
        friend bool operator==(iterator const& i, std::default_sentinel_t) noexcept {
            return i.coro_.done();
        }
        iterator& operator=(iterator&& o) noexcept {
            this->coro_ = std::exchange(o.coro_, {});
            return *this;
        }
        iterator& operator++() {
            next_();
            return *this;
        }
        void operator++(int) {
            this->operator++();
        }
        reference operator*() const noexcept(std::is_nothrow_move_constructible_v<reference>) {
            auto& p = this->coro_.promise();
            return static_cast<reference>(*p.value_());
        }

    private:
        friend class generator;
        iterator(coro_handle g) : coro_{g} {
            this->next_();
        }
        void next_() {
            auto& t = this->coro_.promise().nest_.top_();
            t.resume();
        }
        coro_handle coro_;
    };

    namespace pmr
    {
        template <class Ref, class Val = void>
        using generator = generator<Ref, Val, std::pmr::polymorphic_allocator<std::byte>>;
    }

} // ::mvll::cpp2x

#endif // INCLUDE_MVLL_CPP2X_GENERATOR_HPP
