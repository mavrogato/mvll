
#include "mvll/error-handling.hpp"
#include <array>
#include <bit>
#include <coroutine>
#include <exception>
#include <filesystem>
#include <forward_list>
#include <functional>
#include <iostream>
#include <memory>
#include <tuple>
#include <type_traits>
#include <variant>

#include <sycl/sycl.hpp>

#include <mvll/cpp2x/generator.hpp>
#include <mvll/cpp2x/tuple-support.hpp>
#include <mvll/platform/linux.hpp>
#include <mvll/unique.hpp>

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

#include <xdg-shell-client.h>
#include <zwp-tablet-v2-client.h>

namespace mvll::inline algebra
{
    // Default ULP tolerance for floating point comparisons
    template <class T>
    struct default_ulp_tolerance {
        static constexpr size_t value = 4;
    };

    // Get the underlying integer representation of a floating point number
    template <std::floating_point T>
    constexpr auto backing_int(T s) noexcept {
        if constexpr (std::is_same_v<T, float>) {
            return std::bit_cast<int32_t>(s);
        }
        else if constexpr (std::is_same_v<T, double>) {
            return std::bit_cast<int64_t>(s);
        }
        else if constexpr (std::is_same_v<T, long double>) {
            if constexpr (sizeof(long double) == sizeof(uint64_t)) {
                return std::bit_cast<int64_t>(s);
            }
            static_assert(sizeof (T) == 0, "Unsupported long double size");
        }
    }

    // Compare two floating point numbers for almost equality within a given ULP tolerance
    template <std::floating_point T, size_t ULP_TORLERANCE = default_ulp_tolerance<T>::value>
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

    template <class T, size_t N>
    struct versor : versor<T, N-1> {
    public:
        using base_type = versor<T, N-1>;
        using value_type = typename base_type::value_type;
        using iterator = typename base_type::iterator;
        using const_iterator = typename base_type::const_iterator;
        using reference = typename base_type::reference;
        using const_reference = typename base_type::const_reference;

        template <size_t NN> requires (NN <= N) using sub_type = versor<T, NN>;

    public:
        constexpr friend size_t size(versor) noexcept { return N; }
        constexpr size_t size() const noexcept { return N; }
        static constexpr size_t total_extent = N;

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
        constexpr static auto array_at(const std::array<T, N>& arr, size_t i) noexcept {
            return (i < arr.size()) ? arr[i] : T();
        }

        template <size_t... I>
        constexpr versor(std::array<T, N>&& args_array, std::index_sequence<I...>) noexcept
            : base_type{array_at(args_array, I)...}, last{array_at(args_array, N-1)}
        {
        }

    public:
        template <size_t I>
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
        template <size_t I>
        constexpr auto& get() noexcept {
            static_assert(I < N);
            // See the comments in the const version of get<I>() for the rationale behind this condition.
            if constexpr (I + 1 == N || N == 1)
                return this->last;
            else
                return base_type::template get<I>();
        }
        template <size_t I>
        constexpr friend auto get(versor const& v) noexcept { return v.get<I>(); }
        template <size_t I>
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

        constexpr auto& operator[](size_t i) noexcept { return *(begin() + i); }
        constexpr auto operator[](size_t i) const noexcept { return *(begin() + i); }

        auto& at(size_t i) {
            if (this->size() <= i)
                throw std::range_error("versor index");
            return (*this)[i];
        }
        auto at(size_t i) const {
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
        using size_type = size_t;

    public:
        constexpr friend size_t size(versor) noexcept { return 0; }
        constexpr size_t size() const noexcept { return 0; }

        constexpr bool operator==(versor&&) const noexcept { return true; }
        constexpr bool operator==(versor const&) const noexcept { return true; }

    protected:
        constexpr auto& apply(auto&&...) noexcept { return *this; }
        constexpr friend T inner(versor, versor) noexcept { return T(); }
    };

