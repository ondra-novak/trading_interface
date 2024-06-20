#pragma once

#include "../trading_ifc/strategy_context.h"

namespace trading_api {

class IStorage {
public:
    virtual ~IStorage();

    virtual void store_fill(const Fill &fill) = 0;
    virtual Fills get_fills(std::size_t limit)  = 0;
    virtual Value get_var(int idx) const = 0;
    virtual void set_var(int idx, const Value &val) = 0;

};

}
