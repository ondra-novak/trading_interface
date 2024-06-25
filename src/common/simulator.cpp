#include "simulator.h"

namespace trading_api{

std::string SimulatorBackend::SimOrder::gen_id() {
    static std::atomic<std::uint64_t> idcnt = std::chrono::system_clock::now().time_since_epoch().count();
    std::uint64_t id = idcnt++;
    char buff[50];
    std::snprintf(buff, sizeof(buff), "Sim%lX", id);
    return buff;
}

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

PBasicOrder SimulatorBackend::create_order(const Instrument &instrument,const Order::Setup &setup) {
    return create_sim_order(instrument, setup);

}

PBasicOrder SimulatorBackend::create_order_replace(const PCBasicOrder &replace,const Order::Setup &setup, bool amend) {
    auto ord = create_sim_order(replace->get_instrument(), setup);
    ord->replace = replace;
    ord->amend = amend;
    return PBasicOrder(ord);
}

void SimulatorBackend::batch_cancel(std::span<PCBasicOrder> orders) {
    std::lock_guard _(_mx);
    for (const auto &o: orders) {
        auto iter = _instruments.find(o->get_instrument());
        if (iter != _instruments.end()) {
            InstrumentState &st = iter->second;
            auto i2 = std::find_if(st._active_orders.begin(), st._active_orders.end(),
                    [&](const PendingOrder &x) {return x._order == o;});
            if (i2 != st._active_orders.end()) {
                i2->_target->on_event(i2->_order, Order::State::canceled);
                st._active_orders.erase(i2);
            }
        }
    }
}

std::shared_ptr<SimulatorBackend::SimOrder> SimulatorBackend::create_sim_order(const Instrument &i, const Order::Setup &stp) {
    auto ord = std::make_shared<SimOrder>(i, stp, Order::Origin::strategy);
    if (std::holds_alternative<Order::Transfer>(stp)) {
        ord->set_state(Order::State::discarded, Order::Reason::unsupported, {});
    }
    return ord;
}

void SimulatorBackend::batch_place(IEventTarget *target, std::span<PBasicOrder> orders) {
    std::lock_guard _(_mx);
    for (const PBasicOrder &o: orders) {
        if (o->get_state() == Order::State::sent) {
            const SimOrder *simord = dynamic_cast<const SimOrder *>(o.get());
            if (simord) {
                const auto &new_setup = simord->get_setup();

                Instrument instr = simord->get_instrument();
                InstrumentState &ist = _instruments[instr];
                if (simord->replace) {
                    auto ir = std::find_if(ist._active_orders.begin(), ist._active_orders.end(),
                            [&](const PendingOrder &p){return p._order == o;});
                    if (ir != ist._active_orders.end()) {
                        if (simord->amend) {
                            const auto &cur_setup = ir->_order->get_setup();
                            if (new_setup.index() == cur_setup.index()
                                    && Order::get_side(new_setup) == Order::get_side(cur_setup)) {
                                ir->_target->on_event(ir->_order,Order::State::canceled);
                                o->add_fill(ir->_last_fill_price, ir->_filled);
                                if (ir->_filled >= Order::get_total(new_setup)) {
                                    ir->_target->on_event(o,Order::State::filled);
                                    ist._active_orders.erase(ir);
                                } else {
                                    double f = ir->_filled;
                                    MarketExecState mst = try_market_order(o, f);
                                    if (mst.final_state == Order::State::pending) {
                                        ir->_filled = mst.total_filled;
                                        ir->_last_fill_price = mst.last_fill_price;
                                        ir->_order = o;
                                        ir->_target = target;
                                    } else {
                                        ist._active_orders.erase(ir);
                                    }
                                    if (mst.total_filled > f) {
                                        send_fill(o, mst.side, mst.last_fill_price, mst.total_filled-f);
                                    }
                                    target->on_event(o,mst.final_state, mst.reject_reason);
                                }
                                continue;
                            } else {
                                target->on_event(o,Order::State::rejected, Order::Reason::invalid_amend);
                                continue;
                            }
                        } else {
                            const Order::Options *x = Order::get_options(new_setup);
                            if (x && x->replace_filled_constrain) {
                                if (ir->_filled > x->replace_filled_constrain) {
                                    target->on_event(o, Order::State::rejected, Order::Reason::replace_unprocessed_fill);
                                    continue;
                                }
                            }
                            ir->_target->on_event(o,Order::State::canceled);
                            ist._active_orders.erase(ir);
                        }
                    } else {
                        target->on_event(o, Order::State::rejected,
                                Order::Reason::not_found, {});
                        continue;
                    }
                }
                //place order now
                MarketExecState mst = try_market_order(o, 0);
                if (mst.total_filled) {
                    send_fill(o, mst.side, mst.last_fill_price, mst.total_filled);
                }
                if (mst.final_state == Order::State::pending) {
                    ist._active_orders.push_back({
                        o,target,mst.total_filled, mst.last_fill_price
                    });
                }
                target->on_event(o,mst.final_state, mst.reject_reason);
            } else {
                target->on_event(o, Order::State::rejected, Order::Reason::unsupported,{});
            }
        }
    }
}

std::string SimulatorBackend::SimOrder::get_id() const {
    return id;
}

std::pair<double, double> SimulatorBackend::ticker_info(const Instrument &instrument, Side side) {
    InstrumentState &st = _instruments[instrument];
    if (st._last_ticker.has_value()) {
        switch (side) {
            case Side::buy: return {st._last_ticker->ask, st._last_ticker->ask_volume};
            case Side::sell: return {st._last_ticker->bid, st._last_ticker->bid_volume};
            default: return {0.0,0.0};
        }
    } else {
        return {0.0,0.0};
    }
}

SimulatorBackend::MarketExecState SimulatorBackend::try_market_order(const PBasicOrder &ord, double already_filled) {
    const Order::Setup &setup = ord->get_setup();
    return std::visit([&](const auto &info){
        return try_market_order_spec(ord->get_instrument(), info, already_filled);
    }, setup);
}

SimulatorBackend::MarketExecState SimulatorBackend::try_market_order_spec(
        const Instrument &, const IOrder::Undefined&,
        double ) {
    return {Order::State::rejected, Order::Reason::unsupported};
}

SimulatorBackend::MarketExecState SimulatorBackend::try_market_order_spec(
        const Instrument &, const IOrder::Transfer&,
        double ) {
    return {Order::State::rejected, Order::Reason::unsupported};
}

SimulatorBackend::MarketExecState SimulatorBackend::try_market_order_spec(
        const Instrument &, const Order::Stop&,
        double ) {
    return {Order::State::pending};
}

SimulatorBackend::MarketExecState SimulatorBackend::try_market_order_spec(
        const Instrument &, const Order::TrailingStop&,
        double ) {
    return {Order::State::pending};
}

SimulatorBackend::MarketExecState SimulatorBackend::try_market_order_spec(
        const Instrument &instrument, const Order::ClosePosition &order,
        double ) {
    Account acc = instrument.get_account();
    Account::Position pos = acc.get_position_by_id(instrument, order.pos_id);
    if (pos.side == Side::undefined) return {Order::State::rejected, Order::Reason::not_found};
    //TODO:: update account - remove position
    auto tinfo = ticker_info(instrument, reverse(pos.side));
    if (tinfo.second) {
        return {Order::State::filled, Order::Reason::no_reason, pos.amount, tinfo.first, pos.side};
    } else {
        return {Order::State::rejected, Order::Reason::low_liquidity};
    }
}

SimulatorBackend::MarketExecState SimulatorBackend::try_market_order_spec(
        const Instrument &instrument, const Order::Market &order,
        double already_filled) {
    auto tinfo = ticker_info(instrument, order.side);
    if (tinfo.second) {
        double amount;
        if (order.options.amount_is_volume) amount = order.amount/tinfo.first;
        else amount = order.amount;
        amount -= already_filled;
        if (amount > 0) {
            //todo pdate account, add or remove position (hedge)
        }
        return {Order::State::filled, Order::Reason::no_reason, amount+already_filled, tinfo.first, order.side};
    }
    return {Order::State::rejected, Order::Reason::low_liquidity};


}

SimulatorBackend::MarketExecState SimulatorBackend::try_market_order_spec(
        const Instrument &instrument, const Order::ImmediateOrCancel &order,
        double already_filled) {
    auto tinfo = ticker_info(instrument, order.side);
    double amount;
    if (order.options.amount_is_volume) amount = order.amount/tinfo.first;
    else amount = order.amount;
    amount -= already_filled;
    amount = std::max(amount, tinfo.second);
    if (amount > 0) {
        //todo pdate account, add or remove position (hedge)
    }
    return {amount + already_filled >= order.amount?Order::State::filled:Order::State::canceled,
                Order::Reason::no_reason, amount+already_filled, tinfo.first, order.side};

}

SimulatorBackend::MarketExecState SimulatorBackend::try_market_order_spec(
        const Instrument &instrument, const Order::LimitPostOnly &order,
        double ) {
    auto tinfo = ticker_info(instrument, order.side);
    if ((order.side == Side::buy && order.limit_price > tinfo.first)
            || (order.side == Side::sell && order.limit_price < tinfo.first)) {
        return {Order::State::rejected, Order::Reason::crossing};
    }
    return {Order::State::pending};
}

SimulatorBackend::MarketExecState SimulatorBackend::try_market_order_spec(
        const Instrument &instrument, const Order::Limit &order,
        double already_filled) {
    auto tinfo = ticker_info(instrument, order.side);

    double amount;
    if (order.options.amount_is_volume) amount = order.amount/tinfo.first;
    else amount = order.amount;
    amount -= already_filled;
    amount = std::max(amount, tinfo.second);

    if ((order.side == Side::buy && order.limit_price > tinfo.first)
            || (order.side == Side::sell && order.limit_price < tinfo.first)) {
        //todo record execution
        return {Order::State::pending, Order::Reason::no_reason, tinfo.first, amount, order.side};
    }
    return {Order::State::pending};
}

}

