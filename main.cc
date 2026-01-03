
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
            // auto await_transform(auto&& rec) requires std::is_invocable_v<decltype (rec)> {
            //     auto next = rec();
            //     auto h = next.handle;
            //     next.handle = next.handle.promise().previous;
            //     struct recursive_awaiter {
            //         std::coroutine_handle<> next_handle;
            //         bool await_ready() const noexcept { return false; }
            //         void await_resume() const noexcept {}
            //         std::coroutine_handle<> await_suspend(handle_type h) noexcept {
            //             return next_handle; //!!!
            //         }
            //     };
            //     return recursive_awaiter{h};
            // }
            template <class Awaitable>
            auto await_transform(Awaitable&& awaitable) noexcept {
                auto native_awaiter = get_awaiter(std::forward<Awaitable>(awaitable));
                struct warp_awaiter {
                    decltype (native_awaiter) inner;
                    std::coroutine_handle<> previous;
                    bool await_ready() noexcept(noexcept(inner.await_ready())) {
                        return inner.await_ready();
                    }
                    auto await_resume() noexcept(noexcept(inner.await_resume())) {
                        return inner.await_resume();
                    }
                    auto await_suspend(std::coroutine_handle<> h) noexcept{
                        using result_t = decltype (inner.await_suspend(h));
                        if constexpr (std::is_void_v<result_t>) {
                            inner.await_suspend(h);
                            return previous ? previous : std::noop_coroutine();
                        } else if constexpr (std::is_same_v<result_t, bool>) {
                            if (inner.await_suspend(h)) {
                                return previous ? previous : std::noop_coroutine();
                            }
                            return h;
                        }
                        return inner.await_suspend(h);
                    }
                };
                return warp_awaiter{ std::move(native_awaiter), this->previous };
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
        void plug(Func&& user_coro, Args&&... args) MVLL_NOEXCEPT {
            constexpr auto Member = get_member_v<Func>;
            static std::size_t ordinal = std::bit_cast<std::size_t>(Member) / sizeof (void*);
            MVLL_CHECK(!pin->slots[ordinal]);
            pin->slots[ordinal].reset(new fiblet<Member>{user_coro(std::forward<Args>(args)...)});
            MVLL_CHECK(pin->slots[ordinal]);
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

    template <class T = std::uint32_t, wl_shm_format format = WL_SHM_FORMAT_XRGB8888, size_t bypp = 4>
    [[nodiscard]] inline auto shm_allocate_buffer(wl_shm* shm, size_t cx, size_t cy) MVLL_NOEXCEPT {
        mvll::platform::unique_fd fd{::memfd_create("mvll-shm", MFD_CLOEXEC)};
        MVLL_CHECK(0 <= ::ftruncate(fd, bypp*cx*cy));
        mvll::platform::unique_mmap<T> data{nullptr, bypp*cx*cy, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0};
        auto pool = proxy{wl_shm_create_pool(shm, fd, bypp*cx*cy)};
        auto buffer = proxy{wl_shm_pool_create_buffer(pool.get(), 0, cx, cy, bypp * cx, format)};
        return std::tuple{std::move(fd), std::move(buffer), std::move(data)};
    }

    inline auto lamed(auto&& closure) noexcept {
        static auto cache = closure;
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
    registry.plug([&] MVLL_NOEXCEPT -> fiblet<&wl_registry_listener::global> {
        for (;;) {
            auto const& [registry, name, interface, version] = co_await wait_current_args{};
            if (interface_name<wl_compositor> == interface) {
                compositor.emplace(registry_bind<wl_compositor>(registry, name, version));
            }
            else if (interface_name<wl_seat> == interface) {
                seats.emplace_front(registry_bind<wl_seat>(registry, name, version));
            }
            else if (interface_name<wl_shm> == interface) {
                shm.emplace(registry_bind<wl_shm>(registry, name, version));
            }
            else if (interface_name<xdg_wm_base> == interface) {
                shell.emplace(registry_bind<xdg_wm_base>(registry, name, version));
            }
        }
    });
    registry.plug([&] MVLL_NOEXCEPT -> fiblet<&wl_registry_listener::global_remove> {
        for (;;) {
            auto const& [registry, name] = co_await wait_current_args{};
            std::erase_if(seats, [name](auto const& s) {
                return s.id() == name;
            });
        }
    });
    wl_display_roundtrip(display.get());
    for (auto& seat : seats) {
        seat.plug([&seat] MVLL_NOEXCEPT -> fiblet<&wl_seat_listener::capabilities> {
            std::optional<proxy<wl_keyboard>> keyboard;
            std::optional<proxy<wl_pointer>> pointer;
            std::optional<proxy<wl_touch>> touch;
            for (;;) {
                [[maybe_unused]] auto const& [s, caps] = co_await wait_current_args{};
                if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
                    keyboard.emplace(proxy{wl_seat_get_keyboard(seat.get())});
                    keyboard->plug([] MVLL_NOEXCEPT -> fiblet<&wl_keyboard_listener::key> {
                        for (;;) {
                            [[maybe_unused]] auto const& args = co_await wait_current_args{};
                            std::cout << args << std::endl;
                        }
                    });
                }
                else {
                    keyboard.reset();
                }
                if (caps & WL_SEAT_CAPABILITY_POINTER) {
                    pointer.emplace(proxy{wl_seat_get_pointer(seat.get())});
                    pointer->plug([]  MVLL_NOEXCEPT -> fiblet<&wl_pointer_listener::axis_value120> {
                        for (;;) {
                            [[maybe_unused]] auto const& args = co_await wait_current_args{};
                            std::cout << args << std::endl;
                        }
                    });
                }
                else {
                    pointer.reset();
                }
                if (caps & WL_SEAT_CAPABILITY_TOUCH) {
                    touch = proxy{wl_seat_get_touch(seat.get())};
                    touch->plug([] MVLL_NOEXCEPT -> fiblet<&wl_touch_listener::motion> {
                        for (;;) {
                            [[maybe_unused]] auto const& args = co_await wait_current_args{};
                            std::cout << args << std::endl;
                        }
                    });
                }
                else {
                    pointer.reset();
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

    std::size_t scale = 1;
    std::size_t cx = 640 * scale;
    std::size_t cy = 480 * scale;
    auto [fd, buffer, pixels] = shm_allocate_buffer(shm.value().get(), cx, cy);
    auto toplevel = proxy{xdg_surface_get_toplevel(xsurface.get())};
    xdg_toplevel_set_app_id(toplevel.get(), std::filesystem::path(argv[0]).filename().c_str());
    toplevel.plug([&] MVLL_NOEXCEPT -> fiblet<&xdg_toplevel_listener::configure> {
        for (;;) {
            auto const& args = co_await wait_current_args{};
            std::cout << "toplevel.configure: " << args << std::endl;
            auto const& [toplevel, h, w, states] = args;
            cx = h * scale;
            cy = w * scale;
            if (cx * cy > 0) {
                std::tie(fd, buffer, pixels) = shm_allocate_buffer(shm.value().get(), cx, cy);
                buffer.plug([] MVLL_NOEXCEPT -> fiblet<&wl_buffer_listener::release> {
                    for (;;) {
                        auto const& args = co_await wait_current_args{};
                        std::cout << "buffer.release: " << args << std::endl;
                    }
                });
            }
        }
    });
    bool quit = false;
    toplevel.plug([&] MVLL_NOEXCEPT -> fiblet<&xdg_toplevel_listener::close> {
        co_await wait_current_args{};
        quit = true;
    });

    auto que = sycl::queue();
    std::cout << que.get_device().get_info<sycl::info::device::name>() << std::endl;
    auto callback = proxy{wl_surface_frame(surface.get())};
    callback.plug([&] MVLL_NOEXCEPT -> fiblet<&wl_callback_listener::done> {
        for (;;) {
            auto const& args = co_await wait_current_args{};
            std::cout << "outer: " << args << std::endl;
            wl_surface_attach(surface.get(), buffer.get(), 0, 0);
            wl_surface_damage(surface.get(), 0, 0, cx, cy);
            wl_surface_commit(surface.get());
            auto ret = wl_display_flush(display.get());
            std::cout << ret << std::endl;
            auto callback = proxy{wl_surface_frame(surface.get())};
            callback.plug([&] MVLL_NOEXCEPT -> fiblet<&wl_callback_listener::done> {
                std::cout << "inner ready" << std::endl;
                co_await wait_current_args{};
                std::cout << "inner: " << args << std::endl;
                wl_surface_attach(surface.get(), buffer.get(), 0, 0);
                wl_surface_damage(surface.get(), 0, 0, cx, cy);
                wl_surface_commit(surface.get());
                wl_display_flush(display.get());
            });
        }
    });

    wl_surface_attach(surface.get(), buffer.get(), 0, 0);
    wl_surface_damage(surface.get(), 0, 0, cx, cy);
    wl_surface_commit(surface.get());
    while (-1 != wl_display_dispatch(display.get())) {
        if (quit) break;
    }
    return 0;
}
