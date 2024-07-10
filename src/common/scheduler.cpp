#include "scheduler.h"

namespace trading_api {

void Scheduler::enqueue(Runnable &r) {
    std::lock_guard _(_mx);
    enqueue_lk(r);

}


void Scheduler::enqueue(Timestamp tp, Runnable &r, TimerID id) {
    std::lock_guard _(_mx);
    _timed_queue.push({tp, id, std::move(r)});
    notify();
}


bool Scheduler::cancel_timed(TimerID id) {
    std::lock_guard _(_mx);
    auto iter = std::find_if(_timed_queue.begin(), _timed_queue.end(),
            [&](const TimedEvent &ev){return ev.id == id;});
    if (iter == _timed_queue.end()) return false;
    _timed_queue.erase(iter);
    return true;
}

bool Scheduler::TimedEvent::ordering::operator()(const TimedEvent &a, const TimedEvent &b) const {
    return a.tp > b.tp;
}

Scheduler::Timestamp Scheduler::calc_next_notify() const {
    if (_timed_queue.empty()) return Timestamp::max();
    else return _timed_queue.front().tp;
}

void Scheduler::notify() {
   Timestamp newntf  = calc_next_notify();
   if (newntf < _next_notify) {
       _next_notify = newntf;
       _ntf(newntf);
   }
}

Scheduler::Timestamp Scheduler::wakeup_internal(std::unique_lock<std::mutex> &lk, Timestamp cur_time) {
    if (!_timed_queue.empty() && _timed_queue.front().tp <= cur_time) {
        Runnable r = std::move(_timed_queue.front().r);
        _timed_queue.pop();
        lk.unlock();
        r();
        lk.lock();
    }
    _next_notify = calc_next_notify();
    return _next_notify;
}


void Scheduler::run(std::condition_variable &cond, std::stop_token stop) {
    std::stop_callback __(stop, [&]{
        cond.notify_all();
    });
    std::unique_lock lk(_mx);
    Timestamp nx;
    while (!stop.stop_requested()) {
        nx = wakeup_internal(lk,std::chrono::system_clock::now(),[&](Timestamp newnx) {
            if (newnx < nx) {
                cond.notify_all();
            }
        });
        if (nx == Timestamp::max()) {
            cond.wait(lk);
        } else {
            cond.wait_until(lk, nx);
        }
    }
    wakeup_internal(lk, std::chrono::system_clock::now(), [](auto){}); //disarm wakeup
}


void Scheduler::enqueue_lk(Runnable &r) {
    _timed_queue.push({Timestamp::min(), 0, std::move(r)});
    notify();

}




}

