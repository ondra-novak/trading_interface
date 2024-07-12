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


/*scheduler object acts as invocable, which receives timestamp, function to call,
 * and pointer which serves as identification.
 *
 * If function is called, and there is already scheduled action, it
 * reschedules the action to new time point
 *
 */

template<typename Scheduler>
concept SchedulerType = (std::is_invocable_v<Scheduler, Timestamp, Function<void(Timestamp)>, const void *>);


template<StorageType Storage, SchedulerType Scheduler>
class BasicContext: public IContext, public IEventTarget {
public:

    BasicContext(Storage storage, Scheduler scheduler)
        :_scheduler(std::move(scheduler)), _storage(std::move(storage)) {

    }

    BasicContext(const BasicContext &) = delete;
    BasicContext &operator=(const BasicContext &) = delete;

    ~BasicContext();

    using Configuration = IStrategy::Configuration;

    void init(std::unique_ptr<IStrategy> strategy, const Configuration &config);


    virtual void on_event(const Instrument &i) override;
    virtual void on_event(const Account &a) override;
    virtual void on_event(const Instrument &i, SubscriptionType subscription_type) override;
    virtual void on_event(const Order &order, const Order::Report &report) override;
    virtual void on_event(const Order &order, const Fill &fill) override;
    virtual void subscribe(SubscriptionType type, const Instrument &i)override;
    virtual Order replace(const Order &order, const Order::Setup &setup, bool amend) override;
    virtual Fills get_fills(const Instrument &i, std::size_t limit) const override ;
    virtual Fills get_fills(const Instrument &i, Timestamp tp) const override ;
    virtual Order place(const Instrument &instrument, const Account &account,  const Order::Setup &setup) override;
    virtual void cancel(const Order &order) override;
    virtual void set_timer(Timestamp at, CompletionCB fnptr, TimerID id) override;
    virtual void unsubscribe(SubscriptionType type, const Instrument &i) override;
    virtual Timestamp get_event_time() const override;
    virtual Order bind_order(const Instrument &instrument, const Account &account) override;
    virtual void update_account(const Account &a, CompletionCB complete_ptr) override;
    virtual void allocate(const Account &a, double equity) override;
    virtual bool clear_timer(TimerID id) override;
    virtual void update_instrument(const Instrument &i, CompletionCB complete_ptr) override;
    virtual void unset_var(std::string_view var_name) override;
    virtual void set_var(std::string_view var_name, std::string_view value)
            override;

protected:

    Scheduler _scheduler;
    Storage _storage;
    std::unique_ptr<IStrategy> _strategy;
    Timestamp _event_time;

    struct EvUpdateInstrument {
        Instrument i;
        void operator()(BasicContext *me) {
            auto rg = me->_cb_update_instrument.equal_range(i);
            while (rg.first != rg.second) {
                rg.first->second();
                rg.first = me->_cb_update_instrument.erase(rg.first);
            }
        }
    };

    struct EvUpdateAccount {
        Account a;
        void operator()(BasicContext *me) {
            auto rg = me->_cb_update_account.equal_range(a);
            while (rg.first != rg.second) {
                rg.first->second();
                rg.first = me->_cb_update_account.erase(rg.first);
            }
        }
    };

    struct EvMarketData {
        Instrument i;
        bool ticker = false;
        bool orderbook = false;
        void operator()(BasicContext *me) {
            if (ticker) {
                Ticker tk;
                if (i.get_exchange().get_last_ticker(i, tk)) {
                    me->_strategy->on_ticker(i, tk);
                }
            }
            if (orderbook) {
                OrderBook ordb;
                if (i.get_exchange().get_last_orderbook(i, ordb)) {
                    me->_strategy->on_orderbook(i, ordb);
                }
            }
        }
    };

    struct EvOrderStatus {
        Order order;
        Order::Report report;
        void operator()(BasicContext *me) {
            auto &e = BasicExchangeContext::from_exchange(order.get_account().get_exchange());
            e.order_apply_report(order, report);
            me->_strategy->on_order(Order(std::move(order)));
        }
    };

