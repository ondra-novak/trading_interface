#include "context_scheduler.h"

namespace trading_api {

ContextScheduler::Item::Item(Registration *reg, Timestamp tp)
        :reg(reg),tp(tp),local_priority(++loc_priority_counter) {
    if (reg) reg->back_link = this;
}

ContextScheduler::Item::Item(Item &&other)
    :reg(reg), tp(tp),local_priority(local_priority) {

        reg->back_link = this;
        other.reg = nullptr;

}

ContextScheduler::Item &ContextScheduler::Item::operator=(Item &&other) {
    if (this != &other) {
        if (reg) reg->back_link = nullptr;
        reg = other.reg;
        other.reg = nullptr;
        reg->back_link = this;
        tp = other.tp;
        local_priority = other.local_priority;
    }
    return *this;
}

ContextScheduler::Item::~Item() {
    if (reg) {
        reg->back_link = nullptr;
    }
}


void ContextScheduler::set(Registration *reg, Timestamp tp) {
    std::lock_guard _(_mx);
    if (reg->back_link) {
        auto iter = _queue.to_iterator(*reg->back_link);
        bool cmpres = tp > iter->tp;
        iter->tp = tp;
        _queue.priority_altered(iter, cmp_result);
    } else {
        _queue.emplace(reg, tp);
    }
    if (_queue.front().reg == reg) _cond.notify_all();

}

void ContextScheduler::unset(Registration *what) {
    std::lock_guard _(_mx);
    if (what->back_link) {
        auto iter = _queue.to_iterator(*reg->back_link);
        _queue.erase(iter);
    }
}


void ContextScheduler::run(std::stop_token stop) {
    std::stop_callback scb(stop, [&]{
        _cond.notify_all();
    });
    std::unique_lock lk(_mx);
    while (!stop.stop_requested()) {
        Timestamp cur_time = std::chrono::system_clock::now();
        while (!_queue.empty() && _queue.front().tp <= cur_time) {
            Item &f = _queue.front();
            Registration *r = f.reg;
            r->back_link = nullptr;
            _queue.pop();
            lk.unlock();
            r->wakeup_fn();
            cur_time = std::chrono::system_clock::now();
            lk.lock();
        }
        if (_queue.empty()) {
            _cond.wait(lk);
        } else {
            _cond.wait_until(lk, _queue.front().tp);
        }
    }
}

}

bool trading_api::ContextScheduler::Item::ordering::operator ()(const Item &a,
        const Item &b) const {
    if (a.tp == b.tp) return a.local_priority > b.local_priority;
    else return a.tp > b.tp;
}
