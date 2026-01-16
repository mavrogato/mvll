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
            void const* input_ = nullptr;
            void const* output_ = nullptr;
            std::coroutine_handle<> continuation = nullptr;
            fiblet_base get_return_object() noexcept {
                return fiblet_base{handle_type::from_promise(*this)};
            }
            [[noreturn]] void unhandled_exception() const noexcept {
                std::terminate();
            }
            void return_void() const noexcept {}
            std::suspend_never initial_suspend() const noexcept { return {}; }
            auto final_suspend() const noexcept {
                struct final_awaiter {
                    bool await_ready() const noexcept { return false; }
                    void await_resume() const noexcept {}
                    std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept {
                        auto base_h = std::coroutine_handle<promise_type>::from_address(h.address());
                        if (auto continuation = base_h.promise().continuation) {
                            return continuation;
                        }
                        return std::noop_coroutine();
                    }
                };
                return final_awaiter{};
            }
            struct event_awaiter {
                promise_type const& self;
                bool await_ready() const noexcept { return false; }
                std::coroutine_handle<> await_suspend(std::coroutine_handle<>) const noexcept {
                    if (auto continuation = self.continuation) {
                        return continuation;
                    }
                    return std::noop_coroutine();                    
                }
                void const* await_resume() const noexcept { return self.input_; }
            };
            auto await_transform(wait_current_tag) const noexcept {
                return event_awaiter{*this};
            }
            auto yield_value(void const* yielded_val) noexcept {
                this->output_ = yielded_val;
                struct yield_awaiter {
                    promise_type& self;
                    bool await_ready() const noexcept { return false; }
                    void const* await_resume() noexcept { return self.input_; }
                    std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept {
                        if (self.continuation) return self.continuation;
                        return std::noop_coroutine();
                    }
                };
                return yield_awaiter{*this};
            }
        };

        void connect(fiblet_base& next) const noexcept {
            MVLL_CHECK(handle_.promise().continuation == nullptr);
            MVLL_CHECK(next.handle_);
            MVLL_CHECK(!next.handle_.done());
            this->handle_.promise().continuation = next.handle_;
        }

        void push(void const* input) const {
            if (handle_ && !handle_.done()) {
                handle_.promise().input_ = input;
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
                        return *static_cast<T const*>(this->self.input_);
                    }
                };
                return typed_awaiter{{*this}};
            }
        };
        void push(T const* input) {
            fiblet_base::push(input);
        }

    private:
        using fiblet_base::fiblet_base;
    };
} // ::mvll

#endif /*INCLUDE_MVLL_FIBLET_HPP*/
