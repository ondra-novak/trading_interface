#include "scheduler.h"

namespace trading_api {

void Scheduler::enqueue(Runnable r) {
    std::lock_guard _(_mx);
    _queue.push(std::move(r));
    notify();

}

void Scheduler::enqueue_collapse(EventSeverityID event_id, Runnable r) {
    std::lock_guard _(_mx);
    auto iter = std::find_if(_collapsable_queue.begin(), _collapsable_queue.end(),
            [&](const CollapsableEvent &ev){return ev.event_id == event_id;});
    if (iter != _collapsable_queue.end()) {
        iter->r = std::move(r);
    } else {
        _collapsable_queue.push_back(CollapsableEvent{event_id, std::move(r)});
        std::push_heap(_collapsable_queue.begin(), _collapsable_queue.end(), CollapsableEvent::ordering);
        notify();
    }

}

Scheduler::TimerID Scheduler::enqueue_timed(Timestamp tp, Runnable r) {
    std::lock_guard _(_mx);
    TimerID id = ++_id_counter;
    _timed_queue.push_back(TimedEvent{tp, id, std::move(r)});
    std::push_heap(_timed_queue.begin(), _timed_queue.end(), TimedEvent::ordering);
    notify();
    return id;
}


bool Scheduler::cancel_timed(TimerID id) {
    std::lock_guard _(_mx);
    auto iter = std::find_if(_timed_queue.begin(), _timed_queue.end(),
            [&](const TimedEvent &ev){return ev.id == id;});
    if (iter == _timed_queue.end()) return false;
    if (iter == _timed_queue.begin()) {
        std::pop_heap(_timed_queue.begin(), _timed_queue.end(), TimedEvent::ordering);
        _timed_queue.pop_back();
        return true;
    } else {
        TimedEvent &a = *iter;
        TimedEvent &b = _timed_queue.back();
        if (&a != &b) {
            std::swap(a,b);
            _timed_queue.pop_back();
            std::make_heap(_timed_queue.begin(), _timed_queue.end(), TimedEvent::ordering);
        } else {
            _timed_queue.pop_back();
        }
        return true;
    }

}

bool Scheduler::TimedEvent::ordering(const TimedEvent &a, const TimedEvent &b) {
    return a.tp > b.tp;
}

Scheduler::Timestamp Scheduler::calc_next_notify() const {
    if (_queue.empty() && _collapsable_queue.empty()) {
        if (_timed_queue.empty()) return Timestamp::max();
        else return _timed_queue.front().tp;
    } else {
        return Timestamp::min();
    }
}

void Scheduler::notify() {
   Timestamp newntf  = calc_next_notify();
   if (newntf < _next_notify) {
       _next_notify = newntf;
       _ntf(newntf);
   }
}

Scheduler::Timestamp Scheduler::wakeup_internal(std::unique_lock<std::mutex> &lk, Timestamp cur_time) {
    if (!_queue.empty()) {
        Runnable r = std::move(_queue.front());
        lk.unlock();
        r();
        lk.lock();
    } else if (_collapsable_queue.empty()) {
        Runnable r = std::move(_collapsable_queue.front().r);
        std::pop_heap(_collapsable_queue.begin(), _collapsable_queue.end(), CollapsableEvent::ordering);
        _collapsable_queue.pop_back();
        lk.unlock();
        r();
        lk.lock();
    } else if (!_timed_queue.empty() && _timed_queue.front().tp <= cur_time) {
        Runnable r = std::move(_timed_queue.front().r);
        std::pop_heap(_timed_queue.begin(), _timed_queue.end(), TimedEvent::ordering);
        _collapsable_queue.pop_back();
        lk.unlock();
        r();
        lk.lock();
    }
    _next_notify = calc_next_notify();
    return _next_notify;
}

bool Scheduler::CollapsableEvent::ordering(
        const CollapsableEvent &a, const CollapsableEvent &b) {
    return a.event_id < b.event_id;
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





}

