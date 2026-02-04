#ifndef INCLUDE_MVLL_WAYLAND_CLIENT_PROXY_HPP
#define INCLUDE_MVLL_WAYLAND_CLIENT_PROXY_HPP

#include <mvll/error-handling.hpp>

#include <array>
#include <coroutine>
#include <iosfwd>
#include <tuple>
#include <type_traits>
#include <utility>

#include <cstddef>
#include <cstdint>

#include <mvll/fiblet.hpp>
#include <mvll/platform/linux.hpp>
#include <mvll/memory.hpp>
#include <mvll/versor.hpp>

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <mvll/wayland/client/proxy-meta.hpp>

namespace mvll::inline wayland::inline client
{
    template <auto Member, class Func>
    struct listener_action final {
        Func func;
        listener_action(Func&& func) : func{std::forward<Func>(func)} {}
        void push(void const* src) const {
            using actual_args_tuple = event_traits<Member>::actual_args_tuple;
            auto const& actual_args = *static_cast<actual_args_tuple const*>(src);
            if constexpr (requires {this->func(actual_args);}) {
                this->func(actual_args);
            }
            else {
                std::apply(this->func, actual_args);
            }
        }
    };

    template <auto Member>
    struct listener_fiblet final
        : fiblet<typename event_traits<Member>::actual_args_tuple>
    {
        using fiblet_type = fiblet<typename event_traits<Member>::actual_args_tuple>;
        using fiblet_type::fiblet_type;
        void push(void const* src) const {
            fiblet_base::push(src);
        }
    };

    template <class Func, class Tuple>
    struct listener_action_traits {
        static inline constexpr bool is_unpacked_invocable = []<class... Args>(std::tuple<Args...>*)
            consteval noexcept
        {
            return std::is_invocable_v<Func, Args...>;
        }(static_cast<Tuple*>(nullptr));
        static inline constexpr bool is_tuple_invocable = std::is_invocable_v<Func, Tuple>;
        static inline constexpr bool is_invocable = is_unpacked_invocable || is_tuple_invocable;
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

    template <is_proxy_observable T>
    class thunk_table final {
        friend class proxy<T>;

    public:
        static inline constexpr std::size_t SIZE = pfr::count_members<listener_type<T>>();
        struct table_entry {
            void* self;
            void (*pusher)(void const*, void const*);
            move_only_erased_box<> action_cache;
            move_only_erased_box<> fiblet_cache;
            move_only_erased_box<> payload_cache;
        };
        using table_type = std::array<table_entry, SIZE>;

    private:
        static inline listener_type<T> listener = []<std::size_t ...I>(std::index_sequence<I...>) noexcept {
            return listener_type<T> {
                ([]<class ...Rest>(void* data, T* raw, Rest ...rest) {
                    using traits = event_signature_traits<void (*)(void*, T*, Rest...)>;
                    if (table_type* table = static_cast<table_type*>(data)) {
                        table_entry& entry = (*table)[I];
                        auto actual_args = std::tuple{raw, rest...};
                        traits::to_flatten(entry.payload_cache, std::tuple{rest...});
                        if (entry.action_cache) {
                            entry.pusher(entry.self, &actual_args);
                        }
                        else if (entry.fiblet_cache) {
                            entry.pusher(entry.self, &actual_args);
                        }
                    }
                })...
            };
        }(std::make_index_sequence<SIZE>());

    public:
        thunk_table(proxy<T>* proxy)
            : proxy_{proxy}
            , erased_table_{table_type{}} {}

    public:
        [[nodiscard]] std::int32_t start(T* proxy_raw) noexcept {
            return wl_proxy_add_listener(
                reinterpret_cast<wl_proxy*>(proxy_raw),
                reinterpret_cast<void(**)(void)>(&listener),
                this->erased_table_.get<table_type*>());
        }
        template <auto Member, class Func, class ...InitialArgs>
        void add_fiblet(Func&& coro, InitialArgs&& ...init) noexcept {
            using DecayFunc = std::decay_t<Func>;
            using FibletType = listener_fiblet<Member>;
            constexpr std::size_t ordinal = event_traits<Member>::ordinal;
            table_entry& entry = (*static_cast<table_type*>(erased_table_))[ordinal];
            struct cache_entry {
                DecayFunc coro;
                listener_fiblet<Member> flit;
            };
            auto& cache = (entry.fiblet_cache = cache_entry{std::forward<Func>(coro), {}});
            cache.flit = FibletType{(cache.coro)(std::forward<InitialArgs>(init)...)};
            entry.self = &cache.flit;
            entry.pusher = [](void const* self, void const* args) {
                static_cast<FibletType const*>(self)->push(args);
            };
            cache.flit.handle().resume();
        }
        template <auto Member, class Func>
        void add_action(Func&& func) noexcept {
            using DecayFunc = std::decay_t<Func>;
            using ActionType = listener_action<Member, DecayFunc>;
            constexpr std::size_t ordinal = event_traits<Member>::ordinal;
            table_entry& entry = (*static_cast<table_type*>(erased_table_))[ordinal];
            ActionType& cache = (entry.action_cache = ActionType{std::forward<DecayFunc>(func)});
            entry.self = &cache;
            entry.pusher = [](void const* self, void const* args) {
                static_cast<ActionType const*>(self)->push(args);
            };
        }
        template <auto Member>
        auto const* peek() const noexcept {
            using traits = event_traits<Member>;
            return (*(erased_table_.get<table_type>()))[traits::ordinal]
                .payload_cache.template get<typename traits::payload_flat_tuple>();
        }

