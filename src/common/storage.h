#pragma once

#include "../trading_ifc/strategy_context.h"
#include <unordered_set>
#include <list>

namespace trading_api {


template<typename T>
concept StorageTransaction = requires(T trn, std::string_view key, std::string_view value,
        const Order &o, const Fill &f) {
    {trn.put(key,value)};
    {trn.erase(key)};
    {trn.put(o)};
    {trn.put(f)};
};

///Concept describes interface for storage
/**
 * See MemoryStorage for reference implementation and description of each function
 */
template<typename T>
concept StorageType = requires(T v, const T cv,
        const typename T::Transaction &transaction,
        std::size_t limit,
        const Fill &fill,
        const Instrument &i,
        Timestamp tp
) {
    requires StorageTransaction<typename T::Transaction>;
    {v.begin_transaction()}->StorageTransaction;
    {v.commit(transaction)};
    {cv.is_duplicate(fill)} ->std::same_as<bool>;
    {cv.get_fills(i,limit)}->std::same_as<Fills>;
    {cv.get_fills(i,tp)}->std::same_as<Fills>;
    {cv.get_variables()}->std::same_as<Variables>;
    {cv.get_open_orders()}->std::same_as<std::vector<SerializedOrder> >;
};

class MemoryStorage {
public:

    //fake transaction
    struct Transaction {
        MemoryStorage *stor;
        void put(std::string_view key, std::string_view value) {
            stor->_variables[std::string(key)] = std::string(value);
        }
        void erase(std::string_view key) {
            stor->_variables.erase(std::string(key));
        }
        void put(const Order &o) {
            SerializedOrder sr = o.to_binary();
            if (o.done()) stor->_open_orders.erase(sr);
            stor->_open_orders.insert(sr);

        }
        void put(const Fill &f) {
            auto &fills = stor->_fills[f.instrument_id];
            auto iter = fills.begin();
            while (iter != fills.end() && iter->time >= f.time) {
                if (*iter == f) return ;
                ++iter;
            }
            fills.insert(iter, f);
        }
    };

    Transaction begin_transaction() {
        return {this};
    }

    void commit(const Transaction &) {}

    bool is_duplicate(const Fill &f) const {
        auto fiter = _fills.find(f.instrument_id);
        if (fiter != _fills.end()) {
            const auto &fills = fiter->second;
            auto iter = fills.begin();
            while (iter != fills.end() && iter->time >= f.time) {
                if (*iter == f) return true;
                ++iter;
            }
        }
        return false;
    }

    ///Retrieve recent fills
    /**
     * @param sz count of recent fills
     * @return fills in vector ordered in increasing timestamp
     */
    Fills get_fills(const Instrument &i, std::size_t sz) const {
        Fills out;
        out.reserve(sz);
        auto fiter = _fills.find(i.get_id());
        if (fiter != _fills.end()) {
            const auto &fills = fiter->second;
            auto iter = fills.begin();
            for (std::size_t i = 0; i < sz && iter !=fills.end() ; ++i, ++iter);
            while (iter != fills.begin()) {
                --iter;
                out.push_back(*iter);
            }
        }
        return out;
    }

    ///Retrieve recent fills up to given timestamp
    /**
     * @param tp timestamp (last seen)
     * @return fills in vector ordered in increasing timestamp
     */
    Fills get_fills(const Instrument &i, Timestamp tp) const {
        Fills out;
        auto fiter = _fills.find(i.get_id());
        if (fiter != _fills.end()) {
            const auto &fills = fiter->second;
            auto iter = fills.begin();
            while (iter != fills.end() && iter->time > tp) {
                --iter;
            }
            while (iter != fills.begin()) {
                --iter;
                out.push_back(*iter);
            }
        }
        return out;
    }


    Variables get_variables() const {
        return {{_variables.begin(), _variables.end()}};
    }

    ///Retrieve all open orders (open known when they were stored)
    std::vector<SerializedOrder> get_open_orders() const {
        return {_open_orders.begin(), _open_orders.end()};
    }

protected:
    std::unordered_map<std::string, std::string> _variables;
    std::unordered_set<SerializedOrder> _open_orders;
    std::unordered_map<std::string, std::list<Fill> >_fills;

};

static_assert(StorageType<MemoryStorage>);

}
