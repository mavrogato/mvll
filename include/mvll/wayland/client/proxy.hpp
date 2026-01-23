#ifndef INCLUDE_MVLL_WAYLAND_CLIENT_PROXY_HPP
#define INCLUDE_MVLL_WAYLAND_CLIENT_PROXY_HPP

#include <array>
#include <coroutine>

#include <mvll/fiblet.hpp>
#include <mvll/platform/linux.hpp>
#include <mvll/versor.hpp>

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <mvll/wayland/client/proxy-meta.hpp>

namespace mvll::inline wayland::inline client
{
    struct listener_thunk {
        virtual ~listener_thunk() noexcept = default;
        virtual void push(void const* src) const = 0;
        listener_thunk() = default;
        listener_thunk(listener_thunk const&) = delete;
        listener_thunk& operator=(listener_thunk const&) = delete;
    };

    template <auto Member, class Func>
    struct listener_action final : listener_thunk {
        Func func;
        listener_action(Func&& func) : func{std::forward<Func>(func)} {}
        virtual void push(void const* src) const override {
            using rest_args_tuple = event_traits<Member>::rest_args_tuple;
            auto const& rest_args = *static_cast<rest_args_tuple const*>(src);
            if constexpr (requires {this->func(rest_args);}) {
                this->func(rest_args);
            }
            else {
                std::apply(this->func, rest_args);
            }
        }
    };

    template <auto Member>
    struct listener_fiblet final
        : listener_thunk
        , fiblet<typename event_traits<Member>::rest_args_tuple>
    {
        using fiblet_type = fiblet<typename event_traits<Member>::rest_args_tuple>;
        using fiblet_type::fiblet_type;
        void push(void const* input) const override {
            fiblet_base::push(input);
        }
    };

    template <class Func, class Tuple>
    struct listener_action_traits {
        static inline constexpr bool is_invocable = []<class... Args>(std::tuple<Args...>*) consteval noexcept {
            return std::is_invocable_v<Func, Args...>;
        }((Tuple*)nullptr);
    };
    template <class Func, class Tuple>
    inline constexpr bool is_action_invocable_v = listener_action_traits<Func, Tuple>::is_invocable;

    template <class> struct listener_fiblet_traits;
    template <auto Member> requires is_listener<typename event_traits<Member>::listener_type>
    struct listener_fiblet_traits<listener_fiblet<Member>> {
        static constexpr auto member = Member;
    };
    template <class Func, class ...Args>
    static inline constexpr auto member_from_fiblet_v =
        listener_fiblet_traits<std::invoke_result_t<Func, Args...>>::member;

    template <is_proxy T>
    class proxy_impl {
    public:
        static inline constexpr auto metainfo = metadb[static_cast<std::size_t>(identifier<T>)];
        static inline constexpr auto interface_ptr = mvll::interface_ptr<T>;
        static inline constexpr auto interface_name = mvll::interface_name<T>;

    public:
        proxy_impl(T* raw = nullptr) noexcept : ptr_{raw, delete_proxy<T>} {}
        proxy_impl(proxy_impl&& other) noexcept
            : ptr_{other.ptr_.release(), delete_proxy<T>} {}

        proxy_impl& operator=(proxy_impl&& other) noexcept {
            this->ptr_.reset(other.ptr_.release());
            return *this;
        }

    public:
        void reset(T* raw = nullptr) noexcept { this->ptr_.reset(raw); }
        [[nodiscard]] T* get() const noexcept { return this->ptr_.get(); }
        [[nodiscard]] operator T*() const noexcept { return this->get(); }
        [[nodiscard]] explicit operator bool() const noexcept { return this->ptr_.operator bool(); }
        [[nodiscard]] std::uint32_t id() const noexcept {
            MVLL_CHECK(this->operator bool());
            return wl_proxy_get_id(reinterpret_cast<wl_proxy*>(this->get()));
        }