    struct EvOrderFill {
        Order order;
        Fill fill;
        void operator()(BasicContext *me) {
            auto &e = BasicExchangeContext::from_exchange(order.get_account().get_exchange());
            e.order_apply_fill(order, fill);
            me->_strategy->on_fill(Order(std::move(order)), fill);
        }
    };


    using QueueItem = std::variant<
            EvUpdateAccount,
            EvUpdateInstrument,
            EvOrderStatus,
            EvOrderFill,
            EvMarketData>;

    struct TimerItem {
        Timestamp tp;
        TimerID id;
        Function<void()> r;
        struct ordering {
            bool operator()(const TimerItem &a, const TimerItem &b) const {
                return a.tp > b.tp;
            }
        };
    };

    struct Batches {
        std::vector<Order> _batch_place;
        std::vector<Order> _batch_cancel;
    };

    std::mutex _queue_mx;
    std::deque<QueueItem> _queue;
    PriorityQueue<TimerItem, typename TimerItem::ordering> _timed_queue;

    std::multimap<Account, CompletionCB> _cb_update_account;
    std::multimap<Instrument, CompletionCB> _cb_update_instrument;
    std::map<Exchange, Batches> _exchanges;
    std::optional<typename Storage::Transaction> _trn;

    void flush_batches();
    void notify_queue();

    typename Storage::Transaction &get_transaction() {
        if (!_trn.has_value()) {
            _trn = _storage.begin_transaction();
        }
        return *_trn;
    }

    void on_scheduler(Timestamp tp) {
        _event_time = tp;
        std::unique_lock lk(_queue_mx);
        while (!_queue.empty()) {
            lk.unlock();
            std::visit([&](auto &item) {item(this);},_queue.front());
            lk.lock();
            _queue.pop_front();
        }
        Timestamp nx = Timestamp::max();
        while (!_timed_queue.empty() && _timed_queue.front().tp <= tp) {
                Function<void()> fn (std::move(_timed_queue.front().r));
                _timed_queue.pop();
                lk.unlock();
                fn();
                lk.lock();
        }
        if (!_timed_queue.empty()) {
            nx = _timed_queue.front().tp;
            _scheduler(nx, [this](auto tp){on_scheduler(tp);}, this);
        }
        flush_batches();
    }

};



template<StorageType Storage, SchedulerType Scheduler>
BasicContext<Storage, Scheduler>::~BasicContext() {
    _scheduler(Timestamp{}, [](auto){}, this);
    for (const auto &[e,_]: _exchanges) {
        BasicExchangeContext::from_exchange(e).disconnect(this);
    }
}

template<StorageType Storage, SchedulerType Scheduler>
void BasicContext<Storage, Scheduler>::init(std::unique_ptr<IStrategy> strategy,
        const Configuration &config) {
    strategy = std::move(_strategy);
    for (const auto &a: config.accounts) {
        _exchanges.emplace(a.get_exchange(),Batches{});
    }
    for (const auto &i: config.instruments) {
        _exchanges.emplace(i.get_exchange(),Batches{});
    }
    _strategy->on_init(this, config);
    auto orders = _storage.get_open_orders();
    for (auto &[e, _]: _exchanges) {
        BasicExchangeContext::from_exchange(e).restore_orders(this, {orders.data(), orders.size()});
    }
}



template<StorageType Storage, SchedulerType Scheduler>
void BasicContext<Storage, Scheduler>::on_event(const Instrument &i, SubscriptionType subscription_type) {
    std::lock_guard _(_queue_mx);
    auto iter = std::find_if(_queue.begin(), _queue.end(), [&](QueueItem &q){
        return std::holds_alternative<EvMarketData>(q)
                && std::get<EvMarketData>(q).i == i;
    });
    EvMarketData *md;
    if (iter == _queue.end()) {
        _queue.push_back(EvMarketData{i});
        md = &std::get<EvMarketData>(_queue.back());
    } else {
        md = &std::get<EvMarketData>(*iter);
        notify_queue();
    }
    switch (subscription_type) {
        case SubscriptionType::ticker: md->ticker =true; break;
        case SubscriptionType::orderbook: md->orderbook =true; break;
        default: break;
    }
}


