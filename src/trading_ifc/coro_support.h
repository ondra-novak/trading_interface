#pragma once

#include <coroutine>
#include "function.h"

namespace trading_api {


///simple coroutine
/**
 * Starts coroutine - allows to use co_await. There is no synchronization
 */
class coroutine {
public:
    struct promise_type {
        static constexpr std::suspend_never initial_suspend() noexcept {return {};}
        static constexpr std::suspend_never final_suspend() noexcept {return {};}
        static constexpr void return_void() {}
        coroutine get_return_object() {return {};}
        void unhandled_exception() {}
    };
};


using CompletionCB = Function<void()>;

///awaitable object
class [[nodiscard]] completion_awaiter {
public:


    template<std::invocable<CompletionCB> Fn>
    completion_awaiter(Fn &&fn) {
        fn(create_callback());
    }

    completion_awaiter(const completion_awaiter &) = delete;
    completion_awaiter &operator=(const completion_awaiter &) = delete;

    static constexpr bool await_ready() noexcept {return false;}
    void await_suspend(std::coroutine_handle<> h) {
        _h = h;
    }
    void await_resume() {
        if (!_called) throw std::runtime_error("canceled");
    }

protected:

    struct Resumer {
        void operator()(completion_awaiter *x) {
            x->_h.resume();
        }
    };

    CompletionCB create_callback() {
        return [me = std::unique_ptr<completion_awaiter, Resumer>(this)]{
            me->_called = true;
        };
    }

    bool _called = false;
    std::coroutine_handle<> _h = {};
};


}
