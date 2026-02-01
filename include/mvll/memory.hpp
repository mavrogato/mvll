#ifndef INCLUDE_MVLL_MEMORY_HPP
#define INCLUDE_MVLL_MEMORY_HPP

#include <algorithm>
#include <concepts>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>


namespace mvll::inline memory
{
    struct alloc_block {
        constexpr static std::size_t DEFAULT_NEW_ALIGNMENT = alignof (std::max_align_t);
        alignas (DEFAULT_NEW_ALIGNMENT) std::byte data[DEFAULT_NEW_ALIGNMENT];
        static inline constexpr auto count(std::size_t sz) noexcept {
            auto blksz = sizeof (alloc_block);
            return (sz + blksz - 1) / blksz;
        }
    };

    template <class T>
    concept is_boxable_type = std::destructible<std::decay_t<T>>
        && (!std::is_array_v<std::remove_reference_t<T>>)
        && std::is_nothrow_destructible_v<std::decay_t<T>>
        && std::move_constructible<std::decay_t<T>>
        && std::is_nothrow_move_constructible_v<std::decay_t<T>>
        && std::is_nothrow_constructible_v<std::decay_t<T>, T>;

    template <template <class> class AllocTemplate = std::allocator> // T.B.D.
    class move_only_erased_box {
    public:
        move_only_erased_box(move_only_erased_box const&) = delete;
        move_only_erased_box& operator=(move_only_erased_box const&) = delete;

        constexpr move_only_erased_box() noexcept = default;
        template <class T> requires (!std::derived_from<std::decay_t<T>, move_only_erased_box<AllocTemplate>>
                                     && is_boxable_type<T>)
        constexpr explicit move_only_erased_box(T&& src) {
            this->emplace(std::forward<T>(src));
        }
        constexpr move_only_erased_box(move_only_erased_box&& other) noexcept
            : size_{std::exchange(other.size_, 0)}
            , data_{std::exchange(other.data_, nullptr)}
            , release_{std::exchange(other.release_, nullptr)} {}
        constexpr move_only_erased_box& operator=(move_only_erased_box&& other) noexcept {
            if (this != &other) {
                cleanup();
                size_ = std::exchange(other.size_, 0);
                data_ = std::exchange(other.data_, nullptr);
                release_ = std::exchange(other.release_, nullptr);
            }
            return *this;
        }
        template <class T> requires (!std::derived_from<std::decay_t<T>, move_only_erased_box<AllocTemplate>>
                                     && is_boxable_type<T>)
        constexpr std::decay_t<T>& operator=(T&& src) noexcept { // NOTE: the return type
            return this->emplace(std::forward<T>(src));
        }
        constexpr void cleanup() noexcept {
            if (data_ && release_) {
                release_(data_, size_);
            }
            release_ = nullptr;
            data_ = nullptr;
            size_ = 0;
        }
        constexpr ~move_only_erased_box() noexcept {
            cleanup();
        }

        [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return data_ != nullptr;
        }
        template <class T = void>
        [[nodiscard]] constexpr T const* get() const noexcept {
            return static_cast<T const*>(data_);
        }
        template <class T = void>
        [[nodiscard]] constexpr T* get() noexcept {
            return static_cast<T*>(data_);
        }
        template <class T>
        [[nodiscard]] constexpr explicit operator T const*() const noexcept {
            return get<T>();
        }
        template <class T>
        [[nodiscard]] constexpr explicit operator T*() noexcept {
            return get<T>();
        }
        [[nodiscard]] constexpr std::span<std::byte const> view() const noexcept {
            if (this->operator bool()) {
                return {static_cast<std::byte const*>(data_), this->size_};
            }
        }

        template <is_boxable_type T>
        constexpr std::decay_t<T>& emplace(T&& src) {
            using Decayed = std::decay_t<T>;
            auto chunk = AllocTemplate<Decayed>().allocate(1);
            cleanup();
            this->size_ = 1;
            this->data_ = chunk;
            this->release_ = [](void* raw, std::size_t) constexpr noexcept {
                Decayed* buf = static_cast<Decayed*>(raw);
                std::destroy_at(buf);
                AllocTemplate<Decayed>().deallocate(buf, 1);
            };
            return *std::construct_at(chunk, std::forward<T>(src));
        }

        constexpr std::span<std::byte> emplace_string(std::byte const* src, std::size_t size) {
            auto chunk = AllocTemplate<std::byte>().allocate(size);
            cleanup();
            std::copy_n(src, size, chunk);
            this->size_ = size;
            this->data_ = chunk;
            this->release_ = [](void* raw, std::size_t size) constexpr noexcept {
                std::byte* chunk = static_cast<std::byte*>(raw);
                AllocTemplate<std::byte>().deallocate(chunk, size);
            };
            return {chunk, size};
        }

        /*constexpr*/ std::span<std::byte> alloc_blob(std::size_t size) {
            using block_type = alloc_block;
            std::size_t count = block_type::count(size);
            auto chunk = AllocTemplate<block_type>().allocate(count);
            cleanup();
            //std::copy_n(static_cast<std::byte const*>(src), size, chunk->data);
            this->size_ = size;
            this->data_ = chunk->data;
            this->release_ = [](void* raw, std::size_t size) constexpr noexcept {
                block_type* chunk = static_cast<block_type*>(raw);
                AllocTemplate<block_type>().deallocate(chunk, block_type::count(size));
            };
            return {chunk->data, size};
        }

    private:
        std::size_t size_ = 0;
        void* data_ = nullptr;
        void (*release_)(void*, std::size_t) noexcept = nullptr;
    };
} // ::mvll::memory

#endif /*INCLUDE_MVLL_MEMORY_HPP*/
