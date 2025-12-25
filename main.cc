
#include "mvll/error-handling.hpp"
#include <array>
#include <bit>
#include <coroutine>
#include <filesystem>
#include <iostream>
#include <memory>
#include <tuple>
#include <type_traits>

#include <mvll/cpp2x/generator.hpp>
#include <mvll/cpp2x/tuple-support.hpp>
#include <mvll/platform/linux.hpp>
#include <mvll/unique.hpp>

#include <wayland-client-core.h>
#include <wayland-client.h>

#include <xdg-shell-client.h>
#include <zwp-tablet-v2-client.h>

namespace mvll
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

    template <auto Member, class Listener, class Ref>
    concept is_compatible_signature = std::is_member_pointer_v<decltype (Member)>
        && std::is_same_v<typename member_pointer_traits<decltype (Member)>::class_type, Listener>
        && std::is_same_v<typename function_traits<
                              typename member_pointer_traits<decltype (Member)>::member_type
                              >::rest_args_tuple,
                          std::remove_cvref_t<Ref>>;

    struct empty_type { };
    template <class> constexpr wl_interface const *const interface_ptr = nullptr;
    template <class T> concept client_proxy = (interface_ptr<T> != nullptr);
    template <client_proxy T> struct listener_type_holder { using type = empty_type; };
#define INTERN_CLIENT_PROXY_CONCEPT(CLIENT, LISTENER)                             \
    template <> constexpr wl_interface const *const interface_ptr<CLIENT> = &CLIENT##_interface; \
    template <> struct listener_type_holder<CLIENT> { using type = LISTENER; };
    INTERN_CLIENT_PROXY_CONCEPT(wl_registry,           wl_registry_listener)
    INTERN_CLIENT_PROXY_CONCEPT(wl_compositor,         empty_type)
    INTERN_CLIENT_PROXY_CONCEPT(wl_output,             wl_output_listener)
    INTERN_CLIENT_PROXY_CONCEPT(wl_shm,                wl_shm_listener)
    INTERN_CLIENT_PROXY_CONCEPT(wl_seat,               wl_seat_listener)
    INTERN_CLIENT_PROXY_CONCEPT(wl_surface,            wl_surface_listener)
    INTERN_CLIENT_PROXY_CONCEPT(wl_shm_pool,           empty_type)
    INTERN_CLIENT_PROXY_CONCEPT(wl_buffer,             wl_buffer_listener)
    INTERN_CLIENT_PROXY_CONCEPT(wl_keyboard,           wl_keyboard_listener)
    INTERN_CLIENT_PROXY_CONCEPT(wl_pointer,            wl_pointer_listener)
    INTERN_CLIENT_PROXY_CONCEPT(wl_touch,              wl_touch_listener)
    INTERN_CLIENT_PROXY_CONCEPT(xdg_wm_base,           xdg_wm_base_listener)
    INTERN_CLIENT_PROXY_CONCEPT(xdg_surface,           xdg_surface_listener)
    INTERN_CLIENT_PROXY_CONCEPT(xdg_toplevel,          xdg_toplevel_listener)
    INTERN_CLIENT_PROXY_CONCEPT(zwp_tablet_manager_v2, empty_type)
    INTERN_CLIENT_PROXY_CONCEPT(zwp_tablet_seat_v2,    zwp_tablet_seat_v2_listener)
    INTERN_CLIENT_PROXY_CONCEPT(zwp_tablet_tool_v2,    zwp_tablet_tool_v2_listener)