    private:
        proxy<T>* proxy_; // T.B.D.
        move_only_erased_box<> erased_table_;
    };

    template <is_proxy T>
    class proxy_impl {
    public:
        static inline constexpr auto metainfo = metadb[static_cast<std::size_t>(identifier<T>)];
        static inline constexpr auto interface_ptr = mvll::interface_ptr<T>;
        static inline constexpr auto interface_name = mvll::interface_name<T>;

    public:
        proxy_impl(std::nullptr_t = nullptr) noexcept
            : ptr_{nullptr, delete_proxy<T>}
            , anchor{} {}
        proxy_impl(T* raw) MVLL_NOEXCEPT
            : ptr_{raw, delete_proxy<T>}
            , anchor{}
            {
                MVLL_CHECK(raw);
            }
        proxy_impl(proxy_impl&& other) noexcept
            : ptr_{other.ptr_.release(), delete_proxy<T>}
            , anchor{std::exchange(other.anchor, {})} {}
        proxy_impl& operator=(proxy_impl&& other) noexcept {
            if (this != &other) {
                this->ptr_.reset(other.ptr_.release());
                this->anchor = std::exchange(other.anchor, {});
            }
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
        [[nodiscard]] bool has_anchor() const noexcept { return static_cast<bool>(anchor); }
        template <is_boxable_type R>
        std::decay_t<R>& emplace_anchor(R&& src) {
            return anchor.emplace(std::forward<R>(src));
        }

    public:
        template <class Ch, class Tr>
        friend std::basic_ostream<Ch, Tr>& operator<<(std::basic_ostream<Ch, Tr>& output,
                                                      proxy_impl const& x) {
            return output << std::tuple{interface_name, x.id(), x.get()};
        }

    private:
        unique_pointer<T> ptr_;

    public:
        move_only_erased_box<> anchor;
    };

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
            , table_{this}
            {}
        proxy(T* raw)
            : proxy_impl<T>{raw}
            , table_{this}
            { MVLL_CHECK(-1 != table_.start(this->get())); }

        proxy(proxy&& other) noexcept
            : proxy_impl<T>{std::exchange<proxy_impl<T>>(other, {})}
            , table_{std::exchange(other.table_, {nullptr})}
            {
                table_.proxy_ = this;
            }

        proxy& operator=(proxy&& other) noexcept {
            if (this != &other) {
                proxy_impl<T>::operator=(std::exchange<proxy_impl<T>>(other, {}));
                table_ = std::exchange(other.table_, {nullptr});
                table_.proxy_ = this;
            }
            return *this;
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
            MVLL_CHECK(this->get());
            using DecayFunc = std::decay_t<Func>;
            static_assert(std::is_invocable_v<DecayFunc, InitialArgs...>);
            constexpr auto Member = member_from_fiblet_v<DecayFunc, InitialArgs...>;
            static_assert(std::is_same_v<typename event_traits<Member>::listener_type, listener_type<T>>);
            table_.template add_fiblet<Member>(std::forward<Func>(coro), std::forward<InitialArgs>(init)...);
        }
        template <auto Member, class Func>
        void on(Func&& func) {
            MVLL_CHECK(this->get());
            using DecayFunc = std::decay_t<Func>;
            static_assert(std::is_same_v<typename event_traits<Member>::listener_type, listener_type<T>>);
            static_assert(is_action_invocable_v<DecayFunc, typename event_traits<Member>::actual_args_tuple>);
            table_.template add_action<Member>(std::forward<Func>(func));
        }

    public:
        template <auto Member>
        struct action_assigner {
            proxy& self;
            void operator=(auto&& func) {
                self.template on<Member>(std::forward<decltype(func)>(func));
            }
        };
        template <auto Member>
        auto action() { return action_assigner<Member>{*this}; }

        struct fiblet_assigner {
            proxy& self;
            template <class Func>
            void operator=(Func&& coro) {
                if constexpr (std::is_invocable_v<Func, proxy&>) {
                    self.on(std::forward<Func>(coro), self);
                }
                else if constexpr (std::is_invocable_v<Func>) {
                    self.on(std::forward<Func>(coro));
                }
                else {
                    static_assert("Unsupported fiblet signature (please use proxy::on)");
                }
            }
        };
        auto fiblet() { return fiblet_assigner{*this}; }

    public:
        template <auto Member>
        requires std::is_same_v<typename event_traits<Member>::listener_type, listener_type<T>>
        auto const* peek() const noexcept {
            return table_.template peek<Member>();
        }

    private:
        thunk_table<T> table_ = {};
    };

    template <is_proxy T>
    [[nodiscard]] auto registry_bind(wl_registry* registry, uint32_t name, uint32_t version) MVLL_NOEXCEPT {
        return proxy{static_cast<T*>(::wl_registry_bind(registry, name, interface_ptr<T>, version))};
    }

    template <class T = color, wl_shm_format FORMAT = WL_SHM_FORMAT_XRGB8888, std::size_t BYPP = sizeof (color)>
    [[nodiscard]] auto shm_allocate_buffer(wl_shm* shm,
                                           std::size_t cx,
                                           std::size_t cy,
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
         : coroutine_traits<mvll::fiblet<typename mvll::event_traits<Member>::actual_args_tuple>, Args...> {
        using base_traits = coroutine_traits<
            mvll::fiblet<typename mvll::event_traits<Member>::actual_args_tuple>, Args...>;
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
