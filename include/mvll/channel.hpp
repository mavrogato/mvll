#ifndef INCLUDE_CHANNEL_HPP
#define INCLUDE_CHANNEL_HPP

#include <coroutine>
#include <utility>

#include <mvll/error-handling.hpp>

namespace mvll
{
    template <class PULL, class PUSH = PULL>
    struct channel {
        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;
        struct promise_type {
            PUSH* push_ptr;
            PULL* pull_ptr;
            std::coroutine_handle<> previous;
            auto get_return_object() noexcept {
                return channel{handle_type::from_promise(*this)};
            }
            std::suspend_always initial_suspend() const noexcept { return {}; }
            std::suspend_always final_suspend() const noexcept { return {}; }
            void unhandled_exception() { throw; }
            void return_void() const noexcept {}
            auto yield_value(PULL* yielded_ptr) {
                this->pull_ptr = yielded_ptr;
                struct yield_awaiter {
                    promise_type& self;
                    bool await_ready() const noexcept { return false; }
                    PUSH* await_resume() noexcept { return self.push_ptr; }
                    std::coroutine_handle<> await_suspend(std::coroutine_handle<>) {
                        return self.previous ? self.previous : std::noop_coroutine();
                    }
                };
                return yield_awaiter{*this};
            }
        };
        struct awaiter {
            handle_type target;
            PUSH *pass_ptr;
            bool await_ready() const noexcept { return false; }
            void await_resume() const noexcept {}
            std::coroutine_handle<> await_suspend(std::coroutine_handle<> current) {
                MVLL_CHECK(!target.promise().previous);
                target.promise().previous = current;
                target.promise().push_ptr = this->pass_ptr;
                return target;
            }
        };

    public:
        handle_type handle;

    public:
        explicit channel(handle_type h) : handle{h} {}
        channel(channel&& other) noexcept
            : handle(std::exchange(other.handle, nullptr))
        {
        }
        channel& operator=(channel&& other) noexcept {
            if (this != &other) {
                if (handle) handle.destroy();
                handle = std::exchange(other.handle, nullptr);
            }
            return *this;
        }
        ~channel() {
            if (handle) {
                handle.destroy();
            }
        }

    public:
        void push(PUSH* push_ptr) const  {
            handle.promise().previous = nullptr;
            handle.promise().push_ptr = push_ptr;
            handle.resume();
        }
        PULL* pull() const {
            return handle.promise().pull_ptr;
        }
        void resume() const {
            handle.resume();
        }
        bool done() const {
            return handle.done();
        }
        awaiter pass(PUSH* pass_ptr) const {
            return awaiter{this->handle, pass_ptr};
        }
    };
} // ::mvll

#endif /* INCLUDE_CHANNEL_HPP */
