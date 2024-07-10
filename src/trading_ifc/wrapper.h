#pragma once
#include <memory>

namespace trading_api {


template<typename T>
class Wrapper {
public:

    using NullT = typename T::Null;

    static constexpr NullT null_instance;
    static std::shared_ptr<const T> null_instance_ptr;


    Wrapper():_ptr(null_instance_ptr) {}
    Wrapper(std::shared_ptr<const T> ptr):_ptr(ptr) {}

    bool operator==(const Wrapper<T> &other) const = default;
    std::strong_ordering operator<=>(const Wrapper<T> &other) const = default;

    struct Hasher {
        auto operator()(const Wrapper &wrp) {
            std::hash<std::shared_ptr<const T> > hasher;
            return hasher(wrp._ptr);
        }
    };

    auto get_handle() const {return _ptr;}

    explicit operator bool() const {return _ptr != null_instance_ptr;}
    bool defined() const {return _ptr != null_instance_ptr;}
    bool operator!() const {return _ptr == null_instance_ptr;}


protected:
    std::shared_ptr<const T> _ptr;
};

template<typename T>
std::shared_ptr<const T> Wrapper<T>::null_instance_ptr = {&Wrapper<T>::null_instance, [](auto){}};

}
