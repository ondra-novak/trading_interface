#include "basic_exchange.h"

namespace trading_api {

void AbstractExchange::subscribe(IEventTarget *target, SubscriptionType sbstype, const Instrument &instrument) {
    std::lock_guard _(_mx);
    Subscription s{sbstype, instrument, nullptr};
    auto iter = _subscriptions.lower_bound(s);
    if (iter == _subscriptions.end() || iter->first.type != sbstype || iter->first.i != instrument) {
        this->do_subscribe(sbstype, instrument);
    }
    s.target = target;
    auto r =_subscriptions.emplace(s, SubscriptionLimit::unlimited);
    if (!r.second) r.first->second = SubscriptionLimit::unlimited;
}

void AbstractExchange::unsubscribe(IEventTarget *target, SubscriptionType sbstype, const Instrument &instrument) {
    std::lock_guard _(_mx);
    Subscription s{sbstype, instrument, target};
    _subscriptions.erase(s);
}

void AbstractExchange::income_data(const Instrument &i, const Ticker &t) {
    std::lock_guard _(_mx);
    _tickers[i] = t;
    send_subscription_notify(i, SubscriptionType::ticker);
}

void AbstractExchange::income_data(const Instrument &i, const OrderBook &t) {
    std::lock_guard _(_mx);
    _orderbooks[i] = t;
    send_subscription_notify(i, SubscriptionType::orderbook);
}


void AbstractExchange::send_subscription_notify(const Instrument &i, SubscriptionType type) {
    Subscription s{type, i, nullptr};
    auto iter = _subscriptions.lower_bound(s);
    unsigned int remain = 0;
    while (iter != _subscriptions.end() && iter->first.i == i && iter->first.type == type) {
        s.target = iter->first.target;
        if (iter->second == SubscriptionLimit::onceshot) iter = _subscriptions.erase(iter);
        else ++remain;
        s.target->on_event(s.i, s.type);
    }
    if (remain == 0) this->do_unsubscribe(type, i);
}

bool AbstractExchange::get_last_ticker(const Instrument &instrument, Ticker &tk) {
    std::lock_guard _(_mx);
    auto iter = _tickers.find(instrument);
    if (iter == _tickers.end()) return false;
    tk = iter->second;
    return true;
}

bool AbstractExchange::get_last_orderbook(const Instrument &instrument, OrderBook &ordb) {
    std::lock_guard _(_mx);
    auto iter = _orderbooks.find(instrument);
    if (iter == _orderbooks.end()) return false;
    ordb = iter->second;
    return true;

}

void AbstractExchange::update_ticker(IEventTarget *target, const Instrument &instrument) {
    std::lock_guard _(_mx);
    Subscription s{SubscriptionType::ticker, instrument, nullptr};
    auto iter = _subscriptions.lower_bound(s);
    if (iter == _subscriptions.end() || iter->first.type != SubscriptionType::ticker || iter->first.i != instrument) {
        this->do_subscribe(SubscriptionType::ticker, instrument);
        s.target = target;
        _subscriptions.emplace(s, SubscriptionLimit::onceshot);
    } else {
        target->on_event(instrument, SubscriptionType::ticker);
    }
}

void AbstractExchange::update_account(IEventTarget *target, const Account &account) {
    std::lock_guard _(_mx);
    auto &lst = _account_update_waiting[account];
    if (lst.empty()) {
        do_update_account(account);
    }
    lst.push_back(target);
}

void AbstractExchange::update_instrument(IEventTarget *target, const Instrument &instrument) {
    std::lock_guard _(_mx);
    auto &lst = _instrument_update_waiting[instrument];
    if (lst.empty()) {
        do_update_instrument(instrument);
    }
    lst.push_back(target);
}

void AbstractExchange::object_updated(const Account &account) {
    std::lock_guard _(_mx);
    auto &lst = _account_update_waiting[account];
    for (auto x: lst) {
        x->on_event(account);
    }
    lst.clear();
}

void AbstractExchange::object_updated(const Instrument &instrument) {
    std::lock_guard _(_mx);
    auto &lst = _instrument_update_waiting[instrument];
    for (auto x: lst) {
        x->on_event(instrument);
    }
    lst.clear();
}

void AbstractExchange::disconnect(const IEventTarget *target) {
    std::lock_guard _(_mx);
    for (auto iter = _subscriptions.begin(); iter != _subscriptions.end(); ++iter) {
        if (iter->first.target == target) iter = _subscriptions.erase(iter);
    }
    for (auto &[k,lst]: _account_update_waiting) {
        lst.erase(std::remove(lst.begin(), lst.end(), target), lst.end());
    }
    for (auto &[k,lst]: _instrument_update_waiting) {
        lst.erase(std::remove(lst.begin(), lst.end(), target), lst.end());
    }
}

void AbstractExchange::batch_place(IEventTarget *target, std::span<PBasicOrder> orders) {
    std::lock_guard _(_mx);
    for (const auto &ord: orders) {
        _orders.emplace(ord, target);
    }
    do_batch_place(orders);
}

void AbstractExchange::order_state_changed(const PCBasicOrder &order, Order::State state, Order::Reason reason, std::string message) {
    std::lock_guard _(_mx);
    auto iter= _orders.find(order);
    if (iter != _orders.end()) {
        iter->second->on_event(iter->first, state, reason, message);
        if (IOrder::is_done(state)) {
            _orders.erase(iter);
        }
    }
}

void AbstractExchange::order_fill(const PCBasicOrder &order, const Fill &fill) {
    std::lock_guard _(_mx);
    auto iter= _orders.find(order);
    if (iter != _orders.end()) {
        iter->second->on_event(iter->first, fill);
    }

}

void AbstractExchange::order_restore(IEventTarget *target, const PBasicOrder &order) {
    _orders.emplace(order, target);

}

AbstractExchange *AbstractExchange::from_exchange(Exchange ex) {
    const IExchange *e = ex.get_handle().get();
    const BasicExchange *be = dynamic_cast<const BasicExchange *>(e);
    if (be == nullptr) throw std::runtime_error("Unsupported exchange object");
    return be->get_instance();
}


}
