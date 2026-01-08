
#include "mvll/error-handling.hpp"

#include <array>
#include <bit>
#include <coroutine>
#include <exception>
#include <filesystem>
#include <forward_list>
#include <iostream>
#include <memory>
#include <tuple>
#include <type_traits>
#include <variant>

#include <mvll/cpp2x/generator.hpp>
#include <mvll/cpp2x/tuple-support.hpp>
#include <mvll/platform/linux.hpp>
#include <mvll/unique.hpp>
#include "mvll/versor.hpp"

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

#include <xdg-shell-client.h>
#include <zwp-tablet-v2-client.h>


namespace mvll::inline wayland::inline client
{
    template <class> constexpr wl_interface const *const interface_ptr = nullptr;
    template <class T> concept is_proxy = (interface_ptr<T> != nullptr);
    template <is_proxy T> std::string_view interface_name = interface_ptr<T>->name;
    template <is_proxy T> struct listener_type_holder { using type = std::monostate; };
#define INTERN_CLIENT_PROXY_CONCEPT(CLIENT, LISTENER)                             \
    template <> constexpr wl_interface const *const interface_ptr<CLIENT> = &CLIENT##_interface; \
    template <> struct listener_type_holder<CLIENT> { using type = LISTENER; };
    INTERN_CLIENT_PROXY_CONCEPT(wl_display,            std::monostate)
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

    namespace internals
    {
        template <class T> struct member_pointer_traits;
        template <class R, class T>
        struct member_pointer_traits<R T::*> {
            using class_pointer_type = T;
            using class_type = std::remove_pointer_t<T>;
            using member_type = R;
        };
        template <class T> struct listener_callback_traits;
        template <class... Rest>
        struct listener_callback_traits<void (*)(void*, Rest...)> {
            using return_type = void;
            static constexpr std::size_t rest_arity = sizeof...(Rest);
            static constexpr std::size_t arity = 1 + rest_arity;
            using rest_args_tuple = std::tuple<Rest...>;
            using args_tuple = std::tuple<void*, Rest...>;
            template <std::size_t N> using rest_arg_t = std::tuple_element_t<N, rest_args_tuple>;
            template <std::size_t N> using arg_t = std::tuple_element_t<N, args_tuple>;
        };
    }
    template <auto Member> requires (std::is_member_pointer_v<decltype (Member)>)
    using listener_member_pointer_traits = internals::member_pointer_traits<decltype (Member)>;
    template <auto Member>
    using listener_callback_class = typename listener_member_pointer_traits<Member>::class_type;
    template <auto Member>
    using listener_callback_traits = internals::listener_callback_traits<
        typename listener_member_pointer_traits<Member>::member_type>;
    template <auto Member>
    using listener_callback_rest_args_tuple = typename listener_callback_traits<Member>::rest_args_tuple;

    template <is_proxy T>
    void proxy_deleter(T* raw) noexcept {
        MVLL_CHECK(raw);
        wl_proxy_destroy(reinterpret_cast<wl_proxy*>(raw));
    }
    template <>
    void proxy_deleter<wl_display>(wl_display* raw) noexcept {
        MVLL_CHECK(raw);
        wl_display_disconnect(raw);
    }

    namespace internals
    {
        template <class T>
        auto get_awaiter(T&& t) {
            if constexpr (requires { std::forward<T>(t).operator co_await(); }) {
                return std::forward<T>(t).operator co_await();
            }
            return std::forward<T>(t);
        }

        template <class T, void DELETER(T*) = nullptr>
        class move_only_pointer {
            constexpr move_only_pointer(T const&) = delete;
            constexpr move_only_pointer& operator=(T const&) = delete;
            static void deleter(T* raw) noexcept {
                if constexpr (DELETER) {
                    DELETER(raw);
                }
                else {
                    delete raw; // delete[] not supported!
                }
            }

        public:
            constexpr move_only_pointer(T* raw = nullptr) noexcept : ptr_{raw} {}
            constexpr move_only_pointer(move_only_pointer&& other) noexcept
                : ptr_{std::exchange(other.ptr_, nullptr)}
                {}
            constexpr move_only_pointer& operator=(move_only_pointer&& other) noexcept {
                reset(std::exchange(other.ptr_, nullptr));
                return *this;
            }
            constexpr void reset(T* raw = nullptr) noexcept {
                if (auto old = std::exchange(this->ptr_, raw)) {
                    if (old != this->ptr_) {
                        deleter(old);
                    }
                }
            }
            constexpr ~move_only_pointer() noexcept { reset(); }
            constexpr auto operator<=>(move_only_pointer const&) const noexcept = default;
            constexpr T* get() const noexcept { return this->ptr_; }
            constexpr operator T*() const noexcept { return this->get(); }
            constexpr explicit operator bool() const noexcept { return this->get() != nullptr; }