    template <class T, size_t N>
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

namespace mvll::inline wayland::inline client
{
    template <class T> struct member_pointer_traits;
    template <class R, class T>
    struct member_pointer_traits<R T::*> {
        using class_pointer_type = T;
        using class_type = std::remove_pointer_t<T>;
        using member_type = R;
    };

    template <class T> struct function_traits;
    template <class R, class... Args>
    struct function_traits<R (*)(Args...)> {
        using return_type = R;
        static constexpr std::size_t arity = sizeof...(Args);
        using args_tuple = std::tuple<Args...>;
        template <std::size_t N> using arg_t = std::tuple_element_t<N, args_tuple>;
    };
    template <class... Rest>
    struct function_traits<void (*)(void*, Rest...)> {
        using return_type = void;
        static constexpr std::size_t rest_arity = sizeof...(Rest);
        static constexpr std::size_t arity = 1 + rest_arity;
        using rest_args_tuple = std::tuple<Rest...>;
        using args_tuple = std::tuple<void*, Rest...>;
        template <std::size_t N> using rest_arg_t = std::tuple_element_t<N, rest_args_tuple>;
        template <std::size_t N> using arg_t = std::tuple_element_t<N, args_tuple>;
    };

    template <class> constexpr wl_interface const *const interface_ptr = nullptr;
    template <class T> concept is_proxy = (interface_ptr<T> != nullptr);
    template <is_proxy T> std::string_view interface_name = interface_ptr<T>->name;
    template <is_proxy T> struct listener_type_holder { using type = std::monostate; };
#define INTERN_CLIENT_PROXY_CONCEPT(CLIENT, LISTENER)                             \
    template <> constexpr wl_interface const *const interface_ptr<CLIENT> = &CLIENT##_interface; \
    template <> struct listener_type_holder<CLIENT> { using type = LISTENER; };
    INTERN_CLIENT_PROXY_CONCEPT(wl_registry,           wl_registry_listener)
    INTERN_CLIENT_PROXY_CONCEPT(wl_compositor,         std::monostate)
    INTERN_CLIENT_PROXY_CONCEPT(wl_output,             wl_output_listener)
    INTERN_CLIENT_PROXY_CONCEPT(wl_shm,                wl_shm_listener)
    INTERN_CLIENT_PROXY_CONCEPT(wl_seat,               wl_seat_listener)
    INTERN_CLIENT_PROXY_CONCEPT(wl_surface,            wl_surface_listener)
    INTERN_CLIENT_PROXY_CONCEPT(wl_shm_pool,           std::monostate)
    INTERN_CLIENT_PROXY_CONCEPT(wl_buffer,             wl_buffer_listener)
    INTERN_CLIENT_PROXY_CONCEPT(wl_keyboard,           wl_keyboard_listener)
    INTERN_CLIENT_PROXY_CONCEPT(wl_pointer,            wl_pointer_listener)
    INTERN_CLIENT_PROXY_CONCEPT(wl_touch,              wl_touch_listener)
    INTERN_CLIENT_PROXY_CONCEPT(wl_callback,           wl_callback_listener)
    INTERN_CLIENT_PROXY_CONCEPT(xdg_wm_base,           xdg_wm_base_listener)
    INTERN_CLIENT_PROXY_CONCEPT(xdg_surface,           xdg_surface_listener)
    INTERN_CLIENT_PROXY_CONCEPT(xdg_toplevel,          xdg_toplevel_listener)
    INTERN_CLIENT_PROXY_CONCEPT(zwp_tablet_manager_v2, std::monostate)
    INTERN_CLIENT_PROXY_CONCEPT(zwp_tablet_seat_v2,    zwp_tablet_seat_v2_listener)
    INTERN_CLIENT_PROXY_CONCEPT(zwp_tablet_tool_v2,    zwp_tablet_tool_v2_listener)
#undef INTERN_CLIENT_PROXY_CONCEPT
    template <is_proxy T> using listener_type = listener_type_holder<T>::type;
    template <class T>
    concept is_proxy_observable = is_proxy<T> && !std::is_same_v<std::monostate, listener_type<T>>;

