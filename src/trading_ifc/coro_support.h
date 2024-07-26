#pragma once

#include "async.h"
#include <coroutine>
#include <optional>

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
        void unhandled_exception() {
            ErrorGuard::handle_exception();
        }
    };

};



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
        if (!_status.has_value()) {
            throw AsyncCallException(AsyncStatus::canceled);;
        }
        if (_status->get_status() != AsyncStatus::ok) {
            throw AsyncCallException(*_status);
        }
    }

protected:

    struct Resumer {
        void operator()(completion_awaiter *x) {
            x->_h.resume();
        }
    };

    CompletionCB create_callback() {
        return [me = std::unique_ptr<completion_awaiter, Resumer>(this)](AsyncStatus status){
            me->_status.emplace(std::move(status));
        };
    }

    std::optional<AsyncStatus> _status;


    std::coroutine_handle<> _h = {};
};


}