template<StorageType Storage, SchedulerType Scheduler>
void BasicContext<Storage, Scheduler>::on_event(const Order &order, const Fill &fill) {
    std::lock_guard _(_queue_mx);
    _queue.push_back(EvOrderFill{order, fill});
    notify_queue();
}


template<StorageType Storage, SchedulerType Scheduler>
void BasicContext<Storage, Scheduler>::on_event(const Order &order, const Order::Report &report) {
    std::lock_guard _(_queue_mx);
    _queue.push_back(EvOrderStatus{order, report});
    notify_queue();
}



template<StorageType Storage, SchedulerType Scheduler>
void BasicContext<Storage, Scheduler>::on_event(const Instrument &i) {
    std::lock_guard _(_queue_mx);
    _queue.push_back(EvUpdateInstrument{i});
    notify_queue();

}


template<StorageType Storage, SchedulerType Scheduler>
void BasicContext<Storage, Scheduler>::on_event(const Account &a) {
    std::lock_guard _(_queue_mx);
    _queue.push_back(EvUpdateAccount{a});
    notify_queue();
}

template<StorageType Storage, SchedulerType Scheduler>
void BasicContext<Storage, Scheduler>::notify_queue() {
    Timestamp tp = Timestamp::max();
    if (_queue.empty()) {
        if (_timed_queue.empty()) {
            return;
        }
        tp = _timed_queue.front().tp;
    } else {
        tp = Timestamp::min();
    }
    _scheduler(tp,[this](auto tp){on_scheduler(tp);}, this);
}

template<StorageType Storage, SchedulerType Scheduler>
void BasicContext<Storage, Scheduler>::subscribe(SubscriptionType type, const Instrument &i) {
    BasicExchangeContext::from_exchange(i.get_exchange()).subscribe(this, type, i);
}



template<StorageType Storage, SchedulerType Scheduler>
Fills BasicContext<Storage, Scheduler>::get_fills(const Instrument &i, std::size_t limit) const {
    return _storage.get_fills(i, limit);
}
template<StorageType Storage, SchedulerType Scheduler>
Fills BasicContext<Storage, Scheduler>::get_fills(const Instrument &i,Timestamp tp) const {
    return _storage.get_fills(i, tp);
}
template<StorageType Storage, SchedulerType Scheduler>
Order BasicContext<Storage, Scheduler>::place(const Instrument &instrument, const Account &account, const Order::Setup &setup) {

    Exchange ex = account.get_exchange();
    BasicExchangeContext &e = BasicExchangeContext::from_exchange(ex);
    auto ord = e.create_order(instrument, account, setup);
    if (!ord.discarded()) {
        _exchanges[ex]._batch_place.push_back(ord);
    }
    return ord;
}

template<StorageType Storage, SchedulerType Scheduler>
Order BasicContext<Storage, Scheduler>::replace(const Order &order, const Order::Setup &setup, bool amend) {
    Exchange ex = order.get_account().get_exchange();
    BasicExchangeContext &e = BasicExchangeContext::from_exchange(ex);
    auto ord = e.create_order_replace(order, setup, amend);
    if (!ord.discarded()) {
        _exchanges[ex]._batch_place.push_back(ord);
    }
    return Order(ord);
}


template<StorageType Storage, SchedulerType Scheduler>
void BasicContext<Storage, Scheduler>::cancel(const Order &order) {
    Exchange e = order.get_account().get_exchange();
    _exchanges[e]._batch_cancel.push_back(order);
}

