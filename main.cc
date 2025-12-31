
#include "mvll/error-handling.hpp"
#include <array>
#include <bit>
#include <filesystem>
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
#include <mvll/channel.hpp>

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

    struct fiblet_bridge {
        virtual ~fiblet_bridge() noexcept = default;
        virtual void resume() = 0;
    };

    template <class> class proxy;
    template <class T> proxy(T*) -> proxy<T>;
    template <>
    class proxy<wl_display> {
    public:
        proxy(wl_display* raw) MVLL_NOEXCEPT : ptr{raw, &wl_display_disconnect} {}
        wl_display* get() const MVLL_NOEXCEPT { return this->ptr.get(); }
        operator wl_display*() const MVLL_NOEXCEPT { return this->get(); }

    private:
        std::unique_ptr<wl_display, std::decay_t<decltype (wl_display_disconnect)>> ptr;
    };
    template <is_proxy T>
    class proxy<T> {
    public:
        proxy(T* raw) MVLL_NOEXCEPT : ptr{make_unique(raw)} {}
        T* get() const MVLL_NOEXCEPT { return this->ptr.get(); }
        operator T*() const { return this->get(); }

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
            return std::make_unique<listener_type<T>>(
                []<size_t... I>(std::index_sequence<I...>) MVLL_NOEXCEPT {
                    return listener_type<T> {
                        ([]<class ...Rest>(void* data, Rest... rest) MVLL_NOEXCEPT {
                            auto self = reinterpret_cast<proxy*>(data);
                            if (auto& bridge = self->slots[I]) {
                                auto rest_args = std::tuple{rest...};
                                self->latest_args_raw = &rest_args;
                                bridge->resume();
                            }
                        })...
                    };
                }(std::make_index_sequence<SLOT_SIZE>()));
        }

    public:
        proxy(T* raw) MVLL_NOEXCEPT
            : ptr{make_unique(raw)}
            , listener{create_default_listener()}
            , slots{}
            , latest_args_raw{}
            {
                MVLL_CHECK(ptr != nullptr);
                MVLL_CHECK(-1 != wl_proxy_add_listener(reinterpret_cast<wl_proxy*>(operator T*()),
                                                       reinterpret_cast<void(**)(void)>(this->listener.get()),
                                                       this));
            }
        T* get() const { return this->ptr.get(); }
        operator T*() const MVLL_NOEXCEPT { return this->get(); }
        listener_type<T>* operator->() const MVLL_NOEXCEPT { return this->listener.get(); }

        template <auto Member>
        struct fiblet_channel : fiblet_bridge {
            channel<rest_args_tuple<Member>> ch;
            fiblet_channel(channel<rest_args_tuple<Member>>&& c) : ch{std::move(c)} {}
            void resume() override {
                if (auto handle = ch.handle; handle && !handle.done()) {
                    handle.resume();
                }
            }
        };

        template <auto Member, class Func>
        void open_channel(Func&& user_coro) MVLL_NOEXCEPT {
            static std::size_t ordinal = std::bit_cast<std::size_t>(Member) / sizeof (void*);
            MVLL_CHECK(!this->slots[ordinal]);
            auto bridge_coro = [](proxy* self, [[maybe_unused]] auto&& user_coro) MVLL_NOEXCEPT
                ->  channel<rest_args_tuple<Member>> {
                [[maybe_unused]] rest_args_tuple<Member> rest_args{};
                //auto user_ch = user_coro();
                for (;;) {
                    std::cout << "Begin: " << std::endl;
                    [[maybe_unused]] auto ret = co_yield (reinterpret_cast<rest_args_tuple<Member>*>(self->latest_args_raw));
                    //std::cout << *ret << std::endl;
                    std::cout << *reinterpret_cast<rest_args_tuple<Member>*>(self->latest_args_raw) << std::endl;
                }
            };
            auto bridge_ch = bridge_coro(this, std::move(user_coro));
            slots[ordinal].reset(new fiblet_channel<Member>{std::move(bridge_ch)});
            slots[ordinal]->resume();
        }

    private:
        unique_ptr_type<T> ptr;
        std::unique_ptr<listener_type<T>> listener;
        std::array<std::unique_ptr<fiblet_bridge>, SLOT_SIZE> slots{};
        void* latest_args_raw{};
    };

    template <is_proxy T>
    auto registry_bind(wl_registry* registry, uint32_t name, uint32_t version) MVLL_NOEXCEPT {
        return proxy{static_cast<T*>(::wl_registry_bind(registry, name, interface_ptr<T>, version))};
    }

    template <class T = std::uint32_t, wl_shm_format format = WL_SHM_FORMAT_XRGB8888, size_t bypp = 4>
    [[nodiscard]] inline auto shm_allocate_buffer(wl_shm* shm, size_t cx, size_t cy) MVLL_NOEXCEPT {
        std::string_view xdg_runtime_dir = std::getenv("XDG_RUNTIME_DIR");
        MVLL_CHECK(!xdg_runtime_dir.empty());
        MVLL_CHECK(std::filesystem::exists(xdg_runtime_dir));
        std::string tmp_path(xdg_runtime_dir);
        tmp_path += "/weston-shared-XXXXXX";
        mvll::platform::unique_fd fd{::mkostemp(tmp_path.data(), O_CLOEXEC)};
        MVLL_CHECK(fd);
        MVLL_CHECK(0 <= ::unlink(tmp_path.c_str()));
        MVLL_CHECK(0 <= ::ftruncate(fd, bypp*cx*cy));
        mvll::platform::unique_mmap<T> data{nullptr, bypp*cx*cy, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0};
        auto pool = proxy{wl_shm_create_pool(shm, fd, bypp*cx*cy)};
        auto buffer = proxy{wl_shm_pool_create_buffer(pool, 0, cx, cy, bypp * cx, format)};
        return std::tuple{std::move(fd), std::move(buffer), std::move(data)};
    }
} // ::mvll

