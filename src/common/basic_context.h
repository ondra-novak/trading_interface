#pragma once

#include "event_target.h"
#include "basic_order.h"

#include "scheduler.h"
#include "../trading_ifc/strategy.h"
#include "context_scheduler.h"
#include "storage.h"
#include "exchange.h"

#include <mutex>
#include <map>

namespace trading_api {


template<StorageType Storage, ExchangeType Exchange>
class BasicContext: public IContext, public IEventTarget {
public:

    static constexpr Scheduler::EventSeverityID ticker_id = 1;
    static constexpr Scheduler::EventSeverityID orderbook_id = 2;


    BasicContext() = default;
    ~BasicContext();

    void init(std::unique_ptr<IStrategy> strategy,
            IStrategy::InstrumentList instruments,
            ContextScheduler *ctx_sch,
            Storage storage,
            Exchange exchange,
            const Config &config);


    virtual void on_event(OrderFillUpdateCB ordcb) override;
    virtual void on_event(OrderUpdateCB ordcb) override;
    virtual void on_event(Instrument i, const OrderBook &ord) override;
    virtual void on_event(Instrument i) override;
    virtual void on_event(Instrument i, const Ticker &tk) override;
    virtual void on_event(Account a) override;
    virtual void subscribe(SubscriptionType type, const Instrument &i)override;
    virtual Order replace(const Order &order, const Order::Setup &setup, bool amend) override;
    virtual Fills get_fills(std::size_t limit) override;
    virtual Fills get_fills(Timestamp tp) override;
    virtual Order place(const Instrument &instrument, const Order::Setup &setup) override;
    virtual void cancel(const Order &order) override;
    virtual TimerID set_timer(Timestamp at, CompletionCB fnptr) override;
    virtual void unsubscribe(SubscriptionType type, const Instrument &i) override;
    virtual Timestamp now() const override;
    virtual Order bind_order(const Instrument &instrument) override;
    virtual void update_account(const Account &a, CompletionCB complete_ptr) override;
    virtual void allocate(const Account &a, double equity) override;
    virtual bool clear_timer(TimerID id) override;
    virtual Value get_var(int idx) const override;
    virtual void update_instrument(const Instrument &i, CompletionCB complete_ptr) override;
    virtual void set_var(int idx, const Value &val) override;

protected:

    std::mutex _mx;
    std::unique_ptr<IStrategy> _strategy;
    Scheduler _sch;
    ContextScheduler *_global_scheduler = nullptr;
    ContextScheduler::Registration _reg;
    Storage _storage;
    Exchange _exchange;


    Timestamp _cur_time;



    std::mutex _cb_mx;
    std::multimap<Account, CompletionCB> _cb_update_account;
    std::multimap<Instrument, CompletionCB> _cb_update_instrument;
    std::vector<CompletionCB> _cb_tmp;



