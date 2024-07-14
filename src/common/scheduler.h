#pragma once

#include "priority_queue.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

namespace trading_api {

struct SchedulerDefaultTraits {
    using TimePoint = std::chrono::system_clock::time_point;
    using Ident = const void *;
    static constexpr void notify(TimePoint) {/*empty*/}
    template<typename Subj, typename Tp, typename Id>
    static constexpr void execute(Subj && subj, Tp && tp, Id && id) {
        if constexpr(std::is_invocable_v<Subj, Tp, Id>) {
            std::forward<Subj>(subj)(std::forward<Tp>(tp), std::forward<Id>(id));
        } else if constexpr(std::is_invocable_v<Subj, Tp>) {
            std::forward<Subj>(subj)(std::forward<Tp>(tp));
        } else if constexpr(std::is_invocable_v<Subj, Id>) {
            std::forward<Subj>(subj)(std::forward<Id>(id));
        } else {
            static_assert(std::is_invocable_v<Subj>);
            std::forward<Subj>(subj)();
        }
    }

};


template<typename T, typename Traits = SchedulerDefaultTraits>
class Scheduler {
public:

    using value_type = T;
    using TimePoint = typename std::decay_t<Traits>::TimePoint;
    using Ident = typename std::decay_t<Traits>::Ident;


    constexpr Scheduler() = default;
    constexpr Scheduler(Traits traits) : _traits(std::move(traits)) {}

    ///schedule operation at
    constexpr void insert(TimePoint tp, value_type item, Ident ident = {}) {
        _queue.push(Item{std::move(tp), std::move(item), std::move(ident)});
    }

    ///replace scheduled operation
    constexpr void replace(TimePoint tp, value_type item, Ident ident = {}) {
        auto iter = std::find_if(_queue.begin(), _queue.end(), [&](const Item &item){
            return item.ident == ident;
        });
        if (iter == _queue.end()) {
            _queue.push(Item{std::move(tp), std::move(item), std::move(ident)});
        } else {
            reschedule(iter, tp, item);
        }
    }

    ///update existing operation
    /**
     * @param tp new time
     * @param item new item
     * @param ident identification
     * @retval true updated
     * @retval false not exists
     */
    constexpr bool update(TimePoint tp, value_type item, Ident ident = {}) {
        auto iter = std::find_if(_queue.begin(), _queue.end(), [&](const Item &item){
            return item.ident == ident;
        });
        if (iter == _queue.end()) return false;
        reschedule(iter, tp, item);
        return true;
    }


    template<typename Dur>
    constexpr void insert_after(const Dur &dur, value_type item, Ident ident = {}) {
        at(_cur_time + dur, std::move(item), std::move(ident));
    }

    constexpr std::optional<TimePoint> get_next_event() const {
        return _queue.empty()?std::optional<TimePoint>():std::optional<TimePoint>(_queue.front().tp);
    }

    constexpr void set_time(TimePoint tm) {
        while (!_queue.empty() && !(_queue.front().tp > tm)) {
            Ident ident ( std::move(_queue.front().ident));
            T subj ( std::move(_queue.front().object));
            _cur_time = std::max(_cur_time, _queue.front().tp);
            const TimePoint &now = _cur_time;
            _queue.pop();
            _traits.execute(std::move(subj), now, std::move(ident));
        }
        _cur_time = std::max(_cur_time, _queue.front().tp);
    }

    constexpr bool erase(Ident ident) {
        auto iter = std::find_if(_queue.begin(), _queue.end(), [&](const Item &item){
            return item.ident == ident;
        });
        if (iter != _queue.end()) {
            _queue.erase(iter);
            return true;
        }
        return false;


    }

    TimePoint get_current_time() const {
        return _cur_time;
    }

protected:
    struct Item {
        TimePoint tp;
        T object;
        Ident ident;
        struct ordering {
            constexpr bool operator()(const Item &a, const Item &b) const {
                return a.tp > b.tp;
            }
        };

    };
    using PQueue = PriorityQueue<Item, typename Item::ordering> ;


    constexpr void reschedule(typename PQueue::Super::const_iterator iter, TimePoint &tp, value_type &item) {
        _queue.replace(iter, Item{std::move(tp), std::move(item), iter->ident});
    }


    Traits _traits = {};
    TimePoint _cur_time = {};
    PQueue _queue;


};

template<typename ID = const void *>
struct SchedulerRealTimeTraits {

    using TimePoint = std::chrono::system_clock::time_point;
    using Ident = ID;

