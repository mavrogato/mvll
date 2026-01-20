
#include "mvll/error-handling.hpp"

#include <array>
#include <coroutine>
#include <filesystem>
#include <forward_list>
#include <tuple>
#include <type_traits>

#include <mvll/cpp2x/generator.hpp>
#include <mvll/cpp2x/tuple-support.hpp>
#include <mvll/fiblet.hpp>
#include <mvll/platform/linux.hpp>
#include <mvll/unique.hpp>
#include <mvll/versor.hpp>

#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <xdg-shell-client.h>
#include <zwp-tablet-v2-client.h>
#include <mvll/wayland/client/proxy-pre.hpp>
#define MVLL_PROXY_LIST(V)                                              \
    V(xdg_wm_base,           PROXY_ATTR_HAS_LISTENER)                   \
    V(xdg_surface,           PROXY_ATTR_HAS_LISTENER)                   \
    V(xdg_toplevel,          PROXY_ATTR_HAS_LISTENER)                   \
    V(zwp_tablet_manager_v2, PROXY_ATTR_NONE)                           \
    V(zwp_tablet_seat_v2,    PROXY_ATTR_HAS_LISTENER)                   \
    V(zwp_tablet_tool_v2,    PROXY_ATTR_HAS_LISTENER)
#include <mvll/wayland/client/proxy-meta.hpp>


namespace mvll::inline wayland::inline client
{
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
                if constexpr (DELETER != nullptr) {
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
    struct action_invocable_traits {
        static inline constexpr bool value = []<class... Args>(std::tuple<Args...>*) {
            return std::is_invocable_v<Func, Args...>;
        }((Tuple*)nullptr);
    };
    template <class Func, class Tuple>
    inline constexpr bool action_invocable_traits_v = action_invocable_traits<Func, Tuple>::value;

    template <is_proxy T>
    class proxy_impl : public internals::move_only_pointer<T, delete_proxy<T>> {
    public:
        using base_type = internals::move_only_pointer<T, delete_proxy<T>>;
        static inline constexpr auto metainfo = metadb[static_cast<std::size_t>(identifier<T>)];
        static inline constexpr auto interface_ptr = mvll::interface_ptr<T>;
        static inline constexpr auto interface_name = mvll::interface_name<T>;

    public:
        using base_type::base_type;

    public:
        std::uint32_t id() const noexcept {
            return wl_proxy_get_id(reinterpret_cast<wl_proxy*>(this->get()));
        }

    public:
        template <class Ch, class Tr>
        friend std::basic_ostream<Ch, Tr>& operator<<(std::basic_ostream<Ch, Tr>& output,
                                                      proxy_impl const& x) {
            return output << std::tuple{interface_name, x.id(), x.get()};
        }
    };

    template <is_proxy_observable T>
    class thunk_table final {
    public:
        static inline constexpr std::size_t SIZE = sizeof (listener_type<T>) / sizeof (void*);
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
        std::int32_t start(T* target) noexcept {
            return wl_proxy_add_listener(
                reinterpret_cast<wl_proxy*>(target),
                reinterpret_cast<void(**)(void)>(&listener),
                this->table_.get());
        }

        template <auto Member> //!!!
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
        proxy()
            : proxy_impl<T>::proxy_impl{}
            , table_{}
            {
            }
        proxy(T* raw)
            : proxy_impl<T>::proxy_impl{raw}
            , table_{}
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
        template <class> struct fiblet_traits;
        template <auto Member> requires std::is_same_v<listener_type<T>,
                                                       typename event_traits<Member>::listener_type>
        struct fiblet_traits<listener_fiblet<Member>> {
            static constexpr auto member = Member;
        };
        template <class Func>
        static inline constexpr auto get_member_v = fiblet_traits<std::invoke_result_t<Func>>::member;
        template <class Func, class... InitialArgs>
        void on(Func&& coro, InitialArgs&&... init) {
            constexpr auto Member = get_member_v<Func>;
            table_.template add<Member>(new listener_fiblet<Member>{coro(std::forward<InitialArgs>(init)...)});
        }
        template <auto Member, class Func>
        void on(Func&& func) {
            using DecayFunc = std::decay_t<Func>;
            table_.template add<Member>(new listener_action<Member, DecayFunc>{std::forward<DecayFunc>(func)});
        }

    private:
        thunk_table<T> table_;
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

} // namespace mvll::inline wayland::inline client

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
}

#include <iostream>

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
        seat.on([&seat] -> listener_fiblet<&wl_seat_listener::capabilities> {
            proxy<wl_keyboard> keyboard;
            proxy<wl_pointer> pointer;
            proxy<wl_touch> touch;
            for (;;) {
                [[maybe_unused]] auto const& [s, caps] = co_await wait_current;
                if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
                    if (!keyboard) {
                        keyboard = proxy{wl_seat_get_keyboard(seat)};
                    }
                    keyboard.on([] -> listener_fiblet<&wl_keyboard_listener::key> {
                        for (;;) {
                            [[maybe_unused]] auto const& args = co_await wait_current;
                        }
                    });
                    keyboard.on([] -> listener_fiblet<&wl_keyboard_listener::modifiers> {
                        for (;;) {
                            [[maybe_unused]] auto const& args = co_await wait_current;
                        }
                    });
                    keyboard.on([] -> listener_fiblet<&wl_keyboard_listener::repeat_info> {
                        for (;;) {
                            [[maybe_unused]] auto const& args = co_await wait_current;
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
                    pointer.on([] -> listener_fiblet<&wl_pointer_listener::axis_value120> {
                        for (;;) {
                            [[maybe_unused]] auto const& args = co_await wait_current;
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
                    struct stroke {
                        std::int32_t id;
                        fiblet<versor<wl_fixed_t, 2>> coro;
                    };
                    std::forward_list<stroke> strokes;
                    touch.on<&wl_touch_listener::down>([&](wl_touch*,
                                                           std::uint32_t,
                                                           std::uint32_t,
                                                           wl_surface*,
                                                           std::int32_t id,
                                                           wl_fixed_t x,
                                                           wl_fixed_t y) noexcept {
                        strokes.emplace_front(stroke {
                                id,
                                [&]() -> fiblet<versor<wl_fixed_t, 2>> {
                                    for (;;) {
                                        auto ret = co_yield nullptr;
                                        std::cout << ret << std::endl;
                                    }
                                }(),
                            });
                        std::cout << "start stroke #" << id << std::endl;
                        versor<wl_fixed_t, 2> cur{x, y};
                        strokes.front().coro.push(&cur);
                    });
                    touch.on<&wl_touch_listener::up>([&](wl_touch*,
                                                         std::uint32_t,
                                                         std::uint32_t,
                                                         std::int32_t id) noexcept {
                        std::erase_if(strokes, [id](auto const& s) { return s.id == id; });
                        std::cout << "remove stroke #" << id << std::endl;
                    });
                    // touch.on<&wl_touch_listener::motion>([&](wl_touch*,
                    //                                          std::uint32_t,
                    //                                          std::int32_t id,
                    //                                          wl_fixed_t x,
                    //                                          wl_fixed_t y) noexcept {
                    // });
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
    toplevel.on([&] MVLL_NOEXCEPT -> listener_fiblet<&xdg_toplevel_listener::configure> {
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
            auto const& args = co_await wait_current;
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
                wl_surface_damage_buffer(surface, 0, 0, cx, cy);
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
