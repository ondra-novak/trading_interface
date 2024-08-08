#pragma once

#include <concepts>
#include <iterator>
#include <iostream>
#include <string_view>
#include <source_location>
#include <optional>
#include <variant>
#include <tuple>

namespace trading_api {

template<typename T>
concept is_container = requires(T x) {
    {x.begin()}->std::forward_iterator;
    {x.end()}->std::forward_iterator;
    {x.begin() == x.end()};
};

template<typename T>
concept is_pair_type = requires(T x) {
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
template<typename T>
concept is_variant_type = requires(T x) {
    {x.index()}->std::same_as<std::size_t>;
    {x.valueless_by_exception()}->std::same_as<bool>;
    {std::visit([](auto){}, x)};
};

template<typename T>
concept is_optional_type = requires(const T x) {
    {x.has_value()}->std::same_as<bool>;
    {*x}->std::same_as<const typename T::value_type &>;
};

template<typename T>
concept is_tuple_type = requires(const T x) {
    typename std::tuple_size<T>;
    {std::get<0>(x)};
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

template<typename T>
constexpr bool assert_error = false; 


enum class Side: signed char {
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

template<typename T>
concept BinarySerializable = (
            (std::is_trivially_copy_constructible_v<T> && std::is_standard_layout_v<T>)
            || (std::is_constructible_v<T, std::string_view> && std::is_convertible_v<T, std::string_view>));


template<BinarySerializable T>
std::string_view serialize_binary(const T &data) {
    if constexpr(std::is_constructible_v<T, std::string_view> && std::is_convertible_v<T, std::string_view>) {
        return std::string_view(data);
    } else {
        return std::string_view(reinterpret_cast<const char *>(&data), sizeof(data));
    }
}

template<BinarySerializable T>
std::optional<T> deserialize_binary(std::string_view binary) {
    if constexpr(std::is_constructible_v<T, std::string_view> && std::is_convertible_v<T, std::string_view>) {
        return T(binary);
    } else {
        std::optional<T> ret;
        if (binary.size() == sizeof(T)) {
            ret.emplace(*reinterpret_cast<const T *>(binary.data()));
        }
        return ret;
    }
}

template<BinarySerializable T>
T deserialize_binary(std::string_view binary, const T &defval) {
    if constexpr(std::is_constructible_v<T, std::string_view> && std::is_convertible_v<T, std::string_view>) {
        return T(binary);
    } else if (sizeof(T) != binary.size()) {
        return defval;
    } else {
        return T(*reinterpret_cast<const T *>(binary.data()));
    }
}


}