    template <is_proxy T>
    void proxy_deleter(T* raw) noexcept {
        MVLL_CHECK(raw);
        wl_proxy_destroy(reinterpret_cast<wl_proxy*>(raw));
    }
    template <is_proxy T>
    auto make_unique(T* raw) MVLL_NOEXCEPT {
        MVLL_CHECK(raw);
        return std::unique_ptr<T, std::decay_t<decltype (proxy_deleter<T>)>>(raw, proxy_deleter);
    }
    template <is_proxy T>
    using unique_ptr_type = decltype (make_unique<T>(std::declval<T*>()));

    template <class T>
    auto get_awaiter(T&& t) {
        if constexpr (requires { std::forward<T>(t).operator co_await(); }) {
            return std::forward<T>(t).operator co_await();
        }
        return std::forward<T>(t);
    }
    struct wait_current_args {};
    struct fiblet_bridge {
        virtual ~fiblet_bridge() noexcept = default;
        virtual void push(void const* src) const noexcept = 0;
    };
    template <auto Member> requires std::is_member_pointer_v<decltype (Member)>
    struct fiblet : fiblet_bridge {
        using traits = function_traits<typename member_pointer_traits<decltype (Member)>::member_type>;
        using rest_args_tuple = typename traits::rest_args_tuple;
        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;
        handle_type handle;
        struct promise_type {
            rest_args_tuple const* current;
            std::exception_ptr exception = nullptr;
            std::coroutine_handle<> previous = nullptr;
            auto get_return_object() noexcept {
                return fiblet{handle_type::from_promise(*this)};
            }
            void unhandled_exception() {
                this->exception = std::current_exception();
            }
            void return_void() const noexcept {}
            std::suspend_never initial_suspend() const noexcept { return {}; }
            auto final_suspend() const noexcept {
                struct final_awaiter {
                    bool await_ready() const noexcept { return false; }
                    void await_resume() const noexcept {}
                    std::coroutine_handle<> await_suspend(handle_type h) noexcept {
                        if (auto previous = h.promise().previous) return previous;
                        return std::noop_coroutine();
                    }
                };
                return final_awaiter{};
            }
            struct event_awaiter {
                promise_type& self;
                bool await_ready() const noexcept { return false; }
                void await_suspend(std::coroutine_handle<>) const noexcept {}
                rest_args_tuple const& await_resume() const noexcept { return *self.current; }
            };
            auto await_transform(wait_current_args) noexcept {
                return event_awaiter{*this};
            }
        };

    private:
        explicit fiblet(handle_type h) : handle{h} {}
        fiblet(fiblet const&) = delete;
        fiblet& operator=(fiblet const&) = delete;

    public:
        fiblet& operator=(fiblet&& other) noexcept {
            if (this != &other) {
                if (handle) handle.destroy();
                handle = std::exchange(other.handle, nullptr);
            }
            return *this;
        }
        ~fiblet() noexcept {
            if (handle) {
                handle.destroy();
            }
        }
        void push(void const* update) const noexcept override {
            MVLL_CHECK(handle);
            MVLL_CHECK(!handle.done());
            handle.promise().current = static_cast<rest_args_tuple const*>(update);
            handle.resume();
        }
    };

    template <class> class proxy;
    template <class T> proxy(T*) -> proxy<T>;
    template <>
    class proxy<wl_display> {
    public:
        proxy(wl_display* raw) MVLL_NOEXCEPT : ptr{raw, &wl_display_disconnect} {}
        wl_display* get() const MVLL_NOEXCEPT { return this->ptr.get(); }

    private:
        std::unique_ptr<wl_display, std::decay_t<decltype (wl_display_disconnect)>> ptr;
    };
    template <is_proxy T>
    class proxy<T> {
    public:
        proxy(T* raw) MVLL_NOEXCEPT : ptr{make_unique(raw)} {}
        T* get() const MVLL_NOEXCEPT { return this->ptr.get(); }