    std::mutex _mx;
    std::condition_variable _cond;
    std::optional<TimePoint> _wk_time;
    static thread_local const void *sch_ident;

    void notify(TimePoint tp) {
        std::lock_guard _(_mx);
        if (_wk_time.has_value() && *_wk_time > tp) {
            _wk_time = tp;
            _cond.notify_all();
        }
    }

    template<typename Subj, typename Tp, typename Id>
    void execute_nx(Subj && subj, Tp && tp, Id && id)  noexcept {
        SchedulerDefaultTraits::execute(std::forward<Subj>(subj), std::forward<Tp>(tp), std::forward<Id>(id));
    }


    template<typename Subj, typename Tp, typename Id>
    void execute(Subj && subj, Tp && tp, Id && id)  {
        _mx.unlock();
        execute_nx(std::forward<Subj>(subj), std::forward<Tp>(tp), std::forward<Id>(id));
        if (sch_ident == nullptr) throw sch_ident;
        _mx.lock();
    }

};

template<typename _Subj, typename _Ident = const void * >
class SchedulerRealTime {
public:

    using value_type = _Subj;

    static thread_local bool _scheduler_thread;

    struct Traits {
        std::unique_lock<std::mutex> *ulk = nullptr;
        using TimePoint = std::chrono::system_clock::time_point;
        using Ident = _Ident;

        template<typename Subj, typename Tp, typename Id>
        static constexpr void execute_nx(Subj && subj, Tp && tp, Id && id) noexcept {
            SchedulerDefaultTraits::execute(std::forward<Subj>(subj),
                    std::forward<Tp>(tp), std::forward<Id>(id));
        }

        template<typename Subj, typename Tp, typename Id>
        void execute(Subj && subj, Tp && tp, Id && id)  {
            ulk->unlock();
            execute_nx(std::forward<Subj>(subj),
                    std::forward<Tp>(tp), std::forward<Id>(id));
            if (!_scheduler_thread) throw _scheduler_thread;
            ulk->lock();
        }
};

    using TimePoint = typename Traits::TimePoint;
    using Ident = typename Traits::Ident;

    void insert(TimePoint tp, value_type item, Ident ident = {}) {
        std::lock_guard _(_mx);
        _scheduler.insert(std::move(tp), std::move(item), std::move(ident));
        notify();
    }

    void replace(TimePoint tp, value_type item, Ident ident = {}) {
        std::lock_guard _(_mx);
        _scheduler.replace(std::move(tp), std::move(item), std::move(ident));
        notify();
    }

    bool update(TimePoint tp, value_type item, Ident ident = {}) {
        std::lock_guard _(_mx);
        bool r = _scheduler.update(std::move(tp), std::move(item), std::move(ident));
        if (r) notify();
        return r;
    }

    template<typename A, typename B>
    void insert_after(std::chrono::duration<A,B> dur, value_type item, Ident ident = {}) {
        std::lock_guard _(_mx);
        _scheduler.insert_after(std::move(dur), std::move(item), std::move(ident));
        notify();
    }

    bool erase(Ident ident) {
        std::lock_guard _(_mx);
        return _scheduler.erase(ident);
    }



    SchedulerRealTime(): _scheduler(Traits{&_wrk_lk}) {}


protected:
    Scheduler<_Subj, Traits> _scheduler;
    std::mutex _mx;
    std::unique_lock<std::mutex> _wrk_lk = {_mx, std::defer_lock};
    std::condition_variable _cond;
    std::thread _thread;
    bool _stop = false;
    TimePoint _wk_time = TimePoint::max();

    void notify() {
        auto next = _scheduler.get_next_event();
        if (next.has_value() && *next < _wk_time) {
            _wk_time = *next;
            _cond.notify_all();
            if (!_thread.joinable()) {
                _thread = std::thread([this]{
                   _scheduler_thread = true;
                   worker();
                });
            }
        }
    }

    void worker() noexcept {
        try {
            _wrk_lk.lock();
            while (!_stop) {
                _scheduler.set_time(std::chrono::system_clock::now());
                auto nx = _scheduler.get_next_event();
                if (nx.has_value()) _cond.wait_until(_wrk_lk, *nx);
                else _cond.wait(_wrk_lk);
            }
        } catch (...) {

        }
    }

};


template<typename _Subj, typename _Ident>
inline thread_local bool SchedulerRealTime<_Subj, _Ident>::_scheduler_thread = false;

}