template<StorageType Storage, SchedulerType Scheduler>
void BasicContext<Storage, Scheduler>::set_timer(Timestamp at, CompletionCB fnptr, TimerID id) {

    std::lock_guard _(_queue_mx);

    if (!fnptr) fnptr = [this, id]{
        _strategy->on_timer(id);
    };
    Timestamp top = Timestamp::max();
    if (!_timed_queue.empty()) top = _timed_queue.front().tp;
    _timed_queue.push(TimerItem{at, id, std::move(fnptr)});
    if (at < top) notify_queue();
}

template<StorageType Storage, SchedulerType Scheduler>
void BasicContext<Storage, Scheduler>::unsubscribe(SubscriptionType type, const Instrument &i) {
    BasicExchangeContext::from_exchange(i.get_exchange()).unsubscribe(this, type, i);
}

template<StorageType Storage, SchedulerType Scheduler>
Timestamp BasicContext<Storage, Scheduler>::get_event_time() const {
    return _event_time;

}

template<StorageType Storage, SchedulerType Scheduler>
Order BasicContext<Storage, Scheduler>::bind_order(const Instrument &instrument, const Account &account) {
    return Order(std::make_shared<AssociatedOrder>(instrument, account));
}

template<StorageType Storage, SchedulerType Scheduler>
void BasicContext<Storage, Scheduler>::update_account(const Account &a, CompletionCB complete_ptr) {
    bool do_call = false;
    BasicExchangeContext &e = BasicExchangeContext::from_exchange(a.get_exchange());
    {
        std::lock_guard _(_queue_mx);
        do_call = _cb_update_account.find(a) == _cb_update_account.end();
        _cb_update_account.insert({a, std::move(complete_ptr)});
    }
    if (do_call) {
        e.update_account(this, a);
    }
}

template<StorageType Storage, SchedulerType Scheduler>
void BasicContext<Storage, Scheduler>::allocate(const Account &a, double equity) {
    //todo
}

template<StorageType Storage, SchedulerType Scheduler>
bool BasicContext<Storage, Scheduler>::clear_timer(TimerID id) {
    std::lock_guard _(_queue_mx);
    auto iter = std::find_if(_timed_queue.begin(), _timed_queue.end(), [&](const TimerItem &item){
       return item.id == id;
    });
    if (iter != _timed_queue.end()) {
        _timed_queue.erase(iter);
        return true;
    }
    return false;
}


template<StorageType Storage, SchedulerType Scheduler>
void BasicContext<Storage, Scheduler>::update_instrument(const Instrument &i, CompletionCB complete_ptr) {
    bool do_call = false;
    BasicExchangeContext &e = BasicExchangeContext::from_exchange(i.get_exchange());
    {
        std::lock_guard _(_queue_mx);
        do_call = _cb_update_instrument.find(i) == _cb_update_instrument.end();
        _cb_update_instrument.insert({i, std::move(complete_ptr)});
    }
    if (do_call) {
        e.update_instrument(this, i);
    }

}

template<StorageType Storage, SchedulerType Scheduler>
void BasicContext<Storage, Scheduler>::flush_batches() {
    for (auto &[ex, batch]: _exchanges) {
        BasicExchangeContext &e = BasicExchangeContext::from_exchange(ex);
        if (!batch._batch_cancel.empty()) {
            e.batch_cancel({batch._batch_cancel.begin(), batch._batch_cancel.end()});
            batch._batch_cancel.clear();
        }
        if (!batch._batch_place.empty()) {
            e.batch_place(this, {batch._batch_place.begin(), batch._batch_place.end()});
            batch._batch_place.clear();
        }
    }
    if (_trn.has_value()) {
        _storage.commit(*_trn);
        _trn.reset();
    }
}



template<StorageType Storage, SchedulerType Scheduler>
inline void BasicContext<Storage, Scheduler>::unset_var(std::string_view var_name) {
    get_transaction().erase(var_name);
}

template<StorageType Storage, SchedulerType Scheduler>
inline void BasicContext<Storage, Scheduler>::set_var(std::string_view var_name, std::string_view value) {
    get_transaction().put(var_name, value);
}

}
