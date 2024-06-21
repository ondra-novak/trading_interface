#pragma once

#include "../trading_ifc/strategy_context.h"
#include <unordered_set>
#include <list>

namespace trading_api {


///Concept describes interface for storage
/**
 * See MemoryStorage for reference implementation and description of each function
 */
template<typename T>
concept StorageType = requires(T v, const T cv, const Fill &fill,
        std::size_t limit, int idx, const Value &val,
        const Order &o) {
    {v.store_fill(fill)}->std::same_as<bool>;
    {cv.get_fills(limit)}->std::same_as<Fills>;
    {cv.get_var(idx)}->std::same_as<Value>;
    {v.set_var(idx, val)};
    {v.store_order(o)};
    {cv.get_open_orders()}->std::same_as<std::vector<SerializedOrder> >;
};

class MemoryStorage {
public:
    ///Store recorded fill
    /**
     * @param f fill structure
     * @retval true fill recorded
     * @retval false fill duplicated (already recorded)
     *
     * @note storage must check whether fill is duplicate or not. If
     * fill is duplicate, it must not be stored and such situation is
     * indicated in return value
     *
     * @note storage must store fills ordered by time. It can happen
     * that fills come not in perfectly ordered (because it is asynchronous
     * operation, race can happen)
     *
     */
    bool store_fill(const Fill &f) {
        auto iter = _fills.begin();
        while (iter != _fills.end() && iter->time >= f.time) {
            if (*iter == f) return false;
            ++iter;
        }
        _fills.insert(iter, f);
        return true;
    }

    ///Retrieve recent fills
    /**
     * @param sz count of recent fills
     * @return fills in vector ordered in increasing timestamp
     */
    Fills get_fills(std::size_t sz) const {
        Fills out;
        out.reserve(sz);
        auto iter = _fills.begin();
        for (std::size_t i = 0; i < sz && iter !=_fills.end() ; ++i, ++iter);
        while (iter != _fills.begin()) {
            --iter;
            out.push_back(*iter);
        }
        return out;
    }

    ///Retrieve recent fills up to given timestamp
    /**
     * @param tp timestamp (last seen)
     * @return fills in vector ordered in increasing timestamp
     */
    Fills get_fills(Timestamp tp) const {
        Fills out;
        auto iter = _fills.begin();
        while (iter != _fills.end() && iter->time > tp) {
            --iter;
        }
        while (iter != _fills.begin()) {
            --iter;
            out.push_back(*iter);
        }
        return out;
    }
    ///Retrieve permanent variable's value
    Value get_var(int idx) const {
        auto iter = _variables.find(idx);
        if (iter == _variables.end()) return {};
        else return iter->second;
    }
    ///Store value permanently
    void set_var(int idx, const Value &val) {
        auto iter = _variables.find(idx);
        if (iter == _variables.end()) {
            _variables.emplace(idx, val);
        } else {
            iter->second = val;
        }
    }

    ///Store order state
    /**
     * @param o order to store
     */
    void store_order(const Order &o) {
        SerializedOrder sr = o.to_binary();
        if (o.done()) _open_orders.erase(sr);
        _open_orders.insert(sr);
    }

    ///Retrieve all open orders (open known when they were stored)
    std::vector<SerializedOrder> get_open_orders() const {
        return {_open_orders.begin(), _open_orders.end()};
    }

protected:
    std::unordered_map<int, Value> _variables;
    std::unordered_set<SerializedOrder> _open_orders;
    std::list<Fill> _fills;

};

static_assert(StorageType<MemoryStorage>);

}
