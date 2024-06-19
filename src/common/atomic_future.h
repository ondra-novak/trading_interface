#pragma once
#include <atomic>
#include <memory>

namespace trading_api {


///Contains future value which can be set (once) atomically
template<typename T>
class atomic_future {
public:

    atomic_future():_valid(false) {}
    template<std::convertible_to<T> V>
    atomic_future(V &&val):_valid(true), _value(std::forward<V>(val)) {}
    atomic_future(const atomic_future &other):_valid(other._valid.load(std::memory_order_acquire)) {
        if (_valid.load(std::memory_order_relaxed)) {
            std::construct_at(&_value, other._value);
        }
    }
    atomic_future &operator=(const atomic_future &) = delete;

    ~atomic_future() {
        if (_valid.load(std::memory_order_relaxed)) {
            std::destroy_at(&_value);
        }
    }

    bool ready() const {
        return _valid.load(std::memory_order_acquire);
    }

    ///Set value
    /**
     * @note NOT MT SAFE - you can have multiple readers, but only one writer
     * @param val new value
     */
    template<std::convertible_to<T> V>
    void set(V &&val) {
        if (!ready()) {
            std::construct_at(&_value, std::forward<V>(val));
            _valid.store(true, std::memory_order_release);
        }
    }

    ///Get pointer or return nullptr if not ready
    T *try_get() {
        if (ready()) {
            return &_value;
        } else {
            return nullptr;
        }
    }

    ///Get pointer or return nullptr if not ready
    const T *try_get() const {
        if (ready()) {
            return &_value;
        } else {
            return nullptr;
        }
    }

    ///wait for resolution
    void wait() const {
        while (!ready()) {
            _valid.wait(false);
        }
    }

    ///get value (or wait)
    T &get() & {
        wait();
        return _value;
    }
    ///get value (or wait)
    T &&get() && {
        wait();
        return std::move(_value);
    }

    ///get value (or wait)
    const T &get() const & {
        wait();
        return _value;
    }

    ///get value (or wait)
    const T &&get() const && {
        wait();
        return _value;
    }

    ///reset internal state, the variable can be used again
    /** @note NOT MT SAFE */
    void reset() {
        if (ready()) {
            std::destroy_at(&_value);
            _valid.store(false);
        }
    }

protected:
    std::atomic<bool> _valid;
    union {
        T _value;
    };
};


}
