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
                               std::span<Value> >;


class Value : public ValueBase {
public:
    using ValueBase::ValueBase;
};


class StrategyConfig {
public:

    StrategyConfig() = default;

    StrategyConfig(const std::vector<std::pair<std::string, Value> > &values) {
        for (const auto &[k,v]: values) {
            _values[k] = &v;
        }
    }

    struct ValueRef {
        const Value *ref;

        template<typename T>
        operator T () const {
            if (ref == nullptr) return T();
            return std::visit([&](const auto &x){
                if constexpr(std::is_constructible_v<decltype(x), T>) {
                    return T(x);
                } else {
                    return T();
                }
            },*ref);
        }

        template<typename T>
        T operator()(const T &defval) const {
            if (ref == nullptr) return defval;
            return std::visit([&](const auto &x){
                if constexpr(std::is_constructible_v<decltype(x), T>) {
                    return T(x);
                } else {
                    return defval;
                }
            },*ref);
        }
};

    ValueRef operator[](std::string_view name) const {
        auto iter = _values.find(name);
        if (iter == _values.end()) return {nullptr};
        else return {iter->second};
    }


protected:
    std::map<std::string_view, const Value *> _values;
};




}
