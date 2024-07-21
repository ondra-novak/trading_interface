#pragma once

#include "../../trading_ifc/exchange_service.h"
#include "../../trading_ifc/weak_object_map.h"
#include "secure_rpc.h"

#include "ws_streams.h"

#include "instrument.h"

#include <shared_mutex>
#include <unordered_map>

class BinanceExchange : public trading_api::IExchangeService,
                        public WSStreams::IEvents {
public:

    using Instrument = trading_api::Instrument;
    using Account = trading_api::Account;
    using Order = trading_api::Order;
    using Ticker = trading_api::Ticker;
    using OrderBook = trading_api::OrderBook;

    /*
     * '<instrument_name>' - query single instrument
     * '*' - all instruments
     * 'USDT' - all usdt instruments
     * 'USDC' - all usdc instruments
     * 'COIN' - all coin-m instruments
     */
    virtual void query_instruments(std::string_view query, std::string_view label, Function<void(Instrument)> cb) override;
    /*
     * '*' - all accounts
     * 'USDT' - usdt account
     * 'USDC' - usdc account
     */
    virtual void query_accounts(std::string_view query, std::string_view label, Function<void(Account)> cb) override ;


    virtual trading_api::StrategyConfigSchema get_config_schema() const
            override;
    virtual void init(trading_api::ExchangeContext context,
            const trading_api::StrategyConfig &config) override;

    virtual void subscribe(trading_api::SubscriptionType type,
            const Instrument &i) override;
    virtual void unsubscribe(trading_api::SubscriptionType type,
            const Instrument &i) override;

    virtual Order create_order(
            const Instrument &instrument,
            const trading_api::Account &account,
            const Order::Setup &setup) override;
    virtual void batch_place(std::span<Order> orders) override;
    virtual void order_apply_fill(const Order &order,
            const trading_api::Fill &fill) override;
    virtual void update_account(const trading_api::Account &a) override;
    virtual Order create_order_replace(
            const Order &replace,
            const Order::Setup &setup, bool amend) override;
    virtual std::string get_id() const override;

    virtual std::optional<trading_api::IExchange::Icon> get_icon() const override;
    virtual void batch_cancel(std::span<Order> orders) override;
    virtual void update_instrument(const Instrument &i) override;
    virtual void order_apply_report(const Order &order,
            const Order::Report &report) override;
    virtual std::string get_name() const override;
    virtual void restore_orders(void *context, std::span<trading_api::SerializedOrder>  orders) override;

    static WebSocketContext &get_ws_context();

protected:
    using InstrumentMap = trading_api::WeakObjectMapWithLock<BinanceInstrument>;

    trading_api::ExchangeContext _ctx;
    std::optional<SecureRPCClient::Account> _key;
    std::optional<WSStreams> _public_fstream;
    std::optional<RPCClient> _rpc;
    trading_api::Log _log;

    InstrumentMap _instruments;


    struct InstrumentDefCache {
        std::string _fapi_url;
        std::string _dapi_url;
        std::vector<BinanceInstrument::Config> _cache;
        std::chrono::system_clock::time_point _expires;
        trading_api::Log _log;
        const auto &get_cache() {
            if (std::chrono::system_clock::now() > _expires) {
                reload();
            }
            return _cache;
        }
        void reload();
        void parse_instruments(std::string_view json_data, bool coinm);
    };

    InstrumentDefCache _instrument_def_cache;



    virtual void on_ticker(std::string_view symbol,  const Ticker &ticker) override;
    virtual void on_orderbook(std::string_view symbol,  const OrderBook &update) override;
    virtual void subscribe_result(std::string_view symbol, trading_api::SubscriptionType type, const RPCClient::Result &result) override;


};