    public:
        template <class Ch, class Tr>
        friend std::basic_ostream<Ch, Tr>& operator<<(std::basic_ostream<Ch, Tr>& output,
                                                      proxy_impl const& x) {
            return output << std::tuple{interface_name, x.id(), x.get()};
        }

    private:
        unique_pointer<T> ptr_;
    };

    template <is_proxy_observable T>
    class thunk_table final {
    public:
        static inline constexpr std::size_t SIZE = pfr::count_members<listener_type<T>>();
        using table_type = std::array<std::unique_ptr<listener_thunk>, SIZE>;

    private:
        static inline listener_type<T> listener = []<std::size_t ...I>(std::index_sequence<I...>) noexcept {
            return listener_type<T> {
                ([]<class ...Rest>(void* data, Rest... rest) {
                    if (table_type const* table = static_cast<table_type*>(data)) {
                        if (listener_thunk const* thunk = (*table)[I].get()) {
                            auto rest_args = std::tuple{rest...};
                            thunk->push(&rest_args);
                        }
                    }
                })...
            };
        }(std::make_index_sequence<SIZE>());

    public:
        thunk_table() : table_{new table_type{}} {}

    public:
        [[nodiscard]] std::int32_t start(T* target) noexcept {
            return wl_proxy_add_listener(
                reinterpret_cast<wl_proxy*>(target),
                reinterpret_cast<void(**)(void)>(&listener),
                this->table_.get());
        }

        template <auto Member> requires std::is_same_v<typename event_traits<Member>::proxy_type, T>
        void add(listener_thunk* raw) noexcept {
            constexpr std::size_t ordinal = event_traits<Member>::ordinal;
            (*table_.get())[ordinal].reset(raw);
        }

    private:
        std::unique_ptr<table_type> table_;
    };

    template <class> class proxy;
    template <class T> proxy(T*) -> proxy<T>;
    template <is_proxy T>
    class proxy<T> : public proxy_impl<T> {
    public:
        using proxy_impl<T>::proxy_impl;
    };
    template <is_proxy_observable T>
    class proxy<T> : public proxy_impl<T> {
    public:
        proxy(std::nullptr_t = nullptr)
            : proxy_impl<T>{}
            , table_{}
            , persistent_buffer_{}
            {
            }
        proxy(T* raw)
            : proxy_impl<T>{raw}
            , table_{}
            , persistent_buffer_{}
            {
                MVLL_CHECK(-1 != table_.start(this->get()));
            }

    public:
        void rebind(T* raw) MVLL_NOEXCEPT {
            MVLL_CHECK(raw != this->get());
            this->reset(raw);
            MVLL_CHECK(-1 != table_.start(this->get()));
        }

    public:
        template <class Func, class ...InitialArgs>
        void on(Func&& coro, InitialArgs&&... init) {
            using DecayFunc = std::decay_t<Func>;
            constexpr auto Member = member_from_fiblet_v<DecayFunc, InitialArgs...>;
            constexpr std::size_t ordinal = event_traits<Member>::ordinal;
            static_assert(std::is_same_v<typename event_traits<Member>::listener_type, listener_type<T>>);
            auto persistent_coro = sustain_optimized_out(std::forward<Func>(coro), ordinal);
            auto flit = new listener_fiblet<Member>{(*persistent_coro)(std::forward<InitialArgs>(init)...)};
            table_.template add<Member>(flit);
            flit->handle().resume();
        }
        template <auto Member, class Func>
        void on(Func&& func) {
            using DecayFunc = std::decay_t<Func>;
            static_assert(std::is_same_v<typename event_traits<Member>::listener_type, listener_type<T>>);
            static_assert(is_action_invocable_v<DecayFunc, typename event_traits<Member>::rest_args_tuple>);
            auto action = new listener_action<Member, DecayFunc>{std::forward<DecayFunc>(func)};
            table_.template add<Member>(action);
        }

    private:
        struct optimized_out_sustainer {
            std::byte* chunk = nullptr;
            void (*release)(void*) noexcept = nullptr;