    private:
        unique_ptr_type<T> ptr;
    };
    template <is_proxy_observable T>
    class proxy<T> {
    public:
        template <auto Member> requires std::is_same_v<
            typename member_pointer_traits<decltype (Member)>::class_type, listener_type<T>>
        using rest_args_tuple = typename function_traits<
            typename member_pointer_traits<decltype (Member)>::member_type>::rest_args_tuple;

    private:
        static constexpr std::size_t SLOT_SIZE = sizeof (listener_type<T>) / sizeof (void*);
        static constexpr auto create_default_listener() MVLL_NOEXCEPT {
            return []<size_t... I>(std::index_sequence<I...>) MVLL_NOEXCEPT {
                return listener_type<T> {
                    ([]<class ...Rest>(void* data, Rest... rest) MVLL_NOEXCEPT {
                        auto const* pinned_raw = static_cast<movable_storage*>(data);
                        MVLL_CHECK(pinned_raw);
                        if (auto bridge = pinned_raw->slots[I].get()) {
                            auto rest_args = std::tuple{rest...};
                            bridge->push(&rest_args);
                        }
                    })...
                };
            }(std::make_index_sequence<SLOT_SIZE>());
        }

    public:
        proxy(T* raw) MVLL_NOEXCEPT
            : ptr{make_unique(raw)}
            , pin{new movable_storage{ .listener = create_default_listener(), .slots = {}}}
            {
                MVLL_CHECK(ptr != nullptr);
                MVLL_CHECK(pin != nullptr);
                MVLL_CHECK(-1 != wl_proxy_add_listener(
                    reinterpret_cast<wl_proxy*>(this->get()),
                    reinterpret_cast<void(**)(void)>(&pin->listener),
                    pin.get()));
            }
        proxy(proxy&&) noexcept = default;
        proxy& operator=(proxy&& other) noexcept = default;
        ~proxy() noexcept = default;
        proxy(proxy const&) = delete;
        proxy& operator=(proxy const&) = delete;

    public:
        T* get() const { return this->ptr.get(); }
        listener_type<T>* operator->() const MVLL_NOEXCEPT { return &pin->listener; }
        std::uint32_t id() const noexcept {
            return wl_proxy_get_id(reinterpret_cast<wl_proxy*>(this->get()));
        }

    public:
        template <class> struct fiblet_traits;
        template <auto Member> requires std::is_same_v<
            typename member_pointer_traits<decltype (Member)>::class_type, listener_type<T>>
        struct fiblet_traits<fiblet<Member>> {
            static constexpr auto member = Member;
        };
        template <class Func>
        static constexpr auto get_member_v = fiblet_traits<std::invoke_result_t<Func>>::member;

        template <class Func, class... Args>
        void attach(Func&& user_coro, Args&&... args) MVLL_NOEXCEPT {
            constexpr auto Member = get_member_v<Func>;
            static std::size_t ordinal = std::bit_cast<std::size_t>(Member) / sizeof (void*);
            MVLL_CHECK(!pin->slots[ordinal]);
            pin->slots[ordinal].reset(new fiblet<Member>{user_coro(std::forward<Args>(args)...)});
            MVLL_CHECK(pin->slots[ordinal]);
        }
        template <class Func, class...Args>
        void attach(std::move_only_function<void (Args...)>&& func, Args&&... args) MVLL_NOEXCEPT {
        }

    private:
        unique_ptr_type<T> ptr;
        struct movable_storage {
            listener_type<T> listener;
            std::array<std::unique_ptr<fiblet_bridge>, SLOT_SIZE> slots;
        };
        std::unique_ptr<movable_storage> pin;
    };

    template <is_proxy T>
    auto registry_bind(wl_registry* registry, uint32_t name, uint32_t version) MVLL_NOEXCEPT {
        return proxy{static_cast<T*>(::wl_registry_bind(registry, name, interface_ptr<T>, version))};
    }

