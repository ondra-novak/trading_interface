#include "instrument_def_cache.h"

bool InstrumentDefCache::need_reload() const {
    std::lock_guard _(_mx);
    return std::chrono::system_clock::now() > _expires;
}

void InstrumentDefCache::reload(RestClient &client, bool coinm) {
    using namespace std::placeholders;
    std::lock_guard _(_mx);
    ++_pending_counter;
    client.public_call("/v1/exchangeInfo", {}, [this,coinm](const RestClient::Result &res) mutable {
        process(res,coinm);
    });
}


std::optional<BinanceInstrument::Config> InstrumentDefCache::find(std::string_view symbol) const {
    std::lock_guard _(_mx);
    auto iter =std::lower_bound(_instruments.begin(), _instruments.end(),
            BinanceInstrument::Config{.id = std::string(symbol)}, &sort_order);
    if (iter != _instruments.end() && iter->id == symbol) {
        return {*iter};
    } else {
        return {};
    }

}

std::vector<BinanceInstrument::Config> InstrumentDefCache::query(std::string_view query_str) const {
    std::lock_guard _(_mx);
    if (query_str == "*") return _instruments;
    auto iter =std::lower_bound(_instruments.begin(), _instruments.end(),
            BinanceInstrument::Config{.id = std::string(query_str)}, &sort_order);
    if (iter != _instruments.end() && iter->id == query_str) {
        return {*iter};
    }
    return {};
}

void InstrumentDefCache::process(const RestClient::Result &result, bool coinm) {

    std::vector<BinanceInstrument::Config> new_cache;

    if (result.is_error()) {
        {
            std::lock_guard _(_mx);
            _last_error = result.content;
        }
        end_reload();
        return;
    } else {

        for (json::value sd: result.content["symbols"]) {
            BinanceInstrument::Config cfg;
            cfg.id = sd["symbol"].as<std::string>();
            cfg.can_short = true;
            cfg.type = coinm?trading_api::Instrument::Type::inverted_contract:
                             trading_api::Instrument::Type::contract;
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
            cfg.base_asset = sd["baseAsset"].as<std::string>();

            new_cache.push_back(std::move(cfg));
        }

        std::sort(new_cache.begin(), new_cache.end(), &sort_order);
        {
            std::lock_guard _(_mx);
            auto old_cache = std::move(_instruments);
            std::set_union(
                    new_cache.begin(), new_cache.end(),
                    old_cache.begin(), old_cache.end(),
                    std::back_inserter(_instruments),
                    &sort_order);
        }
    }
    end_reload();
}

bool InstrumentDefCache::sort_order(const BinanceInstrument::Config &a,
        const BinanceInstrument::Config &b) {
    return a.id.compare(b.id) < 0;
}
