#pragma once

#include <memory>

namespace trading_api {

template<typename T>
class SharedState {
public:

    using value_type = T;

    struct NullLock {
        void lock() {};
        void unlock() {};
        bool try_lock() {return true;}
    };

    using Lock = std::conditional_t<std::is_const_v<T>, NullLock, std::mutex>;

    class Content {
    public:
        Content(T &v):_val(std::move(v)) {}
        T _val;
        Lock lock;
        std::atomic<bool> *_signaled ={};

        virtual ~Content() {
            if (_signaled) {
                *_signaled = true;
                _signaled->notify_all();
            }
        }
    };

    template<typename Fn>
    class Content_Fn: public Content {
    public:
        Content_Fn(T &&v, Fn &&fn):Content(v),_fn(std::move(fn)) {}
        virtual ~Content_Fn() {

            _fn(this->_val);
        }
    protected:
        Fn _fn;
    };


    void lock() const {
        _ctx->lock.lock();
    }
    void unlock() const {
        _ctx->lock.unlock();
    }
    bool try_lock() const {
        return _ctx->lock.try_lock();
    }
    value_type *operator->() const {
        return &_ctx->_val;
    }

    value_type &operator*() const {
        return _ctx->_val;
    }

    void clear() {_ctx.reset();}

    template<typename Fn>
    SharedState(T &data, Fn &&fn):_ctx(std::shared_ptr<Content>(
            new Content_Fn<std::decay_t<Fn> >(std::move(data), std::move(fn)))) {}
    template<typename Fn>
    SharedState(T &&data, Fn &&fn): SharedState(data, std::forward<Fn>(fn)) {}
    SharedState(T &data):SharedState(data,[](const auto &){}) {}
    SharedState(T &&data):SharedState(data) {}
    SharedState():SharedState(T()) {}

    void wait() {
        std::atomic<bool> w={false};
        _ctx->_signaled = &w;
        _ctx.reset();
        w.wait(false);
    }

protected:

    std::shared_ptr<Content> _ctx;


};


}
