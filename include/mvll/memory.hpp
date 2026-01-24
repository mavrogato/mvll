#ifndef INCLUDE_MVLL_MEMORY_HPP
#define INCLUDE_MVLL_MEMORY_HPP

#include <memory_resource>
#include <utility>

#include <cstddef>

namespace mvll::inline memory_strategy
{
    struct unique_erased_pod {
        std::byte* chunk = nullptr;
        void (*dtor)(void*) noexcept = nullptr;

        unique_erased_pod(unique_erased_pod const&) = delete;
        unique_erased_pod& operator=(unique_erased_pod const&) = delete;

        unique_erased_pod() noexcept = default;
        unique_erased_pod(std::byte* chunk, void (*release)(void*) noexcept) noexcept
            : chunk{chunk}
            , dtor{release} {}
        unique_erased_pod(unique_erased_pod&& other) noexcept
            : chunk{std::exchange(other.chunk, nullptr)}
            , dtor{std::exchange(other.dtor, nullptr)} {}
        unique_erased_pod& operator=(unique_erased_pod&& other) noexcept {
            if (this != &other) {
                cleanup();
                chunk = std::exchange(other.chunk, nullptr);
                dtor = std::exchange(other.dtor, nullptr);
            }
            return *this;
        }
        void cleanup() noexcept {
            if (chunk && dtor) {
                dtor(chunk);
            }
            chunk = nullptr;
            dtor = nullptr;
        }
        ~unique_erased_pod() noexcept {
            cleanup();
        }

        template <class T> requires (std::destructible<std::decay_t<T>> && 
                                     (!std::is_array_v<std::remove_reference_t<T>>) &&
                                     std::is_nothrow_destructible_v<std::decay_t<T>> &&
                                     std::move_constructible<std::decay_t<T>>)
        std::decay_t<T>* emplace(T&& src) {
            using Decayed = std::decay_t<T>;
            constexpr std::size_t size = sizeof (Decayed);
            constexpr std::size_t align = alignof (Decayed);
            static auto alloc = std::pmr::new_delete_resource();
            auto chunk = static_cast<std::byte*>(alloc->allocate(size, align));
            cleanup();
            this->chunk = chunk;
            this->dtor = [](void* raw) noexcept {
                static_cast<Decayed*>(raw)->~Decayed();
                alloc->deallocate(raw, size, align);
            };
            return new (chunk) Decayed(std::forward<T>(src));
        }
    };
} // ::mvll::memory_strategy

#endif /*INCLUDE_MVLL_MEMORY_HPP*/
