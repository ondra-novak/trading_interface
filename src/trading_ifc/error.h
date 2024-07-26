#pragma once
#include <memory>

namespace trading_api {

class AsyncStatus {
public:
    enum Type {
        ok,
        timeout,
        canceled,
        failed,
        gone
    };
    AsyncStatus() = default;
    AsyncStatus(Type t):type(t) {}
    AsyncStatus(Type t, std::string_view message):type(t),message_size(message.size()) {
        auto ptr = std::make_shared<char[]>(message.size());
        std::copy(message.begin(), message.end(), ptr.get());
        message_ptr = std::move(ptr);
    }


    bool operator==(const AsyncStatus &) const = default;
    Type get_status() const {return type;}
    std::string_view get_message() const {
        return {message_ptr.get(), message_size};
    }
    operator std::string_view() const {return get_message();}
    operator Type() const {return type;}

    friend std::string_view to_string(AsyncStatus st) {
        switch (st.type) {
            case AsyncStatus::ok: return "OK";
            case AsyncStatus::canceled: return "canceled";
            case AsyncStatus::timeout: return "timeout";
            case AsyncStatus::failed: return "failed";
            case AsyncStatus::gone: return "gone";
            default: return "AsyncStatus=unknown";
        }
    }


protected:
    Type type = Type::ok;
    std::shared_ptr<const char[]> message_ptr = {};
    std::size_t message_size = 0;

};


class AsyncCallException: public std::exception {
public:

    AsyncCallException(AsyncStatus st):_status(std::move(st)) {}

    virtual const char *what() const noexcept override {
        if (_whatmsg.empty()) {
            _whatmsg.append(to_string(_status));
            std::string_view msg = _status.get_message();
            if (msg.empty()) {
                _whatmsg.push_back(':');
                _whatmsg.append(msg);
            }
        }
        return _whatmsg.c_str();
    }

protected:
    AsyncStatus _status;
    mutable std::string _whatmsg;
};

class IErrorHandler {
public:
    virtual void on_unhandled_exception()  = 0;
    virtual ~IErrorHandler() = default;
};

class ErrorGuard {
public:
    static bool handle_exception() {
        if (handler) {
            handler->on_unhandled_exception();
            return true;
        } else {
            return false;
        }
    }

    ErrorGuard(IErrorHandler *h):_save(handler) {handler = h;}
    ~ErrorGuard() {handler = _save;}

protected:
    IErrorHandler *_save = nullptr;
    static thread_local IErrorHandler *handler;
};

inline thread_local IErrorHandler *ErrorGuard::handler = nullptr;


}
