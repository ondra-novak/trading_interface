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

WebSocketContext &BinanceExchange::get_ws_context() {
    static WebSocketContext ctx;
    return ctx;
}

void BinanceExchange::init(ExchangeContext context, const StrategyConfig &config) {

    auto [api_name, private_key, server] = config("api_name","private_key","server");

    if (!!api_name && !!private_key) {
        _key.emplace(api_name,private_key);
    }
    _ctx = std::move(context);
    _log = _ctx.get_log();

    std::string stype = server.get<std::string>("live");
    std::string fstreams;
    std::string frpc;
    std::string fresturl;
    std::string dresturl;
    if (stype == "testnet") {
        frpc = "wss://testnet.binancefuture.com/ws-fapi/v1";
        fresturl = "https://testnet.binancefuture..com/fapi";
        dresturl = "https://testnet.binancefuture..com/dapi";
        fstreams = "wss://fstream.binance.com/ws";

    } else {
        frpc = "wss://ws-fapi.binance.com/ws-fapi/v1";
        fresturl = "https://fapi.binance.com/fapi";
        dresturl = "https://dapi.binance.com/dapi";
        fstreams = "wss://fstream.binance.com/ws";
    }
    _public_fstream.emplace(*this, get_ws_context(), fstreams);
    _rpc.emplace(get_ws_context(), frpc);
    _instrument_def_cache._fapi_url = fresturl+"/v1/exchangeinfo";
    _instrument_def_cache._dapi_url = dresturl+"/v1/exchangeinfo";
    _instrument_def_cache._log = Log(_log, "Instrument cache");
}

void BinanceExchange::subscribe(SubscriptionType type, const Instrument &i) {
    auto id = i.get_id();
    _log.debug("Request to subscribe: {}", id);
    _public_fstream->subscribe(type, id);
}


void BinanceExchange::unsubscribe(SubscriptionType type, const Instrument &i) {
    auto id = i.get_id();
    _log.debug("Request to unsubscribe: {}", id);
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
        _log.info("Subscribed {} {}", type, symbol);
    }

}

void BinanceExchange::query_instruments(std::string_view query,
        std::string_view label, Function<void(Instrument)> cb) {

}


void BinanceExchange::InstrumentDefCache::reload() {
    HttpClientRequest dapi_rq(get_ws_context(), _dapi_url);
    HttpClientRequest fapi_rq(get_ws_context(), _fapi_url);
    HttpClientRequest::Data data;
    _expires = std::chrono::system_clock::now() + std::chrono::hours(1);
    int status = fapi_rq.get_status_sync();
    if (status != 200) {
        _log.error("Failed to reload USDS-M : {}", status);
    } else {
        if (fapi_rq.read_body_sync(data, true) != HttpClientRequest::data) {
            _log.error("Failed to reload USDS-M : timeout");
        }
        parse_instruments({data.data(), data.size()},false);
    }
    status = dapi_rq.get_status_sync();
    if (status != 200) {
        _log.error("Failed to reload COIN-M : {}", status);
    } else {
        if (fapi_rq.read_body_sync(data, true) != HttpClientRequest::data) {
            _log.error("Failed to reload COIN-M : timeout");
        }
        parse_instruments({data.data(), data.size()}, true);
    }
}

void BinanceExchange::InstrumentDefCache::parse_instruments(std::string_view json_data, bool coinm) {
    json::value jsdata = json::value::from_json(json_data);
    std::vector<BinanceInstrument::Config> _new_cache;
    for (json::value sd: jsdata["symbols"]) {
        BinanceInstrument::Config cfg;
        cfg.id = sd["symbol"].as<std::string>();
        cfg.can_short = true;
        cfg.type = coinm?Instrument::Type::inverted_contract:Instrument::Type::contract;
        cfg.tradable = sd["status"].as<std::string_view>() == "TRADING";
        for (auto f: sd["filters"]) {
            std::string_view type = f["filterType"].get();
            if (type == "PRICE_FILTER") {
                cfg.tick_size = f["tickSize"].get();
            } else if (type == "LOT_SIZE") {
                cfg.min_size = f["minQty"].get();
                cfg.lot_size = f["stepSize"].get();
            }
        }
        cfg.min_volume = 0;
        cfg.lot_multiplier = 1;
        cfg.quantum_factor = 1;
        cfg.quantity_precision = sd["quotePrecision"].get();
        cfg.base_asset_precision = sd["baseAssetPrecision"].get();
        cfg.quote_precision = sd["quotePrecision"].get();
        cfg.quote_asset = sd["quoteAsset"].as<std::string>();

        _new_cache.push_back(std::move(cfg));
    }
}
