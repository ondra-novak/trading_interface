#include "exchange_main.h"
#include "instrument.h"

#include "identity.h"


using namespace trading_api;




ConfigSchema BinanceExchange::get_exchange_config_schema() const {

    return {
        params::Select("server",{{"live","live"},{"testnet","testnet"}})
    };
}


void BinanceExchange::init(ExchangeContext context, const Config &config) {

    auto [server] = config("server");

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
    _frest.emplace(*_rest_context, frest);
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

PIdentity BinanceExchange::find_identity(const std::string &ident) const {
    auto idlk = _identities.lock();
    auto iter = idlk->find(ident);
    if (iter == idlk->end()) return {};
    return iter->second;
}

void BinanceExchange::query_accounts(std::string_view identity, std::string_view query,
        std::string_view label, Function<void(Account)> cb) {

    auto ident = find_identity(std::string(identity));
    if (ident) {
        _frest->signed_call(*ident, HttpMethod::GET, "/v1/account", {},
                [this, ident = std::string(identity),
                       q = std::string(query),
                       l = std::string(label),
                       cb = std::move(cb)](const RestClient::Result &result) mutable {
            auto ex = _ctx.get_exchange();
            if (!result.is_error()) {
                auto data = result.content;
                for (json::value_t a: data["assets"]) {
                    if (q.empty() || q == "*" || q == a["asset"].as<std::string_view>()) {}
                    auto acc =_accounts.create_if_not_exists<BinanceAccount>(a["asset"].get(),
                            ident, ex, l);
                    update_account(acc, a, data["positions"]);
                    cb(Account(acc));
                }
            }
        });
    }
}

void BinanceExchange::update_account(const std::shared_ptr<BinanceAccount> &acc,
        const json::value &asset_info, const json::value &positions) {
    Account::Info nfo;
    nfo.balance = asset_info["marginBalance"].as<double>();
    nfo.blocked = asset_info["initialMargin"].as<double>();
    nfo.currency = acc->get_ident();
    nfo.equity = nfo.balance;
    nfo.leverage = 0;
    nfo.ratio = 0;

    Account::Positions poslist;

    for (json::value pos: positions) {
        auto symbol = pos["symbol"].as<std::string_view>();
        auto sinfo = _instrument_def_cache.find(symbol);
        if (sinfo.has_value() && sinfo->quote_asset == nfo.currency) {
            double lev = pos["leverage"].get();
            double amn = pos["positionAmt"].get();
            if (amn) {
                Side side = amn<0?Side::sell:amn>0?Side::buy:Side::undefined;
                IAccount::Position qpos;
                //INSTRUMENT!!!!
                qpos.amount = amn * static_cast<double>(side);
                qpos.side = side;
                qpos.id = pos["positionSide"].as<std::string>();
                qpos.open_price =pos["entryPrice"].get();
                qpos.leverage = lev;
                poslist.push_back(std::move(qpos));
            }
        }
    }



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

trading_api::ConfigSchema BinanceExchange::get_api_key_config_schema() const {
    return {
        params::TextInput("api_name", ""),
        params::TextArea("private_key", 3, "", 1024)
    };
}

void BinanceExchange::unset_api_key(std::string_view name) {
    _identities.lock()->erase(std::string(name));
}

void BinanceExchange::set_api_key(std::string_view name,const trading_api::Config &api_key_config) {
    auto [api_name, secret] = api_key_config("api_name","secret");



    _identities.lock()->emplace(std::string(name), Identity::create({
        api_name.as<std::string>(),
        secret.as<std::string>()}));
}
