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
            void const* current_ = nullptr;
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
                void const* await_resume() const noexcept { return self.current_; }
            };
            auto await_transform(wait_current_tag) const noexcept {
                return event_awaiter{*this};
            }
            auto yield_value(void const* yielded_val) noexcept {
                this->current_ = yielded_val;
                struct yield_awaiter {
                    promise_type& self;
                    bool await_ready() const noexcept { return false; }
                    void const* await_resume() noexcept { return self.current_; }
                    std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept {
                        if (self.continuation) return self.continuation;
                        return std::noop_coroutine();
                    }
                };
                return yield_awaiter{*this};
            }
        };

        [[deprecated]]
        void connect(fiblet_base& next) const noexcept {
            MVLL_CHECK(handle().promise().continuation == nullptr);
            MVLL_CHECK(next.handle());
            MVLL_CHECK(!next.handle().done());
            handle().promise().continuation = next.handle();
        }

        void push(void const* update) const {
            if (handle() && !handle().done()) {
                handle().promise().current_ = update;
                handle().resume();
            }
        }

    public:
        fiblet_base() = default;
        explicit fiblet_base(std::coroutine_handle<> h) : handle_{h} {}
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
                handle_ = nullptr;
            }
        }
        handle_type handle() const noexcept {
            return handle_type::from_address(handle_.address());
        }

    private:
        std::coroutine_handle<> handle_;
    };

    template <class T>
    struct fiblet : fiblet_base {
        using fiblet_base::fiblet_base;
        void push(T const* input) {
            fiblet_base::push(input);
        }
    };
} // ::mvll

namespace std
{
    template <class T, class ...Args>
    struct coroutine_traits<mvll::fiblet<T>, Args...> {
        struct promise_type : mvll::fiblet_base::promise_type {
            mvll::fiblet<T> get_return_object() noexcept {
                return mvll::fiblet<T> {
                    std::coroutine_handle<promise_type>::from_promise(*this),
                };
            }
            auto await_transform(mvll::wait_current_tag) noexcept {
                struct fiblet_event_awaiter : event_awaiter {
                    T const& await_resume() const noexcept {
                        return *static_cast<T const*>(this->self.current_);
                    }
                };
                return fiblet_event_awaiter{{*this}};
            }
        };
    };
} // ::std

#endif /*INCLUDE_MVLL_FIBLET_HPP*/
