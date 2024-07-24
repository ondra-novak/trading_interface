#include "basic_context.h"

#include "basic_order.h"
namespace trading_api {



BasicContext::~BasicContext() {
    _scheduler(Timestamp{}, [](auto){}, this);
    for (const auto &[e,_]: _exchanges) {
        BasicExchangeContext::from_exchange(e).disconnect(this);
    }
}


void BasicContext::init(std::unique_ptr<IStrategy> strategy,
        std::vector<Account> accounts,
        std::vector<Instrument> instruments,
        StrategyConfig config) {
    _strategy = std::move(strategy);
    _accounts = std::move(accounts);
    _instruments = std::move(instruments);
    _config = std::move(config);

    for (const auto &a: _accounts) {
        _exchanges.emplace(a.get_exchange(),Batches{});
    }
    for (const auto &i: _instruments) {
        _exchanges.emplace(i.get_exchange(),Batches{});
    }
    _strategy->on_init(this);
    auto orders = _storage->load_open_orders();
    for (auto &[e, _]: _exchanges) {
        BasicExchangeContext::from_exchange(e).restore_orders(this, {orders.data(), orders.size()});
    }
}




void BasicContext::on_event(const Instrument &i, SubscriptionType subscription_type) {
    std::lock_guard _(_queue_mx);
    auto iter = std::find_if(_queue.begin(), _queue.end(), [&](QueueItem &q){
        return std::holds_alternative<EvMarketData>(q)
                && std::get<EvMarketData>(q).i == i;
    });
    EvMarketData *md;
    if (iter == _queue.end()) {
        _queue.push_back(EvMarketData{this,i});
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



void BasicContext::on_event(const Order &order, const Fill &fill) {
    std::lock_guard _(_queue_mx);
    _queue.push_back(EvOrderFill{this, order, fill});
    notify_queue();
}



void BasicContext::on_event(const Order &order, const Order::Report &report) {
    std::lock_guard _(_queue_mx);
    _queue.push_back(EvOrderStatus{this, order, report});
    notify_queue();
}




void BasicContext::on_event(const Instrument &i) {
    std::lock_guard _(_queue_mx);
    _queue.push_back(EvUpdateInstrument{this, i});
    notify_queue();

}



void BasicContext::on_event(const Account &a) {
    std::lock_guard _(_queue_mx);
    _queue.push_back(EvUpdateAccount{this, a});
    notify_queue();
}


void BasicContext::notify_queue() {
    Timestamp tp = Timestamp::max();
    if (_queue.empty()) {
        if (_timed_queue.empty()) {
            return;
        }
        tp = _timed_queue.front().tp;
    } else {
        tp = Timestamp::min();
    }
    if (_scheduled_time > tp) {
        _scheduled_time = tp;
        _scheduler(tp,[this](auto tp){on_scheduler(tp);}, this);
    }

}


void BasicContext::subscribe(SubscriptionType type, const Instrument &i) {
    BasicExchangeContext::from_exchange(i.get_exchange()).subscribe(this, type, i);
}




Fills BasicContext::get_fills(std::size_t limit, std::string_view filter) const {
    return _storage->load_fills(limit, filter);
}
Fills BasicContext::get_fills(Timestamp tp, std::string_view filter) const {
    return _storage->load_fills(tp, filter);

}


Order BasicContext::place(const Instrument &instrument, const Account &account, const Order::Setup &setup) {

    Exchange ex = account.get_exchange();
    BasicExchangeContext &e = BasicExchangeContext::from_exchange(ex);
    auto ord = e.create_order(instrument, account, setup);
    if (!ord.discarded()) {
        _exchanges[ex]._batch_place.push_back(ord);
    }
    return ord;
}


Order BasicContext::replace(const Order &order, const Order::Setup &setup, bool amend) {
    Exchange ex = order.get_account().get_exchange();
    BasicExchangeContext &e = BasicExchangeContext::from_exchange(ex);
    auto ord = e.create_order_replace(order, setup, amend);
    if (!ord.discarded()) {
        _exchanges[ex]._batch_place.push_back(ord);
    }
    return Order(ord);
}



void BasicContext::cancel(const Order &order) {
    Exchange e = order.get_account().get_exchange();
    _exchanges[e]._batch_cancel.push_back(order);
}


void BasicContext::set_timer(Timestamp at, CompletionCB fnptr, TimerID id) {

    std::lock_guard _(_queue_mx);

    if (!fnptr) fnptr = [this, id]{
        _strategy->on_timer(id);
    };
    _timed_queue.push(TimerItem{at, id, std::move(fnptr)});
    notify_queue();
}


void BasicContext::unsubscribe(SubscriptionType type, const Instrument &i) {
    BasicExchangeContext::from_exchange(i.get_exchange()).unsubscribe(this, type, i);
}


Timestamp BasicContext::get_event_time() const {
    return _event_time;

}


Order BasicContext::bind_order(const Instrument &instrument, const Account &account) {
    return Order(std::make_shared<AssociatedOrder>(instrument, account));
}


void BasicContext::update_account(const Account &a, CompletionCB complete_ptr) {
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


void BasicContext::allocate(const Account &, double ) {
    //todo
}


bool BasicContext::clear_timer(TimerID id) {
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



void BasicContext::update_instrument(const Instrument &i, CompletionCB complete_ptr) {
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


void BasicContext::flush_batches() {
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
}




void BasicContext::unset_var(std::string_view var_name) {
    _storage->erase_var(var_name);
}


void BasicContext::set_var(std::string_view var_name, std::string_view value) {
    _storage->put_var(var_name, value);
}



void BasicContext::EvUpdateInstrument::operator ()() {
        auto rg = me->_cb_update_instrument.equal_range(i);
        while (rg.first != rg.second) {
            rg.first->second();
            rg.first = me->_cb_update_instrument.erase(rg.first);
        }
}

void BasicContext::EvUpdateAccount::operator ()() {
    auto rg = me->_cb_update_account.equal_range(a);
    while (rg.first != rg.second) {
        rg.first->second();
        rg.first = me->_cb_update_account.erase(rg.first);
    }
}


void BasicContext::EvMarketData::operator ()() {
    if (ticker) {
        TickData tk;
        if (i.get_exchange().get_last_ticker(i, tk)) {
            me->_strategy->on_market_event(i, tk);
        }
    }
    if (orderbook) {
        OrderBook ordb;
        if (i.get_exchange().get_last_orderbook(i, ordb)) {
            me->_strategy->on_market_event(i, ordb);
        }
    }
}

void BasicContext::EvOrderStatus::operator ()() {
    auto &e = BasicExchangeContext::from_exchange(order.get_account().get_exchange());
    e.order_apply_report(order, report);
    me->_storage->put_order(order);
    me->_strategy->on_order(std::move(order));
}

void BasicContext::EvOrderFill::operator ()() {
    auto &e = BasicExchangeContext::from_exchange(order.get_account().get_exchange());
    if (me->_storage->is_duplicate_fill(fill)) return;
    e.order_apply_fill(order, fill);
    fill.label = me->_strategy->on_fill(Order(std::move(order)), fill);
    me->_storage->put_fill(fill);

}

std::string BasicContext::get_var(std::string_view var_name) const {
    return _storage->get_var(var_name);
}

std::span<const Account> BasicContext::get_accounts() const {
    return {_accounts.data(), _accounts.size()};
}

std::span<const Instrument> BasicContext::get_instruments() const {
    return {_instruments.data(), _instruments.size()};
}


const StrategyConfig &BasicContext::get_config() const {
    return _config;
}

void BasicContext::enum_vars(std::string_view start, std::string_view end,
        Function<void(std::string_view,std::string_view)> &fn) const {
    _storage->enum_vars(start,end, fn);
}
void BasicContext::enum_vars(std::string_view prefix,
        Function<void(std::string_view,std::string_view)> &fn) const {
    _storage->enum_vars(prefix, fn);

}

}
