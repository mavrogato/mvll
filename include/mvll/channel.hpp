#ifndef INCLUDE_CHANNEL_HPP
#define INCLUDE_CHANNEL_HPP

#include <coroutine>
#include <exception>
#include <utility>

//#include <mvll/error-handling.hpp>

namespace mvll
{
    template <class PULL, class PUSH = PULL>
    struct channel {
        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;
        struct promise_type {
            PUSH* push_ptr = nullptr;
            PULL* pull_ptr = nullptr;
            std::exception_ptr exception = nullptr;
            bool started = false;
            bool updated = false;
            std::coroutine_handle<> previous;
            auto get_return_object() noexcept {
                return channel{handle_type::from_promise(*this)};
            }
            std::suspend_always initial_suspend() const noexcept { return {}; }
            auto final_suspend() const noexcept {
                struct final_awaiter {
                    bool await_ready() const noexcept { return false; }
                    void await_resume() const noexcept {}
                    std::coroutine_handle<> await_suspend(handle_type h) noexcept {
                        h.promise().updated = false;
                        h.promise().pull_ptr = nullptr;
                        return h.promise().previous ? h.promise().previous : std::noop_coroutine();
                    }
                };
                return final_awaiter{};
            }
            void unhandled_exception() {
                this->exception = std::current_exception();
            }
            void return_void() const noexcept {}
            auto yield_value(PULL* yielded_ptr) {
                this->pull_ptr = yielded_ptr;
                this->updated = true;
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
            void await_resume() const noexcept {
                if (auto e = target.promise().exception) {
                    std::rethrow_exception(e);
                }
            }
            std::coroutine_handle<> await_suspend(std::coroutine_handle<> current) {
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
        channel(channel const&) = delete;
        channel& operator=(channel const&) = delete;

    public:
        void start() const {
            // if (handle && !handle.done() && !handle.promise().started) {
            //     handle.promise().started = true;
            //     handle.resume();
            // }
        }
        void push(PUSH* push_ptr) const  {
            if (!handle || handle.done()) return;
            handle.promise().previous = nullptr;
            handle.promise().push_ptr = push_ptr;
            handle.promise().updated = false;
            handle.resume();
        }
        PULL* pull() const {
            if (auto e = handle.promise().exception) {
                std::rethrow_exception(e);
            }
            if (handle && !handle.done()) {
                if (bool& started = handle.promise().started; !started) {
                    started = true;
                    handle.resume();
                }
                // if (handle.promise().updated) {
                //     handle.promise().updated = false;
                //     return handle.promise().pull_ptr;
                // }
            }
            return nullptr;
        }
        bool done() const {
            return handle.done();
        }
        awaiter pass(PUSH* pass_ptr) const {
            // if (handle && !handle.done()) {
            //     if (bool& started = handle.promise().started; !started) {
            //         started = true;
            //         handle.resume();
            //     }
            // }
            return awaiter{this->handle, pass_ptr};
        }
    };
} // ::mvll

#endif /* INCLUDE_CHANNEL_HPP */