    template <class T = color, wl_shm_format format = WL_SHM_FORMAT_XRGB8888, size_t bypp = sizeof (color)>
    [[nodiscard]] inline auto shm_allocate_buffer(wl_shm* shm, size_t cx, size_t cy) MVLL_NOEXCEPT {
        mvll::platform::unique_fd fd{::memfd_create("mvll-shm", MFD_CLOEXEC)};
        MVLL_CHECK(0 <= ::ftruncate(fd, bypp*cx*cy));
        mvll::platform::unique_mmap<T> data{nullptr, bypp*cx*cy, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0};
        auto pool = proxy{wl_shm_create_pool(shm, fd, bypp*cx*cy)};
        auto buffer = proxy{wl_shm_pool_create_buffer(pool.get(), 0, cx, cy, bypp * cx, format)};
        return std::tuple{std::move(fd), std::move(buffer), std::move(data)};
    }

    inline auto lamed(auto&& closure) noexcept {
        static auto cache = std::move(closure);
        return [](auto... args) {
            return cache(args...);
        };
    }
} // ::mvll

int main(int, char** argv) {
    using namespace mvll;
    auto display = proxy{wl_display_connect(nullptr)};
    auto registry = proxy{wl_display_get_registry(display.get())};
    std::optional<proxy<wl_compositor>> compositor;
    std::forward_list<proxy<wl_seat>> seats;
    std::optional<proxy<wl_shm>> shm;
    std::optional<proxy<xdg_wm_base>> shell;
    registry->global = lamed([&](auto, auto... rest) MVLL_NOEXCEPT {
        auto args = std::tuple{rest...};
        auto const& [registry, name, interface, version] = args;
        if (interface_name<wl_compositor> == interface) {
            std::cout << "compositor: " << args << std::endl;
            compositor.emplace(registry_bind<wl_compositor>(registry, name, version));
        }
        else if (interface_name<wl_seat> == interface) {
            std::cout << "seat: " << args << std::endl;
            seats.emplace_front(registry_bind<wl_seat>(registry, name, version));
        }
        else if (interface_name<wl_shm> == interface) {
            std::cout << "shm: " << args << std::endl;
            shm.emplace(registry_bind<wl_shm>(registry, name, version));
        }
        else if (interface_name<xdg_wm_base> == interface) {
            std::cout << "shell: " << args << std::endl;
            shell.emplace(registry_bind<xdg_wm_base>(registry, name, version));
        }
    });
    registry->global_remove = lamed([&](auto, auto... args)  MVLL_NOEXCEPT {
        auto const& [registry, name] = std::tuple{args...};
        std::erase_if(seats, [name](auto const& s) {
            return s.id() == name;
        });
    });
    wl_display_roundtrip(display.get());
    for (auto& seat : seats) {
        seat.attach([&seat] MVLL_NOEXCEPT -> fiblet<&wl_seat_listener::capabilities> {
            std::optional<proxy<wl_keyboard>> keyboard;
            std::optional<proxy<wl_pointer>> pointer;
            std::optional<proxy<wl_touch>> touch;
            for (;;) {
                [[maybe_unused]] auto const& [s, caps] = co_await wait_current_args{};
                if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
                    keyboard.emplace(proxy{wl_seat_get_keyboard(seat.get())});
                    keyboard->attach([] MVLL_NOEXCEPT -> fiblet<&wl_keyboard_listener::key> {
                        for (;;) {
                            [[maybe_unused]] auto const& args = co_await wait_current_args{};
                            std::cout << "key: " << args << std::endl;
                        }
                    });
                    keyboard->attach([] MVLL_NOEXCEPT -> fiblet<&wl_keyboard_listener::modifiers> {
                        for (;;) {
                            [[maybe_unused]] auto const& args = co_await wait_current_args{};
                            std::cout << "key mod: " << args << std::endl;
                        }
                    });
                    keyboard->attach([] MVLL_NOEXCEPT -> fiblet<&wl_keyboard_listener::repeat_info> {
                        for (;;) {
                            [[maybe_unused]] auto const& args = co_await wait_current_args{};
                            std::cout << "key repeat: " << args << std::endl;
                        }
                    });
                }
                else {
                    keyboard.reset();
                }
                if (caps & WL_SEAT_CAPABILITY_POINTER) {
                    pointer.emplace(proxy{wl_seat_get_pointer(seat.get())});
                    pointer->attach([]  MVLL_NOEXCEPT -> fiblet<&wl_pointer_listener::axis_value120> {
                        for (;;) {
                            [[maybe_unused]] auto const& args = co_await wait_current_args{};
                            std::cout << "axis120: " << args << std::endl;
                        }
                    });
                }
                else {
                    pointer.reset();
                }
                if (caps & WL_SEAT_CAPABILITY_TOUCH) {
                    touch = proxy{wl_seat_get_touch(seat.get())};
                    touch->attach([] MVLL_NOEXCEPT -> fiblet<&wl_touch_listener::motion> {
                        for (;;) {
                            [[maybe_unused]] auto const& args = co_await wait_current_args{};
                            std::cout << "touch.motion: " << args << std::endl;
                        }
                    });
                }
                else {
                    touch.reset();
                }
            }
        });
    }
    wl_display_roundtrip(display.get());

