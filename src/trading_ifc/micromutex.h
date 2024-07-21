#pragma once

#include <atomic>

class uMutex {
public:

    void lock() {
        while (_state.exchange(true) == true) {
            _state.wait(true);
        }
    }
    void unlock() {
        _state = false;
        _state.notify_all();
    }

    bool try_lock() {
        return !_state.exchange(true);
    }

    uMutex() = default;
    uMutex(const uMutex &) = delete;
    uMutex &operator=(const uMutex &) = delete;


protected:
    std::atomic<bool> _state = {};
};