int main() {
    using namespace mvll;
    auto display = proxy{wl_display_connect(nullptr)};
    auto registry = proxy{wl_display_get_registry(display)};

    registry.open_channel<&wl_registry_listener::global>([]{});
    wl_display_roundtrip(display);
    return 0;
}

#if 0
int main(int, char** argv) {
    using namespace mvll;
    auto display = wrapper{wl_display_connect(nullptr)};
    auto registry = wrapper{wl_display_get_registry(display)};
    std::optional<wrapper<wl_compositor>> compositor;
    std::optional<wrapper<wl_seat>> seat;
    std::optional<wrapper<wl_shm>> shm;
    std::optional<wrapper<xdg_wm_base>> shell;
    registry.fiblet_start<&wl_registry_listener::global>
        ([&](auto& rest_args) MVLL_NOEXCEPT -> generator<bool> {
            auto const& [registry, name, interface, version] = rest_args;
            for (;;) {
                co_yield true;
                if (interface_name<wl_compositor> == interface) {
                    compositor.emplace(registry_bind<wl_compositor>(registry, name, version));
                }
                else if (interface_name<wl_seat> == interface) {
                    seat.emplace(registry_bind<wl_seat>(registry, name, version));
                }
                else if (interface_name<wl_shm> == interface) {
                    shm.emplace(registry_bind<wl_shm>(registry, name, version));
                }
                else if (interface_name<xdg_wm_base> == interface) {
                    shell.emplace(registry_bind<xdg_wm_base>(registry, name, version));
                }
            }
        });
    registry.fiblet_start<&wl_registry_listener::global_remove>
        ([&](auto& rest_args) MVLL_NOEXCEPT -> generator<bool> {
            auto const& [registry, name] = rest_args;
            for (;;) {
                co_yield true;
                if (seat.has_value() && name == wl_proxy_get_id(reinterpret_cast<wl_proxy*>(seat.value().get()))) {
                    seat.reset();
                }
            }
        });
    wl_display_roundtrip(display);

    if (seat.has_value()) {
        wl_display_roundtrip(display);
    }
    MVLL_CHECK(seat.has_value());
    auto pointer = wrapper{wl_seat_get_pointer(seat.value())};
    pointer.fiblet_start<&wl_pointer_listener::frame>
        ([](auto&) -> generator<bool> {
            for(;;) co_yield true;
        });
    pointer.fiblet_start<&wl_pointer_listener::axis_value120>
        ([](auto& rest_args) MVLL_NOEXCEPT -> generator<bool> {
            for (;;) {
                co_yield true;
                std::cout << rest_args << std::endl;
            }
        });
    MVLL_CHECK(compositor.has_value());
    MVLL_CHECK(shm.has_value());
    MVLL_CHECK(shell.has_value());
    shell.value()->ping = [](auto, auto shell, auto serial) noexcept {
        xdg_wm_base_pong(shell, serial);
    };
    auto surface = wrapper{wl_compositor_create_surface(compositor.value())};
    auto xsurface = wrapper{xdg_wm_base_get_xdg_surface(shell.value(), surface)};
    xsurface->configure = [](auto, auto xsurface, auto serial) noexcept {
        xdg_surface_ack_configure(xsurface, serial);
    };

    std::size_t scale = 1;
    std::size_t cx = 640 * scale;
    std::size_t cy = 480 * scale;
    auto [fd, buffer, pixels] = shm_allocate_buffer(shm.value(), cx, cy);
    auto toplevel = wrapper{xdg_surface_get_toplevel(xsurface)};
    xdg_toplevel_set_app_id(toplevel, std::filesystem::path(argv[0]).filename().c_str());
    toplevel.fiblet_start<&xdg_toplevel_listener::configure>
        ([&](auto& rest_args) MVLL_NOEXCEPT-> generator<bool> {
            auto const& [toplevel, h, w, states] = rest_args;
            for (;;) {
                co_yield true;
                cx = h * scale;
                cy = w * scale;
                if (cx * cy > 0) {
                    std::tie(fd, buffer, pixels) = shm_allocate_buffer(shm.value(), cx, cy);
                }
            }
        });
    bool quit = false;
    toplevel.fiblet_start<&xdg_toplevel_listener::close>
        ([&]([[maybe_unused]] auto& rest_args) MVLL_NOEXCEPT -> generator<bool> {
            co_yield true;
            quit = true;
            co_return;
        });

    // auto que = sycl::queue();
    // std::cout << que.get_device().get_info<sycl::info::device::name>() << std::endl;

    wl_surface_commit(surface);
    while (-1 != wl_display_dispatch(display)) {
        if (quit) break;
        wl_surface_damage(surface, 0, 0, cx, cy);
        wl_surface_attach(surface, buffer, 0, 0);
        wl_surface_commit(surface);
        wl_display_flush(display);
    }

    return 0;
}
#endif
