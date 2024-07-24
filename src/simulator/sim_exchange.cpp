#include "sim_exchange.h"
#include "../common/basic_order.h"
namespace trading_api {

StrategyConfigSchema SimExchange::get_config_schema() const {
    return {};
}

Order SimExchange::create_order(const Instrument &instrument,
        const Account &account, const Order::Setup &setup) {

    return Order(std::make_shared<BasicOrder>(instrument, account, setup, Order::Origin::strategy));
}

void SimExchange::init(ExchangeContext context, const StrategyConfig &config) {
    this->ctx = std::move(context);
}

void SimExchange::order_apply_fill(const Order &order, const Fill &fill) {
    const BasicOrder &bsorder = dynamic_cast<const BasicOrder &>(*order.get_handle());
    bsorder.get_status().add_fill(fill.price,fill.amount);
}

void SimExchange::update_account(const Account &a) {
    ctx.object_updated(a);
}

Order SimExchange::create_order_replace(const Order &replace,
        const Order::Setup &setup, bool amend) {

    return Order(std::make_shared<BasicOrder>(replace, setup, amend, Order::Origin::strategy));
}

std::string SimExchange::get_id() const {
    return "simul";
}

std::optional<IExchange::Icon> SimExchange::get_icon() const {
    return {};
}

void SimExchange::subscribe(SubscriptionType , const Instrument &) {
    //empty
}

void SimExchange::update_instrument(const Instrument &i) {
    ctx.object_updated(i);
}

void SimExchange::unsubscribe(SubscriptionType , const Instrument &i) {
    //empty
}

void SimExchange::order_apply_report(const Order &order,
        const Order::Report &report) {

    const BasicOrder &bsorder = dynamic_cast<const BasicOrder &>(*order.get_handle());
    bsorder.get_status().update_report(report);

}
#if 0
void SimExchange::create_accounts(std::vector<std::string> account_idents,
        Function<void(std::vector<Account>)> cb) {
    std::vector<Account> acc;
    for (const auto &id : account_idents) {
        auto iter = _accounts.find(id);
        if (iter != _accounts.end()) acc.push_back(iter->second);
    }
    cb(std::move(acc));
}
void SimExchange::create_instruments(std::vector<std::string> instruments,
        Account , Function<void(std::vector<Instrument>)> cb) {
    std::vector<Instrument> instr;
    for (const auto &id : instruments) {
        auto iter = _instruments.find(id);
        if (iter != _instruments.end()) instr.push_back(iter->second);
    }
    cb(std::move(instr));
}

#endif
void SimExchange::restore_orders(void *context, std::span<SerializedOrder> orders) {
    //todo
}



std::string SimExchange::get_name() const {
    return "Simulator";
}


SimExchange::SimExchange(GlobalScheduler scheduler, Config config)
:_scheduler(std::move(scheduler))
{
    std::transform(config.accounts.begin(), config.accounts.end(),
            std::inserter(_accounts, _accounts.end()), [&](const Account &a){
        return std::pair(a.get_id(), a);
    });
    std::transform(config.instruments.begin(), config.instruments.end(),
            std::inserter(_instruments, _instruments.end()), [&](const Instrument &a){
        return std::pair(a.get_id(), a);
    });
}

void SimExchange::on_timer(Timestamp tp) {
    std::lock_guard _(_mx);
    if (_price_data.empty()) return;
    if (_price_data.front().tp <= tp) {
        auto v = std::move(_price_data.front());
        _price_data.pop();
        if (std::holds_alternative<TickData>(v.data)) {
            const auto &tk = std::get<TickData>(v.data);
            ctx.income_data(v.i, tk);
        } else if (std::holds_alternative<TickData>(v.data)) {
            const auto &up = std::get<OrderBook::Update>(v.data);
            OrderBook &ob = _orderbooks[v.i];
            ob.update(up);
            ctx.income_data(v.i, ob);
        }
    }
    reschedule();
}

void SimExchange::reschedule() {
    if (_price_data.empty()) return;
    _scheduler(_price_data.front().tp, [this](Timestamp st){on_timer(st);}, this);
}

void SimExchange::add_record(const Timestamp &tp, const Instrument &i, const OrderBook::Update &ordb) {
    std::lock_guard _(_mx);
    _price_data.push({tp, i, ordb});
    reschedule();
}

void SimExchange::add_record(const Timestamp &tp, const Instrument &i, const TickData &tk) {
    _price_data.push({tp, i, tk});
    reschedule();
}

void SimExchange::batch_place(std::span<Order> orders) {
    //todo
}

void SimExchange::batch_cancel(std::span<Order> orders) {
    //todo
}


}
