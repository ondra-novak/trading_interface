#pragma once

#include "common.h"
#include <memory>

namespace trading_api {


class IService {
public:

    virtual ~IService() = default;
    virtual bool get_service(const std::type_info &tinfo, std::shared_ptr<void> &ptr) = 0;


    template<typename T>
    T get_service() {
        using Handle = decltype(std::declval<T>().get_handle());
        using Element = typename Handle::element_type;
        std::shared_ptr<void> ptr;
        if (!get_service(typeid(Element), ptr)) return T();
        return T(std::reinterpret_pointer_cast<Element>(ptr));
    }

};

}
