#include "basic_context.h"

#include "basic_order.h"
namespace trading_api {




BasicContext::~BasicContext() {
    if (_global_scheduler) _global_scheduler->unset(&_reg);
}

void BasicContext::init(std::unique_ptr<IStrategy> strategy,
        IStrategy::InstrumentList instruments,
        ContextScheduler *ctx_sch,
        std::shared_ptr<IStorage> storage,
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

    _strategy->on_init(Context(this), config, instruments);
}


void BasicContext::reschedule(Timestamp tp) {
    _global_scheduler->set(&_reg, tp);
}

void BasicContext::on_event(Order ord,const Fill &fill) {
    _sch.enqueue([this,ord, fill = Fill(fill)]{
        _storage->store_fill(fill);
        _strategy->on_fill(ord, fill);
    });
}

void BasicContext::on_event(Order ord) {
    _sch.enqueue([this, ord]{
        _strategy->on_order(ord);
    });
}

void BasicContext::on_event(Instrument i, const OrderBook &ord) {
    _sch.enqueue_collapse(orderbook_id, [this, i, ord = OrderBook(ord)]{
        _strategy->on_orderbook(i, ord);
    });
}

template<typename MMap, typename Iter, typename Target>
void move_range(MMap &map, std::pair<Iter, Iter> range, std::vector<Target> &out) {
    for (auto iter = range.first; iter != range.second; ++iter) {
        out.push_back(std::move(iter->second));
    }
    map.erase(range.first, range.second);
}

void BasicContext::on_event(Instrument i) {
    _sch.enqueue([this, i]{
        {
            std::lock_guard _(_cb_mx);
            move_range(_cb_update_instrument, _cb_update_instrument.equal_range(i), _cb_tmp);
        }
        for (auto &cbs:_cb_tmp) {
            cbs();
        }
        _cb_tmp.clear();
    });

}

void BasicContext::on_event(Instrument i, const Ticker &tk) {
    _sch.enqueue_collapse(ticker_id, [this, i, tk = Ticker(tk)]{
        _strategy->on_ticker(i, tk);
    });
}


void BasicContext::on_event(Account a) {
    _sch.enqueue([this, a]{
        {
            std::lock_guard _(_cb_mx);
            move_range(_cb_update_account, _cb_update_account.equal_range(a), _cb_tmp);
        }
        for (auto &cbs:_cb_tmp) {
            cbs();
        }
        _cb_tmp.clear();
    });
}

void BasicContext::subscribe(SubscriptionType type, const Instrument &i, TimeSpan interval) {

}

Order BasicContext::replace(const Order &order,
        const Order::Setup &setup, bool amend) {
}


Fills BasicContext::get_fills(std::size_t limit) {
    return _storage->get_fills(limit);
}

Order BasicContext::place(const Instrument &instrument, const Order::Setup &setup) {

}

void BasicContext::cancel(const Order &order) {
}

TimerID BasicContext::set_timer(Timestamp at, CompletionCB fnptr) {
    return _sch.enqueue_timed(at, std::move(fnptr));
}

void BasicContext::unsubscribe(SubscriptionType type, const Instrument &i) {
}

Timestamp BasicContext::now() const {
    return _cur_time;

}

Order BasicContext::bind_order(const Instrument &instrument) {
    return Order(std::make_shared<AssociatedOrder>(instrument));
}

void BasicContext::update_account(const Account &a, CompletionCB complete_ptr) {
    bool do_call = false;
    {
        std::lock_guard _(_cb_mx);
        do_call = _cb_update_account.find(a) == _cb_update_account.end();
        _cb_update_account.insert({a, std::move(complete_ptr)});
    }
    if (do_call) {
        //todo - handle update account a
    }
}

void BasicContext::allocate(const Account &a, double equity) {
}

bool BasicContext::clear_timer(TimerID id) {
    return _sch.cancel_timed(id);
}

Value BasicContext::get_var(int idx) const {
    return _storage->get_var(idx);
}

void BasicContext::update_instrument(const Instrument &i, CompletionCB complete_ptr) {
    bool do_call = false;
    {
        std::lock_guard _(_cb_mx);
        do_call = _cb_update_instrument.find(i) == _cb_update_instrument.end();
        _cb_update_instrument.insert({i, std::move(complete_ptr)});
    }
    if (do_call) {
        //todo - handle update instrument a
    }

}

void BasicContext::set_var(int idx, const Value &val) {
    _storage->set_var(idx, val);
}


}
