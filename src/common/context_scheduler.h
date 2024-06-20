#pragma once

#include "../trading_ifc/function.h"
#include "../trading_ifc/timer.h"
#include "priority_queue.h"

#include <mutex>
#include <condition_variable>

namespace trading_api {


class ContextScheduler {
public:



protected:
    struct Item;
public:

    struct Registration {
        Function<void(Timestamp)> wakeup_fn;
        const Item *back_link;

        Registration(Function<void(Timestamp)> wakeup_fn):wakeup_fn(std::move(wakeup_fn)), back_link(nullptr) {}
        Registration(const Registration &) = delete;
        Registration &operator=(const Registration &) = delete;
    };

    void set(Registration *reg, Timestamp tp);
    void unset(Registration *what);
    void run(std::stop_token stop);

protected:

    struct Item {
        Timestamp tp;
        std::size_t local_priority;
        Registration *reg;

        struct ordering {
            bool operator()(const Item &a, const Item &b) const;
        };
        Item(Registration *reg, Timestamp tp);
        Item(Item &&other);
        Item &operator=(Item &&other);
        ~Item();
        static std::size_t loc_priority_counter;
    };

    PriorityQueue<Item> _queue;
    std::mutex _mx;
    std::condition_variable _cond;




};


}
