#pragma once

#include "event_target.h"

#include "scheduler.h"

#include <mutex>
#include "../trading_ifc/strategy.h"

namespace trading_api {

class BasicContext: public IContext, public IEventTarget, public std::enable_shared_from_this<BasicContext> {
public:

    void init(std::unique_ptr<IStrategy> strategy, std::vector<Instrument> instruments);


    virtual void on_event(Order ord, Fill fill) override;
    virtual void on_event(Order ord) override;
    virtual void on_event(Instrument i, const OrderBook &ord) override;
    virtual void on_event(Instrument i) override;
    virtual void on_event(Instrument i, const Ticker &tk) override;
    virtual void on_event(Account a) override;

protected:
    std::mutex _mx;
    std::unique_ptr<IStrategy> _strategy;
    Scheduler _sch;

};


}
