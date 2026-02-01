#ifndef INCLUDE_MVLL_PLATFORM_LINUX_HPP
#define INCLUDE_MVLL_PLATFORM_LINUX_HPP

#include <stdexcept>
#include <utility>

#include <cstddef>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <mvll/error-handling.hpp>

namespace mvll::platform::inline nix
{
    struct [[nodiscard]] unique_fd {
        int fd = -1;
        unique_fd(unique_fd const&) = delete;
        unique_fd& operator=(unique_fd const&) = delete;
        explicit unique_fd(int fd = -1) noexcept
            : fd{fd}
        {
            MVLL_CHECK(0 <= this->get());
        }
        unique_fd(unique_fd&& other) noexcept
            : fd{std::exchange(other.fd, -1)}
        {
        }
        ~unique_fd() MVLL_NOEXCEPT {
            if (this->fd != -1) {
                MVLL_CHECK(-1 != ::close(fd));
                this->fd = -1;
            }
        }
        auto& operator=(unique_fd&& other) noexcept {
            if (this != &other) {
                std::swap(this->fd, other.fd);
            }
            return *this;
        }
        [[nodiscard]] int get() const noexcept { return this->fd; }
        [[nodiscard]] operator int() const noexcept { return this->get(); }
    };

    template <class T>
    class [[nodiscard]] unique_mmap {
    public:
        using value_type = T;
        using pointer = T*;
        using const_pointer = T const*;
        using iterator = pointer;
        using const_iterator = const_pointer;
        using reference = T&;
        using const_reference = T const&;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;

    public:
        unique_mmap(unique_mmap const&) = delete;
        unique_mmap& operator=(unique_mmap const&) = delete;

        unique_mmap() = default;
        explicit unique_mmap(void* target,
                             size_t size,
                             int prot = PROT_READ | PROT_WRITE,
                             int flags = MAP_SHARED | MAP_ANONYMOUS,
                             int fd = -1,
                             size_t offset = 0) MVLL_NOEXCEPT
            : size_{size}
            , addr_{static_cast<T*>(::mmap(target,
                                           size * sizeof (T),
                                           prot,
                                           flags,
                                           fd,
                                           offset * sizeof (T)))}
        {
            MVLL_CHECK(addr_ != MAP_FAILED);
        }
        unique_mmap(unique_mmap&& other) noexcept
            : size_{std::exchange(other.size_, 0)}
            , addr_{std::exchange(other.addr_, static_cast<T*>(MAP_FAILED))}
        {
        }
        ~unique_mmap() MVLL_NOEXCEPT {
            if (addr_ != MAP_FAILED) {
                MVLL_CHECK(-1 != ::munmap(addr_, this->size_in_bytes()));
            }
        }
        auto& operator=(unique_mmap&& other) noexcept {
            if (this != &other) {
                unique_mmap tmp{std::move(other)};
                std::swap(size_, tmp.size_);
                std::swap(addr_, tmp.addr_);
            }
            return *this;
        }

    public:
        [[nodiscard]] size_type size() const noexcept { return size_; }
        [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
        [[nodiscard]] size_type size_in_bytes() const noexcept { return size() * sizeof (T); }
        [[nodiscard]] const_pointer data() const noexcept { return addr_; }
        [[nodiscard]] pointer data() noexcept { return addr_; }

        [[nodiscard]] iterator       begin()       noexcept { return data(); }
        [[nodiscard]] const_iterator begin() const noexcept { return data(); }
        [[nodiscard]] iterator       end  ()       noexcept { return data() + size(); }
        [[nodiscard]] const_iterator end  () const noexcept { return data() + size(); }

        [[nodiscard]] auto& operator[](std::size_t idx) noexcept {
            return *(begin() + idx);
        }
        [[nodiscard]] auto const& operator[](std::size_t idx) const noexcept {
            return *(begin() + idx);
        }
        [[nodiscard]] auto& at(std::size_t idx) {
            if (idx >= size()) throw std::out_of_range(__func__);
            return operator[](idx);
        }
        [[nodiscard]] auto const& at(std::size_t idx) const {
            if (idx >= size()) throw std::out_of_range(__func__);
            return operator[](idx);
        }

    private:
        std::size_t size_ = 0;
        T* addr_ = static_cast<T*>(MAP_FAILED);
    };

} // ::mvll::platform::nix

#endif /* INCLUDE_MVLL_PLATFORM_LINUX_HPP */