    void reschedule(Timestamp tp);



};



template<StorageType S, ExchangeType E>
BasicContext<S,E>::~BasicContext() {
    if (_global_scheduler) _global_scheduler->unset(&_reg);
    _exchange.disconnect(this);
}

template<StorageType S, ExchangeType E>
void BasicContext<S,E>::init(std::unique_ptr<IStrategy> strategy,
        IStrategy::InstrumentList instruments,
        ContextScheduler *ctx_sch,
        S storage,
        E exchange,
        const Config &config) {
    strategy = std::move(_strategy);
    _global_scheduler = ctx_sch;
    _reg.wakeup_fn = [this](Timestamp tp){
        std::lock_guard _(_mx);
        _cur_time = tp;
        reschedule(_sch.wakeup(tp, [this](Timestamp tp){
            reschedule(tp);
        }));
    };
    _storage = storage;
    _exchange = exchange;
    auto orders = _storage.get_open_orders();
    _exchange.update_orders(this, {orders.data(), orders.size()});
    _strategy->on_init(Context(this), config, instruments);
}


template<StorageType S, ExchangeType E>
void BasicContext<S,E>::reschedule(Timestamp tp) {
    _global_scheduler->set(&_reg, tp);
}

template<StorageType S, ExchangeType E>
void BasicContext<S,E>::on_event(OrderFillUpdateCB ordcb) {
    _sch.enqueue([this,ordcb = std::move(ordcb)]() mutable {
        auto [order, fill] = ordcb();
        if (_storage.store_fill(fill)) {    //check for duplicate
            _strategy->on_fill(order, fill);
        }
    });
}

template<StorageType S, ExchangeType E>
void BasicContext<S,E>::on_event(OrderUpdateCB ord) {
    _sch.enqueue([this, ord = std::move(ord)]() mutable {
        _strategy->on_order(ord());
    });
}

template<StorageType S, ExchangeType E>
void BasicContext<S,E>::on_event(Instrument i, const OrderBook &ord) {
    _sch.enqueue_collapse(orderbook_id, [this, i, ord = OrderBook(ord)]{
        _strategy->on_orderbook(i, ord);
    });
}

namespace _details {

template<typename MMap, typename Iter, typename Target>
void move_range(MMap &map, std::pair<Iter, Iter> range, std::vector<Target> &out) {
    for (auto iter = range.first; iter != range.second; ++iter) {
        out.push_back(std::move(iter->second));
    }
    map.erase(range.first, range.second);
}

}

template<StorageType S, ExchangeType E>
void BasicContext<S,E>::on_event(Instrument i) {
    _sch.enqueue([this, i]{
        {
            std::lock_guard _(_cb_mx);
            _details::move_range(_cb_update_instrument, _cb_update_instrument.equal_range(i), _cb_tmp);
        }
        for (auto &cbs:_cb_tmp) {
            cbs();
        }
        _cb_tmp.clear();
    });

}

template<StorageType S, ExchangeType E>
void BasicContext<S,E>::on_event(Instrument i, const Ticker &tk) {
    _sch.enqueue_collapse(ticker_id, [this, i, tk = Ticker(tk)]{
        _strategy->on_ticker(i, tk);
    });
}


template<StorageType S, ExchangeType E>
void BasicContext<S,E>::on_event(Account a) {
    _sch.enqueue([this, a]{
        {
            std::lock_guard _(_cb_mx);
            _details::move_range(_cb_update_account, _cb_update_account.equal_range(a), _cb_tmp);
        }
        for (auto &cbs:_cb_tmp) {
            cbs();
        }
        _cb_tmp.clear();
    });
}

template<StorageType S, ExchangeType E>
void BasicContext<S,E>::subscribe(SubscriptionType type, const Instrument &i) {
    _exchange.subscribe(this, type, i);
}

template<StorageType S, ExchangeType E>
Order BasicContext<S,E>::replace(const Order &order, const Order::Setup &setup, bool amend) {
    return _exchange.replace(this, order, setup, amend);
}


template<StorageType S, ExchangeType E>
Fills BasicContext<S,E>::get_fills(std::size_t limit) {
    return _storage.get_fills(limit);
}
template<StorageType S, ExchangeType E>
Fills BasicContext<S,E>::get_fills(Timestamp tp) {
    return _storage.get_fills(tp);
}
template<StorageType S, ExchangeType E>
Order BasicContext<S,E>::place(const Instrument &instrument, const Order::Setup &setup) {
    return _exchange.place(this, instrument, setup);

}

template<StorageType S, ExchangeType E>
void BasicContext<S,E>::cancel(const Order &order) {
    _exchange.cancel(order);
}

template<StorageType S, ExchangeType E>
TimerID BasicContext<S,E>::set_timer(Timestamp at, CompletionCB fnptr) {
    return _sch.enqueue_timed(at, std::move(fnptr));
}

template<StorageType S, ExchangeType E>
void BasicContext<S,E>::unsubscribe(SubscriptionType type, const Instrument &i) {
    _exchange.unsubscribe(this, type, i);
}

template<StorageType S, ExchangeType E>
Timestamp BasicContext<S,E>::now() const {
    return _cur_time;

}

template<StorageType S, ExchangeType E>
Order BasicContext<S,E>::bind_order(const Instrument &instrument) {
    return Order(std::make_shared<AssociatedOrder>(instrument));
}

template<StorageType S, ExchangeType E>
void BasicContext<S,E>::update_account(const Account &a, CompletionCB complete_ptr) {
    bool do_call = false;
    {
        std::lock_guard _(_cb_mx);
        do_call = _cb_update_account.find(a) == _cb_update_account.end();
        _cb_update_account.insert({a, std::move(complete_ptr)});
    }
    if (do_call) {
        _exchange.update_account(this, a);
    }
}

template<StorageType S, ExchangeType E>
void BasicContext<S,E>::allocate(const Account &a, double equity) {
    _exchange.allocate_equity(this, a, equity);
}

template<StorageType S, ExchangeType E>
bool BasicContext<S,E>::clear_timer(TimerID id) {
    return _sch.cancel_timed(id);
}

template<StorageType S, ExchangeType E>
Value BasicContext<S,E>::get_var(int idx) const {
    return _storage.get_var(idx);
}

template<StorageType S, ExchangeType E>
void BasicContext<S,E>::update_instrument(const Instrument &i, CompletionCB complete_ptr) {
    bool do_call = false;
    {
        std::lock_guard _(_cb_mx);
        do_call = _cb_update_instrument.find(i) == _cb_update_instrument.end();
        _cb_update_instrument.insert({i, std::move(complete_ptr)});
    }
    if (do_call) {
        _exchange.update_instrument(this, i);
    }

}

template<StorageType S, ExchangeType E>
void BasicContext<S,E>::set_var(int idx, const Value &val) {
    _storage.set_var(idx, val);
}



}
