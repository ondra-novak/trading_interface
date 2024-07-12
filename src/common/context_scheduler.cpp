#include "context_scheduler.h"

#include <mutex>
#include <thread>
#include <condition_variable>
#include <iostream>
namespace trading_api {


struct Item {
    Timestamp tp;
    Function<void(Timestamp)> fn;
    const void *ident;
    struct ordering {
        bool operator()(const Item &a, const Item &b) const {
            return a.tp > b.tp;
        }
    };
};


static void reschedule_queue(PriorityQueue<Item, Item::ordering> &queue,const Timestamp &tm, Function<void(Timestamp)> &fn, const void *ident) { // @suppress("Unused static function")
    auto iter = std::find_if(queue.begin(), queue.end(), [&](const Item &item){
        return item.ident == ident;
    });
    if (iter == queue.end()) {
        queue.push(Item{tm, std::move(fn), ident});
    } else {
        bool lower = tm > iter->tp;
        iter->tp = tm;
        iter->fn = std::move(fn);
        queue.priority_altered(iter, lower);
    }
}



class ManualControlScheduler {
public:

    void reschedule(const Timestamp &tm, Function<void(Timestamp)> &fn, const void *ident) {
        reschedule_queue(_queue, tm, fn, ident);
    }

    void set_time(Timestamp tp) {
        while (!_queue.empty() && _queue.front().tp < tp) {
            auto fn = std::move(_queue.front().fn);
            _cur_time = std::max(_cur_time,_queue.front().tp);
            _queue.pop();
            fn(_cur_time);
        }
        _cur_time =  std::max(_cur_time,tp);
    }

protected:


    Timestamp _cur_time = Timestamp::min();
    PriorityQueue<Item, Item::ordering> _queue;

};



void ManualContextScheduler::set_time(Timestamp tp) {
    _scheduler->set_time(tp);
}


template<typename SchedulerType>
void ContextScheduler<SchedulerType>::operator ()(Timestamp tm, Function<void(Timestamp)> fn, const void *ident) {
    _scheduler->reschedule(tm, fn, ident);
};

ManualContextScheduler create_scheduler_manual() {
    return ManualContextScheduler(std::make_shared<ManualControlScheduler>());
}

template class ContextScheduler<ManualControlScheduler>;

template<typename Executor>
class RealTimeScheduler  { // @suppress("Miss copy constructor or assignment operator")
public:

    template<typename ... Args>
    RealTimeScheduler(Args && ... args)
        :_executor(std::forward<Args>(args)...) {}
    ~RealTimeScheduler() {
        stop();
    }

    void reschedule(const Timestamp &tm, Function<void(Timestamp)> &fn, const void *ident) {
        std::unique_lock lk(_mx);
        Timestamp toptime = _queue.empty()?Timestamp::max():_queue.front().tp;
        reschedule_queue(_queue, tm, fn, ident);
        if (tm < toptime) _cond.notify_all();
        if (!_thr.joinable()) {
            _thr = std::thread([this]{
                this_instance = this;
                worker();
            });
        }
    }

    void stop() {
        {
            std::lock_guard _(_mx);
            _stop = true;
        }
        _cond.notify_all();
        if (_thr.get_id() == std::this_thread::get_id()) {
            this_instance = nullptr;
            _thr.detach();
        } else {
            _thr.join();
        }
    }



protected:
    void worker() {
        std::unique_lock lk(_mx);
        while (!_stop) {
            if (_queue.empty()) {
                _cond.wait(lk);
            } else {
                _cond.wait_until(lk,  _queue.front().tp);
                Timestamp now = std::chrono::system_clock::now();
                while (!_queue.empty() && _queue.front().tp <= now && !_stop) {
                    auto fn = std::move(_queue.front().fn);
                    _queue.pop();
                    lk.unlock();
                    _executor(fn,now);
                    if (this_instance != this) return;
                    lk.lock();
                }
            }
        }
        this_instance = nullptr;
    }

    Executor _executor;
    std::thread _thr;
    std::mutex _mx;
    std::condition_variable _cond;
    bool _stop = false;
    PriorityQueue<Item, Item::ordering> _queue;

    static thread_local RealTimeScheduler *this_instance;
};

class SingleThreadExecutor {
public:
    void operator()(Function<void(Timestamp)> &fn, const Timestamp &tm) {
        try {
            fn(tm);
        } catch (std::exception &e) {
            std::cerr<< e.what() << std::endl;
        }
    }
};


class SingleThreadScheduler: public RealTimeScheduler<SingleThreadExecutor> {
public:
    using RealTimeScheduler<SingleThreadExecutor>::RealTimeScheduler;
};

SingleThreadContextScheduler create_scheduler() {
    return std::shared_ptr<SingleThreadScheduler>();
}

template<typename Executor>
thread_local RealTimeScheduler<Executor> *RealTimeScheduler<Executor>::this_instance = nullptr;

template class ContextScheduler<SingleThreadScheduler>;



}

