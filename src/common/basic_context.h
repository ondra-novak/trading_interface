#pragma once

#include "event_target.h"
#include "basic_order.h"

#include "scheduler.h"
#include "../trading_ifc/strategy.h"
#include "context_scheduler.h"
#include "storage.h"
#include "exchange.h"

#include "basic_exchange.h"
#include <mutex>
#include <map>
#include <set>

namespace trading_api {


template<StorageType Storage>
class BasicContext: public IContext, public IEventTarget {
public:

    static constexpr Scheduler::EventSeverityID ticker_id = 1;
    static constexpr Scheduler::EventSeverityID orderbook_id = 2;

    virtual void unset_var(std::string_view var_name) override;
    virtual void set_var(std::string_view var_name, std::string_view value)
            override;

    BasicContext() = default;
    ~BasicContext();

    using Configuration = IStrategy::Configuration;

    void init(std::unique_ptr<IStrategy> strategy,
            const Configuration &config,
            ContextScheduler *ctx_sch,
            Storage storage);


    virtual void on_event(const Instrument &i) override;
    virtual void on_event(const Account &a) override;
    virtual void on_event(const Instrument &i, const Ticker &tk) override;
    virtual void on_event(const Instrument &i, const OrderBook &ord) override;
    virtual void on_event(const PBasicOrder &order, Order::State state) override;
    virtual void on_event(const PBasicOrder &order, Order::State state, Order::Reason reason, const std::string &message) override;
    virtual void on_event(const PBasicOrder &order, const Fill &fill) override;
    virtual void subscribe(SubscriptionType type, const Instrument &i)override;
    virtual Order replace(const Order &order, const Order::Setup &setup, bool amend) override;
    virtual Fills get_fills(const Instrument &i, std::size_t limit) const override ;
    virtual Fills get_fills(const Instrument &i, Timestamp tp) const override ;
    virtual Order place(const Instrument &instrument, const Account &account,  const Order::Setup &setup) override;
    virtual void cancel(const Order &order) override;
    virtual void set_timer(Timestamp at, CompletionCB fnptr, TimerID id) override;
    virtual void unsubscribe(SubscriptionType type, const Instrument &i) override;
    virtual Timestamp now() const override;
    virtual Order bind_order(const Instrument &instrument) override;
    virtual void update_account(const Account &a, CompletionCB complete_ptr) override;
    virtual void allocate(const Account &a, double equity) override;
    virtual bool clear_timer(TimerID id) override;
    virtual void update_instrument(const Instrument &i, CompletionCB complete_ptr) override;

protected:

    class LocalScheduler : public Scheduler{
    public:

        using Scheduler::Scheduler;


        template<typename X>
        struct Rec {
            X _data;
            Function<void(const Instrument &, const X &)> _fn;
        };
        using TickerEv = std::map<Instrument, Rec<Ticker> >;
        using OrderBookEv = std::map<Instrument, Rec<OrderBook> >;

        template<typename X, typename Fn>
        void enqueue_update(const Instrument &i, const X &update, Fn &&fn) {
            std::lock_guard _(_mx);
            using Ev = std::map<Instrument, Rec<X> >;
            auto &events = std::get<Ev>(_events);
            auto iter = events.find(i);
            if (iter == events.end()) {
                auto ins = events.emplace(i, Rec<X>{update, std::forward<Fn>(fn)});
                const Instrument &ilb = ins.first->first;
                enqueue_lk([this, &ilb]{
                    std::unique_lock lk(_mx);
                    auto &events = std::get<Ev>(_events);
                    auto iter = events.find(ilb);
                    if (iter != events.end()) {
                        Instrument i = iter->first;
                        Rec<X> rc = std::move(iter->second);
                        events.erase(iter);
                        lk.unlock();
                        rc._fn(i, rc._data);
                    }
                });
            } else {
                iter->second._data = update;
                iter->second._fn = std::forward<Fn>(fn);
            }
        }

    protected:
        std::tuple<TickerEv, OrderBookEv> _events;
    };

