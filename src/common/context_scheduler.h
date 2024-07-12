#pragma once

#include "../trading_ifc/function.h"
#include "../trading_ifc/timer.h"
#include "priority_queue.h"

#include <mutex>
#include <functional>
#include <condition_variable>

namespace trading_api {



template<typename SchedulerType>
class ContextScheduler {
public:
    ContextScheduler() = default;
    ContextScheduler(std::shared_ptr<SchedulerType> sch):_scheduler(std::move(sch)) {}
    ~ContextScheduler();

    void operator()(Timestamp tm, Function<void(Timestamp)> fn, const void *ident);

protected:
    std::shared_ptr<SchedulerType> _scheduler;

};


class SingleThreadScheduler;
class ManualControlScheduler;

class ManualContextScheduler: public ContextScheduler<ManualControlScheduler> {
public:
    void set_time(Timestamp tp);
};
using SingleThreadContextScheduler = ContextScheduler<SingleThreadScheduler>;

using ContextSchedulerGeneric = std::function<void(Timestamp tm, Function<void(Timestamp)> fn, const void *ident)>;

///create single threaded scheduler
SingleThreadContextScheduler create_scheduler();
///create manual scheduler
/**
 * You need store ManualContextScheduler object to able to set time. However this
 * object can be converted to ContextSchedulerGeneric
 * @return
 *
 * @note manual scheduler is not thread safe
 */
ManualContextScheduler create_scheduler_manual();



}
