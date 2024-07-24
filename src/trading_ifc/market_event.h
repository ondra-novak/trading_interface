#pragma once
#include "common.h"
#include <typeinfo>
#include <sstream>

namespace trading_api {



class InvalidType : public std::exception {
public:
    virtual const char *what() const noexcept {return "InvalidType: Variable contains different type";}
};

///Type erased reference
class AnyRef {
public:

    ///construct any reference
    template<typename T>
    AnyRef(const T &val):_ti(&typeid(T)),_ptr(&val),_print(&print_fn<T>) {}

    AnyRef(const AnyRef &) = delete;
    AnyRef &operator=(const AnyRef &) = delete;

    ///test, whether the reference contains this type
    template<typename T>
    bool is() const {
        return typeid(T) == *_ti;
    }

    ///retrieve as type
    template<typename T>
    const T &get() const {
        if (!is<T>()) throw InvalidType();
        return *reinterpret_cast<const T *>(_ptr);
    }

    ///copy to variable
    /**
     * @param ref variable where value is copied
     * @retval true copied
     * @retval false different type
     */
    template<typename T>
    bool get(T &ref) const {
        if (!is<T>()) return false;
        ref = *reinterpret_cast<const T *>(_ptr);
        return true;
    }

    ///direct convert to T
    template<typename T>
    operator const T &() const {
        return get<T>();
    }

    ///calls lambda if its argument matches to stored type
    template<lambda_with_1arg Fn>
    bool operator>>(Fn &&fn) {
        using T = std::decay_t<typename _details::DetectFnFirstArg<std::decay_t<Fn> >::type>;
        if (is<T>()) {
            fn(get<T>());
            return true;
        }
        return false;
    }

    ///to stream
    friend std::ostream &operator<<(std::ostream &other, const AnyRef &ref) {
        if (ref._print) {
            ref._print(other, ref._ptr);
        } else {
            other << ref._ti->name();
        }
        return other;
    }

    ///retrieves internal value as string
    std::string to_string() const {
        std::ostringstream str;
        str << *this;
        return str.str();
    }

protected:

    const std::type_info *_ti;
    const void *_ptr;
    void (*_print)(std::ostream &out, const void *);



    template<typename T>
    static void print_fn(std::ostream &out, const void *ptr) {
        out << type_to_string<T>;
        if constexpr(can_output_to_ostream<T>) {
             const T *val = reinterpret_cast<const T *>(ptr);
             out << "(" << *val << ")";
        }
    }
};

class MarketEvent: public AnyRef {
public:
    using AnyRef::AnyRef;
};

class Signal: public AnyRef {
public:
    using AnyRef::AnyRef;
};
}