#undef INTERN_CLIENT_PROXY_CONCEPT
    template <client_proxy T> using listener_type = listener_type_holder<T>::type;
    template <class T>
    concept client_proxy_with_listener = client_proxy<T> && !std::is_same_v<empty_type, listener_type<T>>;

    template <client_proxy T>
    void client_deleter(T* raw) noexcept {
        MVLL_CHECK(raw);
        wl_proxy_destroy(reinterpret_cast<wl_proxy*>(raw));
    }
    template <client_proxy T>
    auto make_unique(T* raw) MVLL_NOEXCEPT {
        MVLL_CHECK(raw);
        return std::unique_ptr<T, std::decay_t<decltype (client_deleter<T>)>>(raw, client_deleter);
    }
    template <client_proxy T>
    using unique_ptr_type = decltype (make_unique<T>(std::declval<T*>()));

    struct fiblet_bridge {
        virtual ~fiblet_bridge() noexcept = default;
        virtual void resume(void const* rest_args_ptr) MVLL_NOEXCEPT = 0;
    };
    template <class Gen>
    struct fiblet : fiblet_bridge {
        Gen gen;
        Gen::iterator iter;
        fiblet(Gen&& g) MVLL_NOEXCEPT : gen{std::move(g)}, iter{gen.begin()} {}
        void resume(void const* rest_args_ptr) MVLL_NOEXCEPT override {
            using rest_args_tuple = std::remove_cvref_t<decltype(*iter)>;
            *iter = *static_cast<rest_args_tuple const*>(rest_args_ptr);
            ++iter;
        }
    };

    template <class> class wrapper;
    template <class T> wrapper(T*) -> wrapper<T>;
    template <>
    class wrapper<wl_display> {
    public:
        wrapper(wl_display* raw) MVLL_NOEXCEPT : ptr{raw, &wl_display_disconnect} {}
        operator wl_display*() const { return this->ptr.get(); }

    private:
        std::unique_ptr<wl_display, std::decay_t<decltype (wl_display_disconnect)>> ptr;
    };
    template <client_proxy T>
    class wrapper<T> {
    public:
        wrapper(T* raw) MVLL_NOEXCEPT : ptr{make_unique(raw)} {}
        operator T*() const { return this->ptr.get(); }

    private:
        unique_ptr_type<T> ptr;
    };
    template <client_proxy_with_listener T>
    class wrapper<T> {
    private:
        static constexpr std::size_t SLOT_SIZE = sizeof (listener_type<T>) / sizeof (void*);
        static constexpr auto create_default_listener() MVLL_NOEXCEPT {
            return std::make_unique<listener_type<T>>(
                []<size_t... I>(std::index_sequence<I...>) MVLL_NOEXCEPT {
                    return listener_type<T> {
                        ([](void* data, auto... rest) MVLL_NOEXCEPT {
                            auto self = reinterpret_cast<wrapper*>(data);
                            if (auto& bridge = self->slots[I]) {
                                auto rest_args = std::tuple{rest...};
                                bridge->resume(&rest_args);
                            }
                        })...
                    };
                }(std::make_index_sequence<SLOT_SIZE>()));
        }

    public:
        wrapper(T* raw) MVLL_NOEXCEPT
            : ptr{make_unique(raw)}
            , listener{create_default_listener()}
            , slots{}
            {
                MVLL_CHECK(ptr != nullptr);
                MVLL_CHECK(-1 != wl_proxy_add_listener(reinterpret_cast<wl_proxy*>(operator T*()),
                                                       reinterpret_cast<void(**)(void)>(this->listener.get()),
                                                       this));
            }
        operator T*() const { return this->ptr.get(); }
        listener_type<T>* operator->() const { return this->listener.get(); }

        template <auto Member> requires std::is_same_v<
            typename member_pointer_traits<decltype (Member)>::class_type,
            listener_type<T>>
        using rest_args_tuple = typename function_traits<
            typename member_pointer_traits<decltype (Member)>::member_type>::rest_args_tuple;
        template <auto Member, class Func>
        auto fiblet_start(Func&& user_coro) MVLL_NOEXCEPT {
            std::size_t ordinal = std::bit_cast<std::size_t>(Member) / sizeof (void*);
            auto bridge_coro = [user_coro = std::move(user_coro)]()
                -> mvll::cpp2x::generator<rest_args_tuple<Member>&>
                {
                    rest_args_tuple<Member> rest_args{};
                    auto user_gen = user_coro(rest_args);
                    auto user_iter = user_gen.begin();
                    for (;;) {
                        co_yield rest_args;
                        if (user_iter != user_gen.end()) {
                            ++user_iter;
                        }
                    }
                };
            auto bridge_gen = bridge_coro();
            this->slots[ordinal].reset(new fiblet(std::move(bridge_gen)));
        }

    private:
        unique_ptr_type<T> ptr;
        std::unique_ptr<listener_type<T>> listener;
        std::array<std::unique_ptr<fiblet_bridge>, SLOT_SIZE> slots{};
    };

    template <client_proxy T>
    auto registry_bind(wl_registry* registry, uint32_t name, uint32_t version) MVLL_NOEXCEPT {
        return MVLL_CHECK(static_cast<T*>(::wl_registry_bind(registry, name, interface_ptr<T>, version)));
    }

    template <class T = std::uint32_t, wl_shm_format format = WL_SHM_FORMAT_ARGB8888, size_t bypp = 4>
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
        auto pool = wrapper{wl_shm_create_pool(shm, fd, bypp*cx*cy)};
        auto buffer = wrapper{wl_shm_pool_create_buffer(pool, 0, cx, cy, bypp * cx, format)};
        return std::tuple{std::move(fd), std::move(buffer), std::move(data)};
    }
} // ::mvll

int main() {
    using mvll::operator<<;
    auto display = mvll::wrapper{wl_display_connect(nullptr)};
    auto registry = mvll::wrapper{wl_display_get_registry(display)};
    registry.fiblet_start<&wl_registry_listener::global>([](auto& rest_args) -> mvll::generator<bool> {
        for (;;) {
            co_yield true;
            std::cout << rest_args << std::endl;
        }
    });
    wl_display_roundtrip(display);
    return 0;
}