        private:
            T* ptr_;
        };
    } // ::inernals
    struct wait_current_args {};
    struct listener_thunk {
        virtual ~listener_thunk() noexcept = default;
        virtual void push(void const* src) const = 0;
        listener_thunk() = default;
        listener_thunk(listener_thunk const&) = delete;
        listener_thunk& operator=(listener_thunk const&) = delete;
    };
    template <auto Member, class Func>
    struct action final : listener_thunk {
        Func func;
        action(Func&& func) : func{std::forward<Func>(func)} {}
        virtual void push(void const* src) const override {
            using rest_args_tuple = listener_callback_rest_args_tuple<Member>;
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
    struct fiblet final : listener_thunk {
        using rest_args_tuple = listener_callback_rest_args_tuple<Member>;
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
        void push(void const* update) const override {
            MVLL_CHECK(handle);
            MVLL_CHECK(!handle.done());
            handle.promise().current = static_cast<rest_args_tuple const*>(update);
            handle.resume();
        }
    };

    template <is_proxy T>
    class proxy_impl : public internals::move_only_pointer<T, proxy_deleter<T>> {
    public:
        using base_type = internals::move_only_pointer<T, proxy_deleter<T>>;
        static constexpr auto interface_ptr = mvll::interface_ptr<T>;
        static inline std::string_view interface_name = mvll::interface_name<T>;

    public:
        using base_type::base_type;

    public:
        std::uint32_t id() const noexcept {
            return wl_proxy_get_id(reinterpret_cast<wl_proxy*>(this->get()));
        }
        std::string_view name() const noexcept {
            return interface_name;
        }

    public:
        template <class Ch, class Tr>
        friend constexpr std::basic_ostream<Ch, Tr>& operator<<(std::basic_ostream<Ch, Tr>& output,
                                                                proxy_impl const& x) {
            return output << std::tuple{x.name(), x.id(), x.get()};
        }
    };

    template <is_proxy_observable T>
    class thunk_table final {
    public:
        static constexpr std::size_t SIZE = sizeof (listener_type<T>) / sizeof (void*);
        using table_type = std::array<internals::move_only_pointer<listener_thunk>, SIZE>;

    private:
        static auto listener = []<std::size_t ...I>(std::index_sequence<I...>) noexcept {
            return listener_type<T> {
                ([]<class ...Rest>(void* data, Rest... rest) {
                    if (table_type const* table = static_cast<table_type*>(data)) {
                        if (listener_thunk const* thunk = table[I].get()) {
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
        std::int32_t start(T* target) noexcept {
            return wl_proxy_add_listener(
                target,
                reinterpret_cast<void(**)(void)>(&listener),
                this->table_.get());
        }

        template <auto Member> //!!!
        void add(listener_thunk* raw) noexcept {
            static std::size_t ordinal = std::bit_cast<std::size_t>(Member) / sizeof (void*);
            (*table_.get())[ordinal] = raw;
        }

    private:
        internals::move_only_pointer<table_type> table_;
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
    private:
        static constexpr std::size_t THUNK_TABLE_SIZE = sizeof (listener_type<T>) / sizeof (void*);
        struct storage {
            std::array<std::unique_ptr<listener_thunk>, THUNK_TABLE_SIZE> slots;
        };
        static inline auto listener = []<size_t... I>(std::index_sequence<I...>) noexcept {
            return listener_type<T> {
                ([]<class ...Rest>(void* data, Rest... rest) {
                    auto const* pinned_raw = static_cast<storage*>(data);
                    MVLL_CHECK(pinned_raw);
                    if (auto const* bridge = pinned_raw->slots[I].get()) {
                        auto rest_args = std::tuple{rest...};
                        bridge->push(&rest_args);
                    }
                })...
            };
        }(std::make_index_sequence<THUNK_TABLE_SIZE>());

    public:
        proxy()
            : proxy_impl<T>::proxy_impl{}
            , pin{}
            {
            }
        proxy(T* raw)
            : proxy_impl<T>::proxy_impl{raw}
            , pin{new storage{.slots = {}}}
            {
                MVLL_CHECK(pin != nullptr);
                MVLL_CHECK(-1 != wl_proxy_add_listener(
                    reinterpret_cast<wl_proxy*>(this->get()),
                    reinterpret_cast<void(**)(void)>(&listener),
                    pin.get()));
            }
        ~proxy() noexcept {
            // if (thunk_array) {
            //     for (std::size_t i = 0; i < THUNK_ARRAY_SIZE; ++i) {
            //         delete std::exchange(thunk_array[i], nullptr);
            //     }
            //     delete[] std::exchange(thunk_array, nullptr);
            // }
        }
        proxy(proxy&&) noexcept = default;
        proxy& operator=(proxy&& other) noexcept = default;
        proxy(proxy const&) = delete;
        proxy& operator=(proxy const&) = delete;

    public:
        void rebind(T* raw) MVLL_NOEXCEPT {
            MVLL_CHECK(raw != this->get());
            this->reset(raw);
            MVLL_CHECK(-1 != wl_proxy_add_listener(
                reinterpret_cast<wl_proxy*>(this->get()),
                reinterpret_cast<void(**)(void)>(&listener),
                pin.get()));
        }

    public:
        template <class> struct fiblet_traits;
        template <auto Member> requires std::is_same_v<listener_type<T>,
                                                       listener_callback_class<Member>>
        struct fiblet_traits<fiblet<Member>> {
            static constexpr auto member = Member;
        };
        template <class Func>
        static constexpr auto get_member_v = fiblet_traits<std::invoke_result_t<Func>>::member;
        template <class Func, class... InitialArgs>
        void on(Func&& coro, InitialArgs&&... init) {
            constexpr auto Member = get_member_v<Func>;
            static std::size_t ordinal = std::bit_cast<std::size_t>(Member) / sizeof (void*);
            pin->slots[ordinal].reset(new fiblet<Member>{coro(std::forward<InitialArgs>(init)...)});
            MVLL_CHECK(pin->slots[ordinal]);
        }
        template <auto Member, class Func>
        void on(Func&& func) {
            using DecayFunc = std::decay_t<Func>;
            static std::size_t ordinal = std::bit_cast<std::size_t>(Member) / sizeof (void*);
            pin->slots[ordinal].reset(new action<Member, DecayFunc>{std::forward<DecayFunc>(func)});
            MVLL_CHECK(pin->slots[ordinal]);
        }

    private:
        std::unique_ptr<storage> pin;
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

} // ::mvll

int main(int, char** argv) {
    using namespace mvll;
    auto display = proxy{wl_display_connect(nullptr)};
    auto registry = proxy{wl_display_get_registry(display.get())};
    proxy<wl_compositor> compositor;
    std::forward_list<proxy<wl_seat>> seats;
    proxy<wl_shm> shm;
    proxy<xdg_wm_base> shell;
    registry.on<&wl_registry_listener::global>([&](wl_registry*  registry,
                                                   std::uint32_t name,
                                                   char const*   interface,
                                                   std::uint32_t version) {
        if (interface_name<wl_compositor> == interface) {
            compositor = registry_bind<wl_compositor>(registry, name, version);
        }
        else if (interface_name<wl_seat> == interface) {
            seats.emplace_front(registry_bind<wl_seat>(registry, name, version));
        }
        else if (interface_name<wl_shm> == interface) {
            shm = registry_bind<wl_shm>(registry, name, version);
        }
        else if (interface_name<xdg_wm_base> == interface) {
            shell = registry_bind<xdg_wm_base>(registry, name, version);
        }
    });
    registry.on<&wl_registry_listener::global_remove>([&](wl_registry*, std::uint32_t name) noexcept {
        std::erase_if(seats, [name](auto const& s) { return s.id() == name; });
    });
    wl_display_roundtrip(display);

    for (auto& seat : seats) {
        seat.on([&seat] -> fiblet<&wl_seat_listener::capabilities> {
            proxy<wl_keyboard> keyboard;
            proxy<wl_pointer> pointer;
            proxy<wl_touch> touch;
            for (;;) {
                [[maybe_unused]] auto const& [s, caps] = co_await wait_current_args{};
                if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
                    if (!keyboard) {
                        keyboard = proxy{wl_seat_get_keyboard(seat)};
                    }
                    keyboard.on([] -> fiblet<&wl_keyboard_listener::key> {
                        for (;;) {
                            [[maybe_unused]] auto const& args = co_await wait_current_args{};
                            std::cout << "key: " << args << std::endl;
                        }
                    });
                    keyboard.on([] -> fiblet<&wl_keyboard_listener::modifiers> {
                        for (;;) {
                            [[maybe_unused]] auto const& args = co_await wait_current_args{};
                            std::cout << "key mod: " << args << std::endl;
                        }
                    });
                    keyboard.on([] -> fiblet<&wl_keyboard_listener::repeat_info> {
                        for (;;) {
                            [[maybe_unused]] auto const& args = co_await wait_current_args{};
                            std::cout << "key repeat: " << args << std::endl;
                        }
                    });
                }
                else {
                    keyboard = {};
                }
                if (caps & WL_SEAT_CAPABILITY_POINTER) {
                    if (!pointer) {
                        pointer = proxy{wl_seat_get_pointer(seat)};
                    }
                    pointer.on([] -> fiblet<&wl_pointer_listener::axis_value120> {
                        for (;;) {
                            [[maybe_unused]] auto const& args = co_await wait_current_args{};
                            std::cout << "axis120: " << args << std::endl;
                        }
                    });
                }
                else {
                    pointer = {};
                }
                if (caps & WL_SEAT_CAPABILITY_TOUCH) {
                    if (!touch) {
                        touch = proxy{wl_seat_get_touch(seat)};
                    }
                    touch.on([] -> fiblet<&wl_touch_listener::motion> {
                        for (;;) {
                            [[maybe_unused]] auto const& args = co_await wait_current_args{};
                            std::cout << "touch.motion: " << args << std::endl;
                        }
                    });
                }
                else {
                    touch = {};
                }
            }
        });
    }
    wl_display_roundtrip(display);

    MVLL_CHECK(compositor);
    MVLL_CHECK(shm);
    MVLL_CHECK(shell);
    shell.on<&xdg_wm_base_listener::ping>([](xdg_wm_base* shell, std::uint32_t serial) noexcept {
        xdg_wm_base_pong(shell, serial);
    });
    auto surface = proxy{wl_compositor_create_surface(compositor)};
    auto xsurface = proxy{xdg_wm_base_get_xdg_surface(shell, surface)};
    xsurface.on<&xdg_surface_listener::configure>([](xdg_surface* xsurface, std::uint32_t serial) noexcept {
        xdg_surface_ack_configure(xsurface, serial);
    });

    auto toplevel = proxy{xdg_surface_get_toplevel(xsurface)};
    toplevel.on([&] MVLL_NOEXCEPT -> fiblet<&xdg_toplevel_listener::configure> {
        std::size_t scale = 1;
        std::size_t cx = 640 * scale;
        std::size_t cy = 480 * scale;
        auto primary = shm_allocate_buffer(shm, cx, cy);
        auto secondary = shm_allocate_buffer(shm, cx, cy);
        auto release_callback = [&primary, &secondary](wl_buffer*) {
            std::swap(primary, secondary);
        };
        auto& primary_buffer = std::get<1>(primary);
        auto& secondary_buffer = std::get<1>(secondary);
        primary_buffer.on<&wl_buffer_listener::release>(release_callback);
        secondary_buffer.on<&wl_buffer_listener::release>(release_callback);
        auto frame = proxy{wl_surface_frame(surface)}; 
        // auto que = sycl::queue();
        // std::cout << que.get_device().get_info<sycl::info::device::name>() << std::endl;
        for (;;) {
            auto const& args = co_await wait_current_args{};
            auto const& [toplevel, h, w, states] = args;
            cx = h * scale;
            cy = w * scale;
            if (cx * cy > 0) {
                primary = shm_allocate_buffer(shm, cx, cy);
                secondary = shm_allocate_buffer(shm, cx, cy);
                primary_buffer.on<&wl_buffer_listener::release>(release_callback);
                secondary_buffer.on<&wl_buffer_listener::release>(release_callback);
            }
            else { // the initial configuration
                wl_surface_attach(surface, primary_buffer, 0, 0);
                wl_surface_commit(surface);
            }
            frame.on<&wl_callback_listener::done>([&](wl_callback*, std::uint32_t) MVLL_NOEXCEPT {
                frame.rebind(wl_surface_frame(surface));
                wl_surface_attach(surface, primary_buffer, 0, 0);
                wl_surface_damage(surface, 0, 0, cx, cy);
                wl_surface_commit(surface);
                wl_display_flush(display);
            });
        }
    });
    bool quit = false;
    toplevel.on<&xdg_toplevel_listener::close>([&](xdg_toplevel*) noexcept {
        quit = true;
    });
    xdg_toplevel_set_app_id(toplevel, std::filesystem::path(argv[0]).filename().c_str());

    wl_surface_commit(surface);
    while (-1 != wl_display_dispatch(display)) {
        if (quit) break;
    }
    return 0;
}

consteval int f() {
    MVLL_CHECK(true);
    return 42;
}

constexpr int i = f();
static_assert(i == 42);
