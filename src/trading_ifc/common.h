#pragma once

#include <concepts>
#include <iterator>
#include <iostream>
#include <string_view>
#include <source_location>

namespace trading_api {

template<typename T>
concept is_container = requires(T x) {
    {x.begin()}->std::input_iterator;
    {x.end()}->std::input_iterator;
    {x.begin() == x.end()};
};

template<typename T>
concept is_pair = requires(T x) {
    {x.first};
    {x.second};
};

template<typename T>
concept has_to_string_global = requires(T x) {
    {to_string(x)}->std::convertible_to<std::string_view>;
};


template<typename T>
concept can_output_to_ostream = requires(T x, std::ostream &stream) {
    {stream << x};
};

namespace _details {

    template<typename T> struct DetectFnFirstArg;
    template<typename Obj, typename R, typename X, typename ... Args>
    auto detect_arg(R (Obj::*)(X, Args...)) -> X;
    template<typename Obj, typename R, typename X, typename ... Args>
    auto detect_arg(R (Obj::*)(X, Args...) &) -> X;
    template<typename Obj, typename R, typename X, typename ... Args>
    auto detect_arg(R (Obj::*)(X, Args...) &&) -> X;
    template<typename Obj, typename R, typename X, typename ... Args>
    auto detect_arg(R (Obj::*)(X, Args...) const) -> X;
    template<typename Obj, typename R, typename X, typename ... Args>
    auto detect_arg(R (Obj::*)(X, Args...) const &) -> X;
    template<typename Obj, typename R, typename X, typename ... Args>
    auto detect_arg(R (Obj::*)(X, Args...) const &&) -> X;
    template<typename Ret, typename X, typename ... Args>
    struct DetectFnFirstArg<Ret (*)(X, Args...)> {using type = X;};
    template<typename Ret, typename X, typename ... Args>
    struct DetectFnFirstArg<Ret (*)(X, Args...) noexcept> {using type = X;};
    template<typename T> requires requires {{detect_arg(&T::operator())};}
    struct DetectFnFirstArg<T> {using type = decltype(detect_arg(&T::operator()));};

}

template<typename T>
using first_lambda_arg = typename _details::DetectFnFirstArg<T>::type;

template<typename T>
concept lambda_with_1arg = requires(T v) {
    typename first_lambda_arg<T>;
};


namespace _details {
    template<typename T>
    class ClassName: public std::string_view {
    public:
        constexpr ClassName():std::string_view(get_class_name()) {}

        static constexpr std::string_view get_class_name(std::source_location loc = std::source_location::current()) {
            std::string_view fn = loc.function_name();
            auto p1 = fn.find("T = ");
            if (p1 != fn.npos) {
                auto p2 = fn.find("]", p1);
                return fn.substr(p1+4,p2 -p1 - 4);
            } else {
                p1 = fn.find('<');
                auto p2 = fn.rfind('>');
                return fn.substr(p1+1,p2 -p1 - 1);
            }
        }

    };
}

template<typename T>
constexpr std::string_view type_to_string = _details::ClassName<T>();



enum class Side {
    undefined = 0,
    buy = 1,
    sell = -1
};

inline Side reverse(Side side) {
    switch (side) {
        case Side::buy: return Side::sell;
        case Side::sell: return Side::buy;
        default: return side;
    }
}



}
