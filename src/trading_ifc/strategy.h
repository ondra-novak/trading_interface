#pragma once
#include "strategy_context.h"
#include "config_desc.h"
#include "orderbook.h"

namespace trading_api {



class IStrategy;

using PStrategy = std::unique_ptr<IStrategy>;


class IStrategy {
public:

    virtual ~IStrategy() = default;

    ///clone strategy state (need for speculative simulation)
    virtual PStrategy clone() = 0;

    ///helps to write clone function
    template<typename X>
    static PStrategy do_clone(const X *me) {return std::make_unique<X>(*me);}


    virtual StrategyConfigDesc get_config_desc() const = 0;


    ///called on initialization
    virtual void on_init(Context ctx) = 0;

    ///called at the beginning and when instrument update is requested
    virtual void on_instrument(Instrument i) = 0;

    ///called when account data are updated (must be explicitly requested)
    virtual void on_account(Account a) = 0;

    ///called when ticker changes (market triggers)
    virtual void on_ticker(Instrument i, const Ticker &tk) = 0;

    virtual void on_orderbook(Instrument i, const OrderBook &ord) = 0;

    ///called when time reached on a timer (set_timer)
    virtual void on_timer(TimerID id) = 0;

    ///called when order state is updated (market triggers) and also at the beginning for all open orders
    virtual void on_order(Order ord) = 0;

    ///called when fill is detected
    virtual void on_fill(Order ord, Fill fill) = 0;



};



class IStrategyFactory {
public:
    virtual std::unique_ptr<IStrategy> create_strategy(std::string_view strategy_name) const noexcept = 0;
protected:
    virtual ~IStrategyFactory() = default;
};



using EntryPointFn = const IStrategyFactory * (*)();



}
