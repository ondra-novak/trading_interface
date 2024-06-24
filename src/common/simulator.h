#pragma once

#include "exchange.h"

#include "basic_order.h"
#include <map>
#include <set>

namespace trading_api {


class SimulatorBackend {
public:

    class SimOrder: public BasicOrder {
    public:
        using BasicOrder::BasicOrder;
        virtual SerializedOrder to_binary() const override {return {};}
    };

    //frontend
    void disconnect(IEventTarget *target);
    void update_orders(IEventTarget *target, std::span<SerializedOrder> orders);
    void subscribe(IEventTarget *target, SubscriptionType sbstype, const Instrument &instrument);
    void unsubscribe(IEventTarget *target, SubscriptionType sbstype, const Instrument &instrument);
    Order place(IEventTarget *target, const Instrument &instrument, const Order::Setup &order_setup);
    Order replace(IEventTarget *target, const Order &order, const Order::Setup &order_setup, bool amend);
    void cancel(const Order &order);
    void update_ticker(IEventTarget *target, const Instrument &instrument);
    void update_account(IEventTarget *target, const Account &account);
    void update_instrument(IEventTarget *target, const Instrument &instrument);
    void allocate_equity(IEventTarget *target, const Account &account, double equity);

    //backend

protected:
    std::mutex _mx;

    struct PendingOrder {
        std::shared_ptr<SimOrder> _order;
        IEventTarget *_target;
        double _filled = 0;

    };

    struct InstrumentState {
        std::vector<PendingOrder> _active_orders;
        std::optional<Ticker> _last_ticker;
        OrderBook _orderbook;
        std::vector<IEventTarget *> _subscriptions_ticker;
        std::vector<IEventTarget *> _subscriptions_orderbook;

        void disconnect(IEventTarget *target);
    };

    std::unordered_map<Instrument, InstrumentState, Instrument::Hasher> _instruments;

};


class Simulator {
public:

    Simulator() = default;
    Simulator(std::shared_ptr<SimulatorBackend> ptr):_ptr(ptr) {}
    void disconnect(IEventTarget *target) {
        _ptr->disconnect(target);
    }
    void update_orders(IEventTarget *target, std::span<SerializedOrder> orders) {
        _ptr->update_orders(target, orders);
    }
    void subscribe(IEventTarget *target, SubscriptionType sbstype, const Instrument &instrument) {
        _ptr->subscribe(target, sbstype, instrument);
    }
    void unsubscribe(IEventTarget *target, SubscriptionType sbstype, const Instrument &instrument) {
        _ptr->unsubscribe(target, sbstype, instrument);
    }
    Order place(IEventTarget *target, const Instrument &instrument, const Order::Setup &order_setup) {
        return _ptr->place(target, instrument, order_setup);
    }
    Order replace(IEventTarget *target, const Order &order, const Order::Setup &order_setup, bool amend) {
        return _ptr->replace(target, order, order_setup, amend);
    }
    void cancel(const Order &order) {
        _ptr->cancel(order);
    }
    void update_ticker(IEventTarget *target, const Instrument &instrument) {
        _ptr->update_ticker(target, instrument);
    }
    void update_account(IEventTarget *target, const Account &account) {
        _ptr->update_account(target, account);
    }
    void update_instrument(IEventTarget *target, const Instrument &instrument) {
        _ptr->update_instrument(target, instrument);
    }
    void allocate_equity(IEventTarget *target, const Account &account, double equity) {
        _ptr->allocate_equity(target, account, equity);
    }


protected:
    std::shared_ptr<SimulatorBackend> _ptr;
};

static_assert(ExchangeType<Simulator>);

}
