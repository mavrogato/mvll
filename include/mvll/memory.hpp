#ifndef INCLUDE_MVLL_MEMORY_HPP
#define INCLUDE_MVLL_MEMORY_HPP

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>


namespace mvll::inline memory
{
    template <template <class> class AllocTemplate = std::allocator>
    struct unique_erased_pod {
        void* chunk = nullptr;
        void (*dtor)(void*) noexcept = nullptr;

        unique_erased_pod(unique_erased_pod const&) = delete;
        unique_erased_pod& operator=(unique_erased_pod const&) = delete;

        constexpr unique_erased_pod() noexcept = default;
        constexpr unique_erased_pod(void* chunk, void (*dtor)(void*) noexcept) noexcept
            : chunk{chunk}
            , dtor{dtor} {}
        constexpr unique_erased_pod(unique_erased_pod&& other) noexcept
            : chunk{std::exchange(other.chunk, nullptr)}
            , dtor{std::exchange(other.dtor, nullptr)} {}
        constexpr unique_erased_pod& operator=(unique_erased_pod&& other) noexcept {
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
        constexpr ~unique_erased_pod() noexcept {
            cleanup();
        }

        constexpr explicit operator bool() const noexcept {
            return chunk != nullptr;
        }
        template <class T>
        constexpr explicit operator T*() const noexcept {
            return static_cast<T*>(chunk);
        }

        template <class T> requires (std::destructible<std::decay_t<T>> && 
                                     (!std::is_array_v<std::remove_reference_t<T>>) &&
                                     std::is_nothrow_destructible_v<std::decay_t<T>> &&
                                     std::move_constructible<std::decay_t<T>> &&
                                     std::is_nothrow_move_constructible_v<std::decay_t<T>> &&
                                     std::is_nothrow_constructible_v<std::decay_t<T>, T>)
        constexpr std::decay_t<T>* emplace(T&& src) {
            using Decayed = std::decay_t<T>;
            auto buf = AllocTemplate<Decayed>().allocate(1);
            cleanup();
            this->chunk = buf;
            this->dtor = [](void* raw) constexpr noexcept {
                Decayed* buf = static_cast<Decayed*>(raw);
                std::destroy_at(buf);
                AllocTemplate<Decayed>().deallocate(buf, 1);
            };
            return std::construct_at(buf, std::forward<T>(src));
        }
    };
} // ::mvll::memory

#endif /*INCLUDE_MVLL_MEMORY_HPP*/