    MVLL_CHECK(compositor.has_value());
    MVLL_CHECK(shm.has_value());
    MVLL_CHECK(shell.has_value());
    shell.value()->ping = [](auto, auto shell, auto serial) noexcept {
        xdg_wm_base_pong(shell, serial);
    };
    auto surface = proxy{wl_compositor_create_surface(compositor.value().get())};
    auto xsurface = proxy{xdg_wm_base_get_xdg_surface(shell.value().get(), surface.get())};
    xsurface->configure = [](auto, auto xsurface, auto serial) noexcept {
        xdg_surface_ack_configure(xsurface, serial);
    };

    auto toplevel = proxy{xdg_surface_get_toplevel(xsurface.get())};
    toplevel.attach([&] MVLL_NOEXCEPT -> fiblet<&xdg_toplevel_listener::configure> {
        std::size_t scale = 1;
        std::size_t cx = 640 * scale;
        std::size_t cy = 480 * scale;
        auto primary = shm_allocate_buffer(shm.value().get(), cx, cy);
        auto secondary = shm_allocate_buffer(shm.value().get(), cx, cy);
        auto& [fd, buffer, pixels] = primary;
        auto frame = proxy{wl_surface_frame(surface.get())}; 
        auto que = sycl::queue();
        std::cout << que.get_device().get_info<sycl::info::device::name>() << std::endl;
        for (;;) {
            auto const& args = co_await wait_current_args{};
            std::cout << "toplevel.configure: " << args << std::endl;
            auto const& [toplevel, h, w, states] = args;
            cx = h * scale;
            cy = w * scale;
            if (cx * cy > 0) {
                secondary = shm_allocate_buffer(shm.value().get(), cx, cy);
            }
            else { // the initial configuration
                wl_surface_attach(surface.get(), buffer.get(), 0, 0);
                wl_surface_damage(surface.get(), 0, 0, cx, cy);
                wl_surface_commit(surface.get());
            }
            buffer->release = lamed([&](...) {
                std::swap(primary, secondary);
            });
            frame->done = lamed([&](...) MVLL_NOEXCEPT {
                auto next = proxy{wl_surface_frame(surface.get())};
                next->done = frame->done;
                frame = std::move(next);
                wl_surface_attach(surface.get(), buffer.get(), 0, 0);
                wl_surface_damage(surface.get(), 0, 0, cx, cy);
                wl_surface_commit(surface.get());
                wl_display_flush(display.get());
            });
        }
    });
    bool quit = false;
    toplevel->close = lamed([&](...) MVLL_NOEXCEPT {
        quit = true;
    });
    xdg_toplevel_set_app_id(toplevel.get(), std::filesystem::path(argv[0]).filename().c_str());

    wl_surface_commit(surface.get());
    while (-1 != wl_display_dispatch(display.get())) {
        if (quit) break;
    }
    return 0;
}
