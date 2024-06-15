#pragma once
#include "strategy_context.h"

namespace trading_ifc {

class StrategyContext;

class AccountState;




class IStrategy {
public:

    virtual ~IStrategy() = default;

    ///clone strategy state (need for speculative simulation)
    virtual std::unique_ptr<IStrategy> clone() = 0;

    ///helps to write clone function
    template<typename X>
    static std::unique_ptr<X> *do_clone(const X *me) {return std::make_unique<X>(*me);}



    ///called on initialization
    virtual void on_init(StrategyContext ctx) = 0;

    ///called at the beginning and when instrument update is requested
    virtual void on_instrument(Instrument i) = 0;

    ///called when account data are updated (must be explicitly requested)
    virtual void on_account(Account a) = 0;

    ///called when ticker changes (market triggers)
    virtual void on_ticker(Ticker tk) = 0;

    ///called when time reached on a timer (set_timer)
    virtual void on_timer(TimerID id) = 0;

    ///called when order state is updated (market triggers) and also at the beginning for all open orders
    virtual void on_order(Order ord) = 0;

    ///called when fill is detected
    virtual void on_fill(Order ord, Fill fill) = 0;


};




}
