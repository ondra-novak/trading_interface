#include "exchange_main.h"
#include "instrument.h"



using namespace trading_api;




StrategyConfigSchema BinanceExchange::get_config_schema() const {

    return {
        params::TextInput("api_name", ""),
        params::TextArea("private_key", 3, "", 1024),
        params::Select("server",{{"live","live"},{"testnet","testnet"}})
    };
}


void BinanceExchange::init(ExchangeContext context, const StrategyConfig &config) {

    auto [api_name, private_key, server] = config("api_name","private_key","server");

    _key.api_key = api_name.as<std::string>();
    _key.secret = private_key.as<std::string>();
    _ctx = std::move(context);
    _log = _ctx.get_log();

    _ws_context.emplace();
    _rest_context.emplace(*_ws_context, _log);

    std::string stype = server.get<std::string>("live");
    std::string fstreams;
    std::string frest;
    if (stype == "testnet") {
        frest = "https://testnet.binancefuture.com/fapi";
        fstreams = "wss://fstream.binance.com/ws";

    } else {
        frest = "https://fapi.binance.com/fapi";
        fstreams = "wss://fstream.binance.com/ws";
    }
    _public_fstream.emplace(*this, *_ws_context, fstreams);
    _frest.emplace(*_rest_context, frest, _key);
    RPCClient::run_thread_auto_reconnect(*_public_fstream, 5, this);

}

void BinanceExchange::subscribe(SubscriptionType type, const Instrument &i) {
    auto id = i.get_id();
    _log.trace("Request to subscribe: {}", id);
    _public_fstream->subscribe(type, id);
}


void BinanceExchange::unsubscribe(SubscriptionType type, const Instrument &i) {
    auto id = i.get_id();
    _log.trace("Request to unsubscribe: {}", id);
    _public_fstream->unsubscribe(type, id);
}


void BinanceExchange::on_ticker(std::string_view symbol,
        const Ticker &ticker) {

    auto instr = _instruments.find(symbol);
    if (instr) {
        _ctx.income_data(Instrument(instr), ticker);
    } else {
        _public_fstream->unsubscribe(SubscriptionType::ticker, symbol);
    }
}


void BinanceExchange::on_orderbook(std::string_view symbol, const OrderBook &update) {
    auto instr = _instruments.find(symbol);
    if (instr) {
        _ctx.income_data(Instrument(instr), update);
    } else {
        _public_fstream->unsubscribe(SubscriptionType::orderbook, symbol);
    }
}

void BinanceExchange::query_accounts(std::string_view query,
        std::string_view label, Function<void(Account)> cb) {
}

BinanceExchange::Order BinanceExchange::create_order(
        const Instrument &instrument, const trading_api::Account &account,
        const Order::Setup &setup) {
}

void BinanceExchange::batch_place(std::span<Order> orders) {
}

void BinanceExchange::order_apply_fill(const Order &order,
        const trading_api::Fill &fill) {
}

void BinanceExchange::update_account(const trading_api::Account &a) {
}

BinanceExchange::Order BinanceExchange::create_order_replace(
        const Order &replace, const Order::Setup &setup, bool amend) {
}

std::string BinanceExchange::get_id() const {
}

std::optional<trading_api::IExchange::Icon> BinanceExchange::get_icon() const {
}

void BinanceExchange::batch_cancel(std::span<Order> orders) {
}

void BinanceExchange::update_instrument(const Instrument &i) {
}

void BinanceExchange::order_apply_report(const Order &order,
        const Order::Report &report) {
}

std::string BinanceExchange::get_name() const {
}

void BinanceExchange::restore_orders(void *context,
        std::span<trading_api::SerializedOrder> orders) {
}

void BinanceExchange::subscribe_result(std::string_view symbol,
        SubscriptionType type, const RPCClient::Result &result) {
    if (result.is_error) {
        std::string error;
        if (result.status == RPCClient::status_connection_lost) {
            error = "Connection is unavailable";
        } else {
            error = result.content.to_string();
        }
        _log.warning("Failed to subscribe {} {} (will retry): {} ", type, symbol, error);
    } else {
        _log.trace("Subscribed {} {}", type, symbol);
    }

}

void BinanceExchange::query_instruments(std::string_view query,
        std::string_view label, Function<void(Instrument)> cb) {

    _instruments.gc();
    if (_instrument_def_cache.need_reload()) {
        if (_instrument_def_cache.begin_reload(
                [this, q = std::string(query),lbl = std::string(label), cb = std::move(cb)]() mutable {
            query_instruments(q, lbl, std::move(cb));
        })) {
            _instrument_def_cache.reload(*_frest, false);
        }
        _instrument_def_cache.end_reload();
        return;
    }

    auto ex = _ctx.get_exchange();
    auto qr = _instrument_def_cache.query(query);
    for (const auto &r: qr) {
        auto bi = _instruments.create_if_not_exists<BinanceInstrument>(r.id, label, r, ex);
        cb(Instrument(bi));

    }

}


void BinanceExchange::on_reconnect(std::string reason) {
    if (reason.empty()) reason = "Stalled";
    _log.error("{} / Reconnect. ", reason);
}
void BinanceExchange::on_ping() {
    _log.trace("Ping/Keep alive");
}


