#ifndef INCLUDE_MVLL_MEMORY_HPP
#define INCLUDE_MVLL_MEMORY_HPP

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>


namespace mvll::inline memory
{
    template <class T>
    concept is_boxable_type = std::destructible<std::decay_t<T>>
        && (!std::is_array_v<std::remove_reference_t<T>>)
        && std::is_nothrow_destructible_v<std::decay_t<T>>
        && std::move_constructible<std::decay_t<T>>
        && std::is_nothrow_move_constructible_v<std::decay_t<T>>
        && std::is_nothrow_constructible_v<std::decay_t<T>, T>;

    template <template <class> class AllocTemplate = std::allocator> // T.B.D.
    struct move_only_erased_box {
        void* chunk = nullptr;
        void (*dtor)(void*) noexcept = nullptr;

        move_only_erased_box(move_only_erased_box const&) = delete;
        move_only_erased_box& operator=(move_only_erased_box const&) = delete;

        constexpr move_only_erased_box() noexcept = default;
        template <class T>requires (!std::derived_from<std::decay_t<T>, move_only_erased_box<AllocTemplate>>
                                    && is_boxable_type<T>)
        constexpr explicit move_only_erased_box(T&& src) {
            this->emplace(std::forward<T>(src));
        }
        constexpr move_only_erased_box(move_only_erased_box&& other) noexcept
            : chunk{std::exchange(other.chunk, nullptr)}
            , dtor{std::exchange(other.dtor, nullptr)} {}
        constexpr move_only_erased_box& operator=(move_only_erased_box&& other) noexcept {
            if (this != &other) {
                cleanup();
                chunk = std::exchange(other.chunk, nullptr);
                dtor = std::exchange(other.dtor, nullptr);
            }
            return *this;
        }
        constexpr void cleanup() noexcept {
            if (chunk && dtor) {
                dtor(chunk);
            }
            chunk = nullptr;
            dtor = nullptr;
        }
        constexpr ~move_only_erased_box() noexcept {
            cleanup();
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return chunk != nullptr;
        }
        template <class T = void>
        [[nodiscard]] constexpr T const* get() const noexcept {
            return static_cast<T const*>(chunk);
        }
        template <class T = void>
        [[nodiscard]] constexpr T* get() noexcept {
            return static_cast<T*>(chunk);
        }
        template <class T>
        [[nodiscard]] constexpr explicit operator T const*() const noexcept {
            return get<T>();
        }
        template <class T>
        [[nodiscard]] constexpr explicit operator T*() noexcept {
            return get<T>();
        }

        template <is_boxable_type T>
        constexpr std::decay_t<T>& emplace(T&& src) {
            using Decayed = std::decay_t<T>;
            auto buf = AllocTemplate<Decayed>().allocate(1);
            cleanup();
            this->chunk = buf;
            this->dtor = [](void* raw) constexpr noexcept {
                Decayed* buf = static_cast<Decayed*>(raw);
                std::destroy_at(buf);
                AllocTemplate<Decayed>().deallocate(buf, 1);
            };
            return *std::construct_at(buf, std::forward<T>(src));
        }
    };
} // ::mvll::memory

#endif /*INCLUDE_MVLL_MEMORY_HPP*/
