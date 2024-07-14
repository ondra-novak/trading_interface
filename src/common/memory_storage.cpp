#include "memory_storage.h"

namespace trading_api {

void MemoryStorage::rollback() {
    _transaction.clear();
    _transaction_counter = std::max(_transaction_counter-1,0);
}

void MemoryStorage::begin_transaction() {
    ++_transaction_counter;
}

void MemoryStorage::put_order(const Order &ord) {
    store(TrnPutOrder{ord});
}

void MemoryStorage::erase_var(std::string_view name) {
    store(TrnEraseVar{std::string(name)});
}

void MemoryStorage::put_fill(const Fill &fill) {
    store(TrnPutFill{fill});
}

void MemoryStorage::commit() {
    _transaction_counter = std::max(_transaction_counter-1,0);
    if (!_transaction_counter) {
        for (auto &x: _transaction) {
            std::visit([&](auto &item) {
                item(this);
            }, x);
        }
        _transaction.clear();
    }
}

bool MemoryStorage::is_duplicate_fill(const Fill &fill) const {
    auto iter = _fills.rbegin();
    auto end = _fills.rend();
    while (iter != end && iter->time > fill.time) {
        if (*iter == fill) return true;
        ++iter;
    }
    return false;
}

void MemoryStorage::put_var(std::string_view name, std::string_view value) {
    store(TrnPutVar{std::string(name), std::string(value)});
}

std::vector<SerializedOrder> MemoryStorage::load_open_orders() const {
    std::vector<SerializedOrder> ret;
    for (const auto &[key, value]: _orders) {
        ret.push_back({key, value});
    }
    return ret;
}

Fills MemoryStorage::load_fills(std::size_t limit, std::string_view filter) const {
    Fills ret;
    auto iter = _fills.rbegin();
    auto end = _fills.rend();
    while (ret.size()<limit && iter != end) {
        if (filter.empty()) {
            ret.push_back(*iter);
        } else {
            std::string_view label (iter->label);
            if (label.substr(0, filter.size()) == filter) {
                ret.push_back(*iter);
            }
        }
        ++iter;
    }
    return ret;

}

Fills MemoryStorage::load_fills(Timestamp limit, std::string_view filter) const {
    Fills ret;
    auto iter = _fills.rbegin();
    auto end = _fills.rend();
    while (iter != end && iter->time > limit) {
        if (filter.empty()) {
            ret.push_back(*iter);
        } else {
            std::string_view label (iter->label);
            if (label.substr(0, filter.size()) == filter) {
                ret.push_back(*iter);
            }
        }
        ++iter;
    }
    return ret;
}

void MemoryStorage::enum_vars(std::string_view prefix,
         Function<void(std::string_view, std::string_view)> &fn) const {

}

void MemoryStorage::enum_vars(std::string_view start, std::string_view end,
         Function<void(std::string_view, std::string_view)> &fn) const {
    auto iter = _vars.lower_bound(start);
    auto iend = _vars.upper_bound(end);
    while (iter != iend) {
        fn(iter->first, iter->second);
        ++iter;
    }
}

void MemoryStorage::store(TrnItem item) {
    if (_transaction_counter) {
        _transaction.push_back(std::move(item));
    } else {
        std::visit([&](auto &item) {
                      item(this);
                  }, item);
    }
}

void MemoryStorage::TrnEraseVar::operator ()(MemoryStorage* me) {
    me->_vars.erase(name);
}

void MemoryStorage::TrnPutVar::operator ()(MemoryStorage* me) {
    me->_vars.emplace(std::move(name), std::move(value));
}

void MemoryStorage::TrnPutOrder::operator ()(MemoryStorage* me) {
    auto bin = order.to_binary();
    if (unused(bin)) return;
    me->_orders.emplace(std::move(bin.order_id), std::move(bin.order_content));
}

void MemoryStorage::TrnPutFill::operator ()(MemoryStorage*me) {
    me->_fills.push_back(std::move(fill));
}

std::string MemoryStorage::get_var(std::string_view var_name) const {
}


}
