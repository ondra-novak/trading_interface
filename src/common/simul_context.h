#pragma once
#include "../trading_ifc/strategy.h"

namespace trading_api {

class SimulContext: public IContext, public std::enable_shared_from_this<SimulContext> {
public:

    SimulContext(PStrategy strategy):_strategy(std::move(strategy)) {}

    void init(Timestamp time);

    void set_now(Timestamp time);


    virtual Timestamp now() const override;
    virtual TimerID set_timer(Timestamp at) override;
    virtual bool clear_timer(TimerID id) override;

protected:

    using Timer = std::pair<Timestamp, TimerID>;
    using Scheduler = std::vector<Timer>;

    PStrategy _strategy;
    Timestamp _cur_time;
    TimerID _next_id = 0;
    Scheduler _scheduler;
    static bool timer_order(const Timer &a, const Timer &b);



};


}
