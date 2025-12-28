#ifndef INCLUDE_CHANNEL_HPP
#define INCLUDE_CHANNEL_HPP

#include <coroutine>
#include <type_traits>
#include <utility>
#include <variant>

namespace mvll
{
    template <class PULL, class PUSH = PULL>
    struct channel {
        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;
        struct promise_type {
            using storage_type = std::conditional_t<std::is_void_v<PUSH>, std::monostate, PUSH>;
            storage_type push_val;
            PULL const* pull_ptr;
            auto get_return_object() noexcept {
                return channel{handle_type::from_promise(*this)};
            }
            std::suspend_always initial_suspend() const noexcept { return {}; }
            std::suspend_always final_suspend() const noexcept { return {}; }
            void unhandled_exception() { throw; }
            void return_void() const noexcept {}
            auto yield_value(PULL const& yielded) {
                pull_ptr = &yielded;
                struct awaiter {
                    PUSH* push_ptr;
                    bool await_ready() const noexcept { return false; }
                    PUSH* await_resume() noexcept { return push_ptr; }
                    std::coroutine_handle<> await_suspend(std::coroutine_handle<>) {
                        return std::noop_coroutine();
                    }
                };
                if constexpr (std::is_void_v<PUSH>) {
                    return awaiter{nullptr};
                }
                else {
                    return awaiter{&this->push_val};
                }
            }
        };
        struct awaiter {
            handle_type target;
            PUSH push_val;
            bool await_ready() const noexcept { return false; }
            void await_resume() const noexcept {}
            std::coroutine_handle<> await_suspend(std::coroutine_handle<>) {
                target.promise().push_val = std::move(push_val);
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
        void push(PUSH&& push_val) const requires (!std::is_void_v<PUSH>) {
            handle.promise().push_val = std::move(push_val);
            handle.resume();
        }
        void push() const requires std::is_void_v<PUSH> {
            handle.resume();
        }
        PULL const& pull() const {
            return *handle.promise().pull_ptr;
        }
        void resume() const {
            handle.resume();
        }
        bool done() const {
            return handle.done();
        }
        awaiter pass(PUSH&& pass_val) const {
            return awaiter{this->handle, pass_val};
        }
    };
} // ::mvll

#endif /* INCLUDE_CHANNEL_HPP */
