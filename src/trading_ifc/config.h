#pragma once

#include <variant>
#include <span>
#include <vector>
#include <map>
#include <string>

namespace trading_api {


class Value;

struct DateValue {
    int year = 0;
    int month = 0;
    int day = 0;
    bool operator == (const DateValue &) const = default;
    std::strong_ordering operator <=> (const DateValue &) const = default;
    constexpr bool valid() const {return month>0  && month <=12 && day > 0 && day <=31;}
};



struct TimeValue {
    int hour = 0;
    int minute = 0;
    int second = 0;
    bool operator == (const TimeValue &) const = default;
    std::strong_ordering operator <=> (const TimeValue &) const = default;
};

using ValueBase = std::variant<std::monostate,
                               double,
                               int,
                               long,
                               bool,
                               DateValue,
                               TimeValue,
                               std::string,
                               std::span<Value> >;


class Value : public ValueBase {
public:
    using ValueBase::ValueBase;
};


class Config {
public:

    Config() = default;

    Config(const std::vector<std::pair<std::string, Value> > &values) {
        for (const auto &[k,v]: values) {
            _values[k] = v;
        }
    }

    struct ValueRef {
        const Value *ref;

        template<typename T>
        operator T () const {
            if (ref == nullptr) return T();
            return std::visit([&](const auto &x){
                if constexpr(std::is_constructible_v<T, decltype(x)>) {
                    return T(x);
                } else {
                    return T();
                }
            },*ref);
        }


        template<typename T>
        T as() const {
            if (ref == nullptr) return T();
            return std::visit([&](const auto &x){
                if constexpr(std::is_constructible_v<T, decltype(x)>) {
                    return T(x);
                } else {
                    return T();
                }
            },*ref);
        }

        template<typename Def, typename T = Def>
        T get(const Def &def) const {
            static_assert(std::is_constructible_v<T, const Def &>);
            if (ref == nullptr) return T(def);
            return std::visit([&](const auto &x){
                if constexpr(std::is_constructible_v<T, decltype(x)>) {
                    return T(x);
                } else {
                    return T(def);
                }
            },*ref);
        }

        bool operator!() const {return ref == nullptr;}
};

    ValueRef operator[](std::string_view name) const {
        auto iter = _values.find(name);
        if (iter == _values.end()) return {nullptr};
        else return {&iter->second};
    }



    template<std::convertible_to<std::string_view> ... Args>
    auto operator()(const Args & ... args) const {
        return std::make_tuple(operator[](std::string_view(args))...);
    }

protected:
    std::map<std::string, Value ,std::less<>> _values;
};




}
