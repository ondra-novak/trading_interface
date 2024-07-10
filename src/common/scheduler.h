#pragma once

#include "priority_queue.h"

#include <chrono>
#include <cstdint>
#include "../trading_ifc/function.h"
#include <mutex>
#include <queue>
#include <vector>
#include <stop_token>
#include <condition_variable>

namespace trading_api {

class Scheduler {
public:
    using TimerID = std::uintptr_t;
    using Timestamp = std::chrono::system_clock::time_point;
    using Runnable = Function<void()>;
    using Notify = Function<void(Timestamp)>;
    using EventSeverityID = int;

    ///enqueue event to queue
    void enqueue(Runnable &r);
    ///enqueue event to queue
    void enqueue(Runnable &&r) {enqueue(r);}
    ///enqueue timed event
    void enqueue(Timestamp tp, Runnable &r, TimerID id);

    void enqueue(Timestamp tp, Runnable &&r, TimerID id) {enqueue(tp,r,id);}
    ///wake up scheduler and borrow current thread
    /**
     * @param cur_time current time
     * @param ntf notification function. This function is called when scheduler
     * needs to update its wake_up time.
     * @return Timestamp of next wakeup. If function needs another wakeup call,
     * it must reurns cur_time or lower time
     *
     * @note this function should be called under strategy lock (each strategy has own scheduler)
     * @note always processes only one event.
     */

    template<std::invocable<Timestamp> NotifyFn>
    Timestamp wakeup(Timestamp cur_time, NotifyFn &&ntf) {
        std::unique_lock lk(_mx);
        return wakeup_internal(lk, cur_time, std::forward<NotifyFn>(ntf));
    }

    bool cancel_timed(TimerID id);

    ///run thread
    void run(std::condition_variable &cond, std::stop_token stop);


protected:

    struct TimedEvent {
        Timestamp tp;
        TimerID id;
        Runnable r;
        struct ordering {
            bool operator()(const TimedEvent &a, const TimedEvent &b) const;
        };
    };

    std::mutex _mx;
    PriorityQueue<TimedEvent, TimedEvent::ordering> _timed_queue;
    Notify _ntf = [](Timestamp) {};
    Timestamp _next_notify = Timestamp::max();

    void notify();
    template<std::invocable<Timestamp> NotifyFn>
    Timestamp wakeup_internal(std::unique_lock<std::mutex> &lk, Timestamp cur_time, NotifyFn &&ntf) {
        Timestamp retval = wakeup_internal(lk, cur_time);
        if (retval  <= cur_time) return retval;
        _ntf = std::forward<NotifyFn>(ntf);
        return retval;
    }
    Timestamp wakeup_internal(std::unique_lock<std::mutex> &lk, Timestamp cur_time);
    Timestamp calc_next_notify() const;

    void enqueue_lk(Runnable &r);
    void enqueue_lk(Runnable &&r) {enqueue_lk(r);}
};



}
