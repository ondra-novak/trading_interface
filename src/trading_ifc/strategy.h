#pragma once
#include "strategy_context.h"
#include "config_desc.h"
#include "orderbook.h"
#include "module_decl.h"


namespace trading_api {



class IStrategy;

using PStrategy = std::unique_ptr<IStrategy>;


class IStrategy {
public:

    using InstrumentList = std::vector<Instrument>;
    using AccountList = std::vector<Account>;

    using SignalNR = unsigned int;

    static constexpr unsigned int signal_configuration_changed  = 0;


    virtual ~IStrategy() = default;


    virtual StrategyConfigSchema get_config_schema() const = 0;

    struct Configuration {
        AccountList accounts;
        InstrumentList instruments;
        StrategyConfig config;
    };


    ///called on initialization
    virtual void on_init(const Context &ctx, const Configuration &config) = 0;

    ///called when ticker changes (market triggers)
    /**
     * @param i instrument
     * @param tk ticker
     */
    virtual void on_ticker(Instrument i, Ticker tk) = 0;

    ///called when orderbook update
    /**
     * @param i instrument
     * @param ord orderbook
     */
    virtual void on_orderbook(Instrument i, OrderBook ord) = 0;

    ///called when time reached on a timer (set_timer)
    /**
     * Called only when set_timer is used without a callback function, otherwise
     * the callback function is called.
     * @param id
     */
    virtual void on_timer(TimerID id) = 0;

    ///called when order state is updated (market triggers) and also at the beginning for all open orders
    virtual void on_order(Order ord) = 0;

    ///called when fill is detected
    virtual void on_fill(Order ord, Fill fill) = 0;

    ///called when external signal
    /**
     * @param signalnr - signal number. There is predefined one signalnr = 0, when configuration changed
     */
    virtual void on_signal(SignalNR signalnr) = 0;

};


class AbstractStrategy: public IStrategy {
public:
    virtual StrategyConfigSchema get_config_schema() const override {
        return {};
    }

    virtual void on_init(const Context &ctx, const Configuration &config) override = 0;
    virtual void on_orderbook(Instrument, OrderBook ) override {}
    virtual void on_timer(TimerID) override {};
    virtual void on_ticker(Instrument, Ticker ) override {}
    virtual void on_fill(Order, Fill) override {}
    virtual void on_order(Order) override {}
    virtual void on_signal(SignalNR) override {}
};


///Export the strategy, so the strategy can be loaded by the loader
/**
 * @param class_name name of the strategy (class name). The strategy is
 * registered under its name
 */
#define EXPORT_STRATEGY(class_name) ::trading_api::IModule::Factory<IStrategy> strategy_reg_##class_name(#class_name, std::in_place_type<class_name>)

///Export the strategy, but specify other name
/**
 * @param class_name class name of strategy
 * @param export_name exported name
 */
#define EXPORT_STRATEGY_AS(class_name, export_name) ::trading_api::IModule::Factory<IStrategy> strategy_reg_##class_name(export_name, std::in_place_type<class_name>)








}
