#pragma once

#include "event_target.h"


#include "scheduler.h"
#include "../trading_ifc/strategy.h"
#include "context_scheduler.h"
#include "storage.h"

#include <mutex>
#include <map>

namespace trading_api {

class BasicContext: public IContext, public IEventTarget {
public:

    static constexpr Scheduler::EventSeverityID ticker_id = 1;
    static constexpr Scheduler::EventSeverityID orderbook_id = 2;


    BasicContext() = default;
    ~BasicContext();

    void init(std::unique_ptr<IStrategy> strategy,
            IStrategy::InstrumentList instruments,
            ContextScheduler *ctx_sch,
            std::shared_ptr<IStorage> storage,
            const Config &config);


    virtual void on_event(Order ord, const Fill &fill) override;
    virtual void on_event(Order ord) override;
    virtual void on_event(Instrument i, const OrderBook &ord) override;
    virtual void on_event(Instrument i) override;
    virtual void on_event(Instrument i, const Ticker &tk) override;
    virtual void on_event(Account a) override;
    virtual void subscribe(SubscriptionType type, const Instrument &i, TimeSpan interval)override;
    virtual Order replace(const Order &order, const Order::Setup &setup, bool amend) override;
    virtual Fills get_fills(std::size_t limit) override;
    virtual Order place(const Instrument &instrument, const Order::Setup &setup) override;
    virtual void cancel(const Order &order) override;
    virtual TimerID set_timer(Timestamp at, CompletionCB fnptr) override;
    virtual void unsubscribe(SubscriptionType type, const Instrument &i) override;
    virtual Timestamp now() const override;
    virtual Order bind_order(const Instrument &instrument) override;
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
    ContextScheduler *_global_scheduler = nullptr;
    ContextScheduler::Registration _reg;
    std::shared_ptr<IStorage> _storage;


    Timestamp _cur_time;



    std::mutex _cb_mx;
    std::multimap<Account, CompletionCB> _cb_update_account;
    std::multimap<Instrument, CompletionCB> _cb_update_instrument;
    std::vector<CompletionCB> _cb_tmp;



    void reschedule(Timestamp tp);



};


}
