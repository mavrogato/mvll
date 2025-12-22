
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>

#include <mvll/platform/linux.hpp>

#include <wayland-client.h>

#include <xdg-shell-client.h>
#include <zwp-tablet-v2-client.h>

namespace mvll
{
    struct empty_type { };
    template <class> constexpr wl_interface const *const interface_ptr = nullptr;

    template <class T> concept client_like = (interface_ptr<T> != nullptr);

    template <client_like T> struct listener_type { using type = empty_type; };
#define INTERN_CLIENT_LIKE_CONCEPT(CLIENT, LISTENER)                             \
    template <> constexpr wl_interface const *const interface_ptr<CLIENT> = &CLIENT##_interface; \
    template <> struct listener_type<CLIENT> : LISTENER { };
    INTERN_CLIENT_LIKE_CONCEPT(wl_registry,           wl_registry_listener)
    INTERN_CLIENT_LIKE_CONCEPT(wl_compositor,         empty_type)
    INTERN_CLIENT_LIKE_CONCEPT(wl_output,             wl_output_listener)
    INTERN_CLIENT_LIKE_CONCEPT(wl_shm,                wl_shm_listener)
    INTERN_CLIENT_LIKE_CONCEPT(wl_seat,               wl_seat_listener)
    INTERN_CLIENT_LIKE_CONCEPT(wl_surface,            wl_surface_listener)
    INTERN_CLIENT_LIKE_CONCEPT(wl_shm_pool,           empty_type)
    INTERN_CLIENT_LIKE_CONCEPT(wl_buffer,             wl_buffer_listener)
    INTERN_CLIENT_LIKE_CONCEPT(wl_keyboard,           wl_keyboard_listener)
    INTERN_CLIENT_LIKE_CONCEPT(wl_pointer,            wl_pointer_listener)
    INTERN_CLIENT_LIKE_CONCEPT(wl_touch,              wl_touch_listener)
    INTERN_CLIENT_LIKE_CONCEPT(xdg_wm_base,           xdg_wm_base_listener)
    INTERN_CLIENT_LIKE_CONCEPT(xdg_surface,           xdg_surface_listener)
    INTERN_CLIENT_LIKE_CONCEPT(xdg_toplevel,          xdg_toplevel_listener)
    INTERN_CLIENT_LIKE_CONCEPT(zwp_tablet_manager_v2, empty_type)
    INTERN_CLIENT_LIKE_CONCEPT(zwp_tablet_seat_v2,    zwp_tablet_seat_v2_listener)
    INTERN_CLIENT_LIKE_CONCEPT(zwp_tablet_tool_v2,    zwp_tablet_tool_v2_listener)
#undef INTERN_CLIENT_LIKE_CONCEPT

    template <class T>
    concept client_like_with_listener = client_like<T> && !std::is_base_of_v<empty_type, listener_type<T>>;

    template <client_like T>
    void client_deleter(T* raw) noexcept {
        MVLL_CHECK(raw);
        wl_proxy_destroy(reinterpret_cast<wl_proxy*>(raw));
    }
    template <client_like T>
    auto make_unique(T* raw = nullptr) noexcept {
        return std::unique_ptr<T, decltype (client_deleter<T>)*>(raw, client_deleter);
    }
    template <client_like T>
    using unique_ptr_type = decltype (make_unique<T>());

    template <class> class wrapper;
    template <class T> wrapper(T*) -> wrapper<T>;

    template <client_like T>
    class wrapper<T> {
    public:
        wrapper(T* raw = nullptr) : ptr{make_unique(raw)}
            {
            }
        operator T*() const { return this->ptr.get(); }

    private:
        unique_ptr_type<T> ptr;
    };

    template <client_like_with_listener T>
    class wrapper<T> {
    private:
        static constexpr auto create_default_listener() {
            static constexpr auto N = sizeof (listener_type<T>) / sizeof (void*);
            return std::make_unique(listener_type<T>{
                []<size_t... I>(std::index_sequence<I...>) noexcept {
                    return listener_type<T> {
                        ([](void* data, auto...) noexcept {
                            (void) I;
                            auto self = reinterpret_cast<wrapper*>(data);
                        })...
                    };
                }(std::make_index_sequence<N>())});
        }

    public:
        wrapper(T* raw = nullptr) MVLL_NOEXCEPT
            : ptr{make_unique(raw)}
            , listener{create_default_listener()}
            {
                MVLL_CHECK(ptr != nullptr);
                MVLL_CHECK(0 != wl_proxy_add_listener(reinterpret_cast<wl_proxy*>(operator T*()),
                                                      reinterpret_cast<void(**)(void)>(this->listener.get()),
                                                      this));
            }
        operator T*() const { return this->ptr.get(); }
        listener_type<T>* operator->() const { return this->listener.get(); }

    private:
        unique_ptr_type<T> ptr;
        std::unique_ptr<listener_type<T>> listener;
    };

    template <client_like T>
    auto registry_bind(wl_registry* registry, uint32_t name, uint32_t version) noexcept {
        return static_cast<T*>(::wl_registry_bind(registry, name, interface_ptr<T>, version));
    }

    template <class T = std::uint32_t, wl_shm_format format = WL_SHM_FORMAT_ARGB8888, size_t bypp = 4>
    [[nodiscard]] inline auto shm_allocate_buffer(wl_shm* shm, size_t cx, size_t cy) MVLL_NOEXCEPT {
        std::string_view xdg_runtime_dir = std::getenv("XDG_RUNTIME_DIR");
        if (xdg_runtime_dir.empty() || !std::filesystem::exists(xdg_runtime_dir)) {
            MVLL_CHECK(!"No XDG_RUNTIME_DIR settings...");
        }
        std::string tmp_path(xdg_runtime_dir);
        tmp_path += "/weston-shared-XXXXXX";
        mvll::platform::unique_fd fd{::mkostemp(tmp_path.data(), O_CLOEXEC)};
        MVLL_CHECK(fd);
        MVLL_CHECK(0 <= ::unlink(tmp_path.c_str()));
        MVLL_CHECK(0 <= ::ftruncate(fd, bypp*cx*cy));
        mvll::platform::unique_mmap<T> data{nullptr, bypp*cx*cy, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0};
        auto pool = wrapper{wl_shm_create_pool(shm, fd, bypp*cx*cy)};
        auto buffer = wrapper{wl_shm_pool_create_buffer(pool, 0, cx, cy, bypp * cx, format)};
        return std::tuple{
            std::move(fd),
            std::move(buffer),
            std::move(data),
        };
    }
} // ::mvll
    
extern int display_run();

int main() {
    std::cout << "Hi" << std::endl;
    return display_run();
}