            optimized_out_sustainer() noexcept = default;
            optimized_out_sustainer(std::byte* chunk, void (*release)(void*) noexcept) noexcept
                : chunk{chunk}
                , release{release}
                {}
            optimized_out_sustainer(optimized_out_sustainer&& other) noexcept
                : chunk(std::exchange(other.chunk, nullptr))
                , release(std::exchange(other.release, nullptr)) {}
            optimized_out_sustainer& operator=(optimized_out_sustainer&& other) noexcept {
                if (this != &other) {
                    if (chunk && release) release(chunk);
                    chunk = std::exchange(other.chunk, nullptr);
                    release = std::exchange(other.release, nullptr);
                }
                return *this;
            }
            ~optimized_out_sustainer() noexcept {
                if (chunk && release) release(chunk);
            }
            optimized_out_sustainer(optimized_out_sustainer const&) = delete;
            optimized_out_sustainer& operator=(optimized_out_sustainer const&) = delete;
        };
        template <class Func>
        auto const* sustain_optimized_out(Func&& coro, std::uint32_t ordinal) {
            using DecayFunc = std::decay_t<Func>;
            static constexpr std::align_val_t alignment = std::align_val_t{alignof (DecayFunc)};
            auto chunk = new (alignment) std::byte[sizeof (DecayFunc)];
            persistent_buffer_[ordinal] = {
                chunk,
                [](void* raw) noexcept {
                    static_cast<DecayFunc*>(raw)->~DecayFunc();
                    ::operator delete[](raw, alignment);
                },
            };
            return (new (chunk) DecayFunc(std::forward<Func>(coro)));
        }

    private:
        thunk_table<T> table_ = {};
        std::array<optimized_out_sustainer, thunk_table<T>::SIZE> persistent_buffer_ = {};
    };

    template <is_proxy T>
    auto registry_bind(wl_registry* registry, uint32_t name, uint32_t version) MVLL_NOEXCEPT {
        return proxy{static_cast<T*>(::wl_registry_bind(registry, name, interface_ptr<T>, version))};
    }

    template <class T = color, wl_shm_format FORMAT = WL_SHM_FORMAT_XRGB8888, size_t BYPP = sizeof (color)>
    [[nodiscard]] inline auto shm_allocate_buffer(wl_shm* shm,
                                                  size_t cx,
                                                  size_t cy,
                                                  std::size_t bypp = BYPP,
                                                  wl_shm_format format = FORMAT) {
        mvll::platform::unique_fd fd{::memfd_create("mvll-shm", MFD_CLOEXEC)};
        MVLL_CHECK(0 <= ::ftruncate(fd, bypp*cx*cy));
        mvll::platform::unique_mmap<T> data{nullptr, bypp*cx*cy, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0};
        auto pool = proxy{wl_shm_create_pool(shm, fd, bypp*cx*cy)};
        auto buffer = proxy{wl_shm_pool_create_buffer(pool.get(), 0, cx, cy, bypp * cx, format)};
        return std::tuple{std::move(fd), std::move(buffer), std::move(data)};
    }

} // ::mvll::wayland::client

namespace std
{
    template <auto Member, class ...Args>
    struct coroutine_traits<mvll::listener_fiblet<Member>, Args...>
         : coroutine_traits<mvll::fiblet<typename mvll::event_traits<Member>::rest_args_tuple>, Args...> {
         using base_traits = coroutine_traits<
             mvll::fiblet<typename mvll::event_traits<Member>::rest_args_tuple>, Args...>;
         struct promise_type : base_traits::promise_type {
             mvll::listener_fiblet<Member> get_return_object() noexcept {
                return mvll::listener_fiblet<Member> {
                    std::coroutine_handle<promise_type>::from_promise(*this),
                };
            }
        };
    };
} // ::std


#endif /*INCLUDE_MVLL_WAYLAND_CLIENT_PROXY_HPP*/