    std::mutex _mx;
    std::unique_ptr<IStrategy> _strategy;
    LocalScheduler _sch;
    ContextScheduler *_global_scheduler = nullptr;
    ContextScheduler::Registration _reg;
    Storage _storage;


    Timestamp _cur_time;



    std::mutex _cb_mx;
    std::multimap<Account, CompletionCB> _cb_update_account;
    std::multimap<Instrument, CompletionCB> _cb_update_instrument;
    std::vector<CompletionCB> _cb_tmp;

    std::vector<PBasicOrder> _batch_place;
    std::vector<PCBasicOrder> _batch_cancel;
    std::set<Exchange> _exchanges;
    std::optional<typename Storage::Transaction> _trn;


    void reschedule(Timestamp tp);
    void flush_batches();

    typename Storage::Transaction &get_transaction() {
        if (!_trn.has_value()) {
            _trn = _storage.begin_transaction();
        }
        return *_trn;
    }

};



template<StorageType S>
BasicContext<S>::~BasicContext() {
    if (_global_scheduler) _global_scheduler->unset(&_reg);
    for (const auto &e: _exchanges) {
        AbstractExchange::from_exchange(e)->disconnect(this);
    }
}

template<StorageType S>
void BasicContext<S>::init(std::unique_ptr<IStrategy> strategy,
        const Configuration &config,
        ContextScheduler *ctx_sch,
        S storage) {
    strategy = std::move(_strategy);
    _global_scheduler = ctx_sch;
    _reg.wakeup_fn = [this](Timestamp tp){
        std::lock_guard _(_mx);
        _cur_time = tp;
        reschedule(_sch.wakeup(tp, [this](Timestamp tp){
            reschedule(tp);
        }));
        flush_batches();
    };
    _storage = storage;
    for (const auto &a: config.accounts) {
        _exchanges.emplace(a.get_exchange());
    }
    for (const auto &i: config.instruments) {
        _exchanges.emplace(i.get_exchange());
    }
    _strategy->on_init(this, config);
    auto orders = _storage.get_open_orders();
    for (auto &e: _exchanges) {
        AbstractExchange::from_exchange(e)->restore_orders(this, {orders.data(), orders.size()});
    }
}


template<StorageType S>
void BasicContext<S>::reschedule(Timestamp tp) {
    _global_scheduler->set(&_reg, tp);
}

template<StorageType S>
void BasicContext<S>::on_event(const PBasicOrder &order, const Fill &fill) {
    _sch.enqueue([this,order = PBasicOrder(order), fill = Fill(fill)]() mutable {
        if (_storage.is_duplicate(fill)) return;
        auto &trn = get_transaction();
        trn.put(fill);
        order->add_fill(fill.price, fill.amount);
        _strategy->on_fill(Order(order), std::move(fill));
    });
}

template<StorageType S>
void BasicContext<S>::on_event(const PBasicOrder &order, Order::State state) {
    _sch.enqueue([this, order = PBasicOrder(order), state]() mutable {
        order->set_state(state);
        _strategy->on_order(Order(order));
    });
}

template<StorageType S>
void BasicContext<S>::on_event(const PBasicOrder &order, Order::State state, Order::Reason reason, const std::string &message) {
    _sch.enqueue([this, order = PBasicOrder(order), state, reason, message = std::string(message)]() mutable {
        order->set_state(state,reason,message);
        _strategy->on_order(Order(order));
    });
}

template<StorageType S>
void BasicContext<S>::on_event(const Instrument &i, const OrderBook &ord) {
    _sch.enqueue_update(i, ord, [this](const Instrument &i, const OrderBook &ord) {
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

template<StorageType S>
void BasicContext<S>::on_event(const Instrument &i) {
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

template<StorageType S>
void BasicContext<S>::on_event(const Instrument &i, const Ticker &tk) {
    _sch.enqueue_update(i, tk, [this](const Instrument &i, const Ticker &tk){
        _strategy->on_ticker(i, tk);
    });
}


template<StorageType S>
void BasicContext<S>::on_event(const Account &a) {
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

template<StorageType S>
void BasicContext<S>::subscribe(SubscriptionType type, const Instrument &i) {
    _exchange.subscribe(this, type, i);
}



template<StorageType S>
Fills BasicContext<S>::get_fills(const Instrument &i, std::size_t limit) const {
    return _storage.get_fills(i, limit);
}
template<StorageType S>
Fills BasicContext<S>::get_fills(const Instrument &i,Timestamp tp) const {
    return _storage.get_fills(i, tp);
}
template<StorageType S>
Order BasicContext<S>::place(const Instrument &instrument, const Account &account, const Order::Setup &setup) {
    Exchange exch = instrument.get_exchange();
    if (exch != account.get_exchange()) {
        return Order(std::make_shared<ErrorOrder>(
                instrument, account, IOrder::Reason::invalid_params
                ));
    }

    auto ord = _exchange.create_order(instrument, setup);
    if (ord->get_state() != Order::State::discarded) {
        _batch_place.push_back(ord);
    }
    return Order(ord);
}

template<StorageType S>
Order BasicContext<S>::replace(const Order &order, const Order::Setup &setup, bool amend) {
    auto ordptr = std::dynamic_pointer_cast<const BasicOrder>(order.get_handle());
    if (ordptr) {
        auto ord = _exchange.create_order_replace(ordptr, setup, amend);
        if (ord->get_state() != Order::State::discarded) {
            _batch_place.push_back(ord);
        }
        return Order(ord);
    } else{
        auto ordptr = std::dynamic_pointer_cast<const AssociatedOrder>(order.get_handle());
        if (ordptr) {
            return place(ordptr->get_instrument(), setup);
        } else {
            return Order(std::make_shared<ErrorOrder>(order.get_instrument(), Order::Reason::incompatible_order, ""));
        }
    }
}


template<StorageType S>
void BasicContext<S>::cancel(const Order &order) {
    auto ordptr = std::dynamic_pointer_cast<const BasicOrder>(order.get_handle());
    if (ordptr) {
        _batch_cancel.push_back(ordptr);
    }
}

template<StorageType S>
void BasicContext<S>::set_timer(Timestamp at, CompletionCB fnptr, TimerID id) {
    if (!fnptr) fnptr = [this, id]{
        _strategy->on_timer(id);
    };
    return _sch.enqueue(at, std::move(fnptr), id);
}

template<StorageType S>
void BasicContext<S>::unsubscribe(SubscriptionType type, const Instrument &i) {
    _exchange.unsubscribe(this, type, i);
}

template<StorageType S>
Timestamp BasicContext<S>::now() const {
    return _cur_time;

}

template<StorageType S>
Order BasicContext<S>::bind_order(const Instrument &instrument) {
    return Order(std::make_shared<AssociatedOrder>(instrument));
}

template<StorageType S>
void BasicContext<S>::update_account(const Account &a, CompletionCB complete_ptr) {
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

template<StorageType S>
void BasicContext<S>::allocate(const Account &a, double equity) {
    _exchange.allocate_equity(this, a, equity);
}

template<StorageType S>
bool BasicContext<S>::clear_timer(TimerID id) {
    return _sch.cancel_timed(id);
}


template<StorageType S>
void BasicContext<S>::update_instrument(const Instrument &i, CompletionCB complete_ptr) {
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

template<StorageType S>
void BasicContext<S>::flush_batches() {
    if (!_batch_cancel.empty()) {
        _exchange.batch_cancel({_batch_cancel.begin(), _batch_cancel.end()});
    }
    if (!_batch_place.empty()) {
        _exchange.batch_place(this, {_batch_place.begin(), _batch_place.end()});
    }
    _batch_cancel.clear();
    _batch_place.clear();
    if (_trn.has_value()) {
        _storage.commit(*_trn);
        _trn.reset();
    }
}



template<StorageType Storage>
inline void BasicContext<Storage>::unset_var(std::string_view var_name) {
    get_transaction().erase(var_name);
}

template<StorageType Storage>
inline void BasicContext<Storage>::set_var(std::string_view var_name, std::string_view value) {
    get_transaction().put(var_name, value);
}

}
