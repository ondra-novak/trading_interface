#pragma once

#include "event_target.h"

#include "scheduler.h"

#include <mutex>
#include "../trading_ifc/strategy.h"

namespace trading_api {

class BasicContext: public IContext, public IEventTarget, public std::enable_shared_from_this<BasicContext> {
public:

    void init(std::unique_ptr<IStrategy> strategy,
            IStrategy::InstrumentList instruments,
            Scheduler &global_scheduler);


    virtual void on_event(Order ord, Fill fill) override;
    virtual void on_event(Order ord) override;
    virtual void on_event(Instrument i, const OrderBook &ord) override;
    virtual void on_event(Instrument i) override;
    virtual void on_event(Instrument i, const Ticker &tk) override;
    virtual void on_event(Account a) override;
    virtual void subscribe(SubscriptionType type, const Instrument &i, TimeSpan interval)override;
    virtual Order replace(const Order &order, const Order::Setup &setup, bool amend) override;
    virtual Value get_param(std::string_view varname) const override;
    virtual Fills get_fills(std::size_t limit) override;
    virtual Order place(const Instrument &instrument, const Order::Setup &setup) override;
    virtual void cancel(const Order &order) override;
    virtual TimerID set_timer(Timestamp at, CompletionCB fnptr) override;
    virtual void unsubscribe(SubscriptionType type, const Instrument &i) override;
    virtual Timestamp now() const override;
    virtual Order create(const Instrument &instrument) override;
    virtual void update_account(const Account &a, CompletionCB complete_ptr) override;
    virtual void allocate(const Account &a, double equity) override;
    virtual bool clear_timer(TimerID id) override;
    virtual Value get_var(int idx) const override;
    virtual void update_instrument(const Instrument &i, CompletionCB complete_ptr) override;
    virtual void set_var(int idx, const Value &val) override;

protected:
    std::mutex _mx;
    std::unique_ptr<IStrategy> _strategy;
    Scheduler _sch;
    Scheduler &global_scheduler;
    TimerID _sch_id = 0;

    void reschedule(Timestamp tp);



};


}
