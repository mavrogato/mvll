#ifndef INCLUDE_MVLL_FIBLET_HPP
#define INCLUDE_MVLL_FIBLET_HPP

#include <mvll/error-handling.hpp>

#include <coroutine>
#include <exception>
#include <utility>

namespace mvll
{
    struct wait_current_tag{};
    inline constexpr wait_current_tag wait_current{};
    struct fiblet_base {
        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;
        struct promise_type {
            void const* current;
            std::exception_ptr exception = nullptr;
            std::coroutine_handle<> previous = nullptr;
            fiblet_base get_return_object() noexcept {
                return fiblet_base{handle_type::from_promise(*this)};
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
                    std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept {
                        auto base_h = std::coroutine_handle<promise_type>::from_address(h.address());
                        if (auto previous = base_h.promise().previous) {
                            return previous;
                        }
                        return std::noop_coroutine();
                    }
                };
                return final_awaiter{};
            }
            struct event_awaiter {
                promise_type& self;
                bool await_ready() const noexcept { return false; }
                std::coroutine_handle<> await_suspend(std::coroutine_handle<>) const noexcept {
                    if (auto next = self.previous) {
                        return next;
                    }
                    return std::noop_coroutine();                    
                }
                void const* await_resume() const noexcept { return self.current; }
            };
            auto await_transform(wait_current_tag) noexcept {
                return event_awaiter{*this};
            }
        };

        void connect(fiblet_base& next) const noexcept {
            MVLL_CHECK(handle_.promise().previous == nullptr);
            MVLL_CHECK(next.handle_);
            MVLL_CHECK(!next.handle_.done());
            this->handle_.promise().previous = next.handle_;
        }

        void push(void const* update) const {
            if (handle_ && !handle_.done()) {
                handle_.promise().current = update;
                handle_.resume();
            }
        }

    public:
        fiblet_base() = default;
        fiblet_base(fiblet_base&& other) noexcept 
            : handle_(std::exchange(other.handle_, nullptr)) {}
        fiblet_base& operator=(fiblet_base&& other) noexcept {
            if (this != &other) {
                if (handle_) handle_.destroy();
                handle_ = std::exchange(other.handle_, nullptr);
            }
            return *this;
        }
        ~fiblet_base() noexcept {
            if (handle_) {
                handle_.destroy();
            }
        }
        handle_type handle() const noexcept { return handle_; }

    protected:
        explicit fiblet_base(handle_type h) : handle_{h} {}
        handle_type handle_;
    };

    template <class T>
    struct fiblet : fiblet_base {
        struct promise_type : fiblet_base::promise_type {
            fiblet get_return_object() noexcept {
                return fiblet{handle_type::from_promise(*this)};
            }
            auto await_transform(wait_current_tag) noexcept {
                struct typed_awaiter : event_awaiter {
                    T const& await_resume() const noexcept {
                        return *static_cast<T const*>(this->self.current);
                    }
                };
                return typed_awaiter{{*this}};
            }
        };
        void push(T const* update) {
            fiblet_base::push(update);
        }

    private:
        using fiblet_base::fiblet_base;
    };
} // ::mvll

#endif /*INCLUDE_MVLL_FIBLET_HPP*/
