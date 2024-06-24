#include "simulator.h"

namespace trading_api{

void SimulatorBackend::disconnect(IEventTarget *target) {
    std::lock_guard _(_mx);
    for (auto &x: _instruments) {
        x.second.disconnect(target);
    }
}

void SimulatorBackend::update_orders(IEventTarget *, std::span<SerializedOrder> ) {
    //not simulated
}

void SimulatorBackend::subscribe(IEventTarget *target, SubscriptionType sbstype, const Instrument &instrument) {
    std::lock_guard _(_mx);
    InstrumentState &st = _instruments[instrument];
    std::vector<IEventTarget *> *t;
    switch (sbstype) {
        default: return;
        case SubscriptionType::ticker: t = &st._subscriptions_ticker;break;
        case SubscriptionType::orderbook: t = &st._subscriptions_orderbook;break;
    }
    if (std::find(t->begin(), t->end(), target) != t->end()) return;
    t->push_back(target);
    if (st._last_ticker.has_value()) target->on_event(instrument, *st._last_ticker);
}

void SimulatorBackend::unsubscribe(IEventTarget *target, SubscriptionType sbstype, const Instrument &instrument) {
    std::lock_guard _(_mx);
    InstrumentState &st = _instruments[instrument];
    std::vector<IEventTarget *> *t;
    switch (sbstype) {
        default: return;
        case SubscriptionType::ticker: t = &st._subscriptions_ticker;break;
        case SubscriptionType::orderbook: t = &st._subscriptions_orderbook;break;
    }
    t->erase(t->begin(),std::remove(t->begin(),t->end(), target));

}

Order SimulatorBackend::place(IEventTarget *target, const Instrument &instrument, const Order::Setup &order_setup) {
    auto ord = std::make_shared<SimOrder>(order_setup, instrument, Order::Origin::strategy);
    if (std::holds_alternative<IOrder::Transfer>(order_setup)
            || std::holds_alternative<IOrder::Undefined>(order_setup)) {
        ord->set_state(Order::State::discarded, Order::Reason::unsupported, {});
        return Order(ord);
    } else {
        target->on_event(OrderUpdateCB([ord]{return Order(ord);}));//post `sent` state
        std::visit(

    }

}

}
