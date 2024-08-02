#pragma once

#include "../../trading_ifc/exchange_service.h"
#include "../../trading_ifc/weak_object_map.h"
#include "../../trading_ifc/shared_lockable_ptr.h"

#include "ws_streams.h"
#include "stream_map.h"
#include "instrument.h"
#include "instrument_def_cache.h"

#include "rest_client.h"

#include "account.h"

#include <shared_mutex>
#include <unordered_map>

class BinanceExchange : public trading_api::IExchangeService,
                        public WSStreams::IEvents,
                        public RPCClient::IThreadMonitor
                        {
public:

    using Instrument = trading_api::Instrument;
    using Account = trading_api::Account;
    using Order = trading_api::Order;
    using Ticker = trading_api::TickData;
    using OrderBook = trading_api::OrderBook;
    using ConfigSchema = trading_api::ConfigSchema;
    using Config = trading_api::Config;

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
    virtual void query_accounts(std::string_view api_key_name, std::string_view query, std::string_view label, Function<void(Account)> cb) override ;


    virtual ConfigSchema get_exchange_config_schema() const
            override;
    virtual void init(trading_api::ExchangeContext context, const Config &config) override;

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
    virtual ConfigSchema get_api_key_config_schema() const override;
    virtual void unset_api_key(std::string_view name) override;
    virtual void set_api_key(std::string_view name,
            const trading_api::Config &api_key_config) override;

protected:
    
    struct IdentityInfo {
        PIdentity api_key;
        std::unique_ptr<WSStreams> _stream;
    };

    using IdentityList = std::map<std::string, IdentityInfo, std::less<> >;
    using PIdentityList = shared_lockable_ptr<IdentityList>;
    using InstrumentMap = trading_api::WeakObjectMapWithLock<BinanceInstrument>;
    using AccountMap = trading_api::WeakObjectMapWithLock<BinanceAccount>;

    trading_api::ExchangeContext _ctx;
    PIdentityList _identities = make_shared_lockable<IdentityList>();


    std::optional<WebSocketContext> _ws_context;
    std::optional<RestClientContext> _rest_context;
    std::unique_ptr<WSStreams> _public_fstream;
    std::unique_ptr<RestClient> _frest;
    std::unique_ptr<StreamMap> _stream_map;
    std::jthread _stream_wrk;
    trading_api::Log _log;


    InstrumentMap _instruments;
    AccountMap _accounts;



    InstrumentDefCache _instrument_def_cache;

    

    virtual void on_ticker(std::string_view symbol,  const Ticker &ticker) override;
    virtual void on_orderbook(std::string_view symbol,  const OrderBook &update) override;
    virtual void on_reconnect(std::string reason) override;
    virtual void on_ping() override;
    virtual void on_order(const json::value &json_data) override;
    virtual void on_stream_error(const RPCClient::Result &res) override;


    PIdentity find_identity(const std::string &ident) const;

    void update_account(const std::shared_ptr<BinanceAccount> &acc,
            const json::value &asset_info, const json::value &positions);


    void refresh_listenkeys();
    
};
