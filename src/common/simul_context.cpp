#include "simul_context.h"

#include "../trading_ifc/strategy_context.h"

namespace trading_api {

Timestamp SimulContext::now() const {
    return _cur_time;
}

trading_api::TimerID SimulContext::set_timer(Timestamp at) {
    TimerID id = ++_next_id;
    _scheduler.push_back({at, id});
    std::push_heap(_scheduler.begin(), _scheduler.end(), timer_order);
    return id;
}

void SimulContext::init(Timestamp time) {
    _cur_time = time;
    _strategy->on_init(Context(shared_from_this()));
}

void SimulContext::set_now(Timestamp time) {
    while (!_scheduler.empty() && _scheduler.front().first <= time) {
        _cur_time = std::max(_cur_time, _scheduler.front().first);
        TimerID id = _scheduler.front().second;
        std::pop_heap(_scheduler.begin(), _scheduler.end(), timer_order);
        _scheduler.pop_back();
        _strategy->on_timer(id);
    }
    _cur_time = time;
}

bool SimulContext::clear_timer(trading_api::TimerID id) {
    auto iter = std::find_if(_scheduler.begin(), _scheduler.end(), [&](const Timer &tm){
        return tm.second == id;
    });
    if (iter == _scheduler.end()) return false;
    if (iter == _scheduler.begin()) {
        std::pop_heap(_scheduler.begin(), _scheduler.end(), timer_order);
        _scheduler.pop_back();
    } else {
        std::swap(*iter, _scheduler.back());
        _scheduler.pop_back();
        std::make_heap(_scheduler.begin(), _scheduler.end(), timer_order);
    }
    return true;
}

};


