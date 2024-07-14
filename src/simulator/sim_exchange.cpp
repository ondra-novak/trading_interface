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
}

Order SimExchange::create_order_replace(const Order &replace,
        const Order::Setup &setup, bool amend) {

    return Order(std::make_shared<BasicOrder>(replace, setup, amend, Order::Origin::strategy));
}

std::string SimExchange::get_id() const {
}

std::optional<IExchange::Icon> SimExchange::get_icon() const {
}

void SimExchange::subscribe(SubscriptionType type, const Instrument &i) {
}

void SimExchange::update_instrument(const Instrument &i) {
}

void SimExchange::unsubscribe(SubscriptionType type, const Instrument &i) {
}

void SimExchange::order_apply_report(const Order &order,
        const Order::Report &report) {

    const BasicOrder &bsorder = dynamic_cast<const BasicOrder &>(*order.get_handle());
    bsorder.get_status().update_report(report);

}

void SimExchange::create_accounts(std::vector<std::string> account_idents,
        Function<void(std::vector<Account>)> cb) {
}

void SimExchange::batch_cancel(std::span<Order> orders) {
}

void SimExchange::restore_orders(void *context,
        std::span<SerializedOrder> orders) {
}

void SimExchange::batch_place(std::span<Order> orders) {
}

void SimExchange::create_instruments(std::vector<std::string> instruments,
        Account accunt, Function<void(std::vector<Account>)> cb) {
}

std::string SimExchange::get_name() const {
    return "Simulator";
}

void SimExchange::send_ticker(const Instrument &i, const Ticker &tk) {
    ctx.income_data(i, tk);
}

void SimExchange::send_orderbook(const Instrument &i, const OrderBook &ordb) {
    ctx.income_data(i, ordb);
}

}
