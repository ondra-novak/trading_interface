#pragma once
#include "strategy_context.h"
#include "config_desc.h"
#include "orderbook.h"


namespace trading_api {



class IStrategy;

using PStrategy = std::unique_ptr<IStrategy>;


class IStrategy {
public:

    using InstrumentList = std::vector<Instrument>;
    using AccountList = std::vector<Account>;


    static constexpr unsigned int signal_configuration_changed  = 0;


    virtual ~IStrategy() = default;


    virtual StrategyConfigSchema get_config_schema() const = 0;


    ///called on initialization
    virtual void on_init(const Context &ctx) = 0;

    ///called when market event happened
    /**
     * You need to subscribe to given market event. See Context::subscribe
     *
     * @param i instrument
     * @param tk ticker
     */
    virtual void on_market_event(Instrument i, MarketEvent event) = 0;


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
    /**
     * @param ord associated order
     * @param fill recorded fill
     * @return strategy's custom label. This allows to filter fills later. This
     * is also critical to calculate pnl. It is recommended to use different label
     * for different currency
     *
     * @note the label can have a an internal structure.
     *      you can filter labels by a prefix. For example "usd_1234" means
     *      usd for currency and 1234 is custom identifier. Label don't need to be
     *      unique.
     */
    virtual std::string on_fill(Order ord, const Fill &fill) = 0;

    ///called when external signal
    /**
     * @param signal - signal happened. The strategy must use signal.is<name>() test, what
     * signal has been delivered. The signal can carry arguments
     */
    virtual void on_signal(Signal signal) = 0;

    ///called when unhandled exception is detected anywhere in the strategy
    /**
     * Default implementation throws exception back, so it is processed by control object
     */
    virtual void on_unhandled_exception() = 0;
};


class AbstractStrategy: public IStrategy {
public:
    virtual StrategyConfigSchema get_config_schema() const override {
        return {};
    }

    virtual void on_init(const Context &ctx) override = 0;
    virtual void on_market_event(Instrument , MarketEvent ) override {}
    virtual void on_timer(TimerID) override {};
    virtual std::string on_fill(Order, const Fill &) override {return {};}
    virtual void on_order(Order) override {}
    virtual void on_signal(Signal) override {}
    virtual void on_unhandled_exception() override {throw;}
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
