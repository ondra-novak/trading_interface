#pragma once

#include "event_target.h"

#include "../trading_ifc/strategy.h"
#include "context_scheduler.h"
#include "storage.h"

#include "basic_exchange.h"
#include <deque>
#include <mutex>
#include <map>
#include <set>

namespace trading_api {




/*scheduler object acts as invocable, which receives timestamp, function to call,
 * and pointer which serves as identification.
 *
 * If function is called, and there is already scheduled action, it
 * reschedules the action to new time point
 *
 */

template<typename Scheduler>
concept SchedulerType = (std::is_invocable_v<Scheduler, Timestamp, Function<void(Timestamp)>, const void *>);



class BasicContext: public IContext, public IEventTarget {
public:

    using GlobalScheduler = std::function<void(Timestamp,std::function<void(Timestamp)>, const void *)>;

    BasicContext(std::unique_ptr<IStorage> storage, GlobalScheduler gscheduler, Log logger, std::string_view strategy_name)
        :_scheduler(std::move(gscheduler))
        ,_storage(std::move(storage))
        ,_logger(std::move(logger), "{}", strategy_name){
    }

    BasicContext(const BasicContext &) = delete;
    BasicContext &operator=(const BasicContext &) = delete;

    ~BasicContext();


    void init(std::unique_ptr<IStrategy> strategy,
            std::vector<Account> accounts,
            std::vector<Instrument> instruments,
            Config config);

    virtual void on_event(const Instrument &i) override;
    virtual void on_event(const Account &a) override;
    virtual void on_event(const Instrument &i, SubscriptionType subscription_type) override;
    virtual void on_event(const Order &order, const Order::Report &report) override;
    virtual void on_event(const Order &order, const Fill &fill) override;
    virtual void subscribe(SubscriptionType type, const Instrument &i)override;
    virtual Order replace(const Order &order, const Order::Setup &setup, bool amend) override;
    virtual Fills get_fills(std::size_t limit, std::string_view filter = {}) const override;
    virtual Fills get_fills(Timestamp tp, std::string_view filter = {}) const override;
    virtual Order place(const Instrument &instrument, const Account &account,  const Order::Setup &setup) override;
    virtual void cancel(const Order &order) override;
    virtual void set_timer(Timestamp at, CompletionCB fnptr, TimerID id) override;
    virtual void unsubscribe(SubscriptionType type, const Instrument &i) override;
    virtual Timestamp get_event_time() const override;
    virtual Order bind_order(const Instrument &instrument, const Account &account) override;
    virtual void update_account(const Account &a, CompletionCB complete_ptr) override;
    virtual void allocate(const Account &a, double equity) override;
    virtual bool clear_timer(TimerID id) override;
    virtual void update_instrument(const Instrument &i, CompletionCB complete_ptr) override;
    virtual void unset_var(std::string_view var_name) override;
    virtual void set_var(std::string_view var_name, std::string_view value) override;
    virtual Log get_logger() const override;
    virtual std::string get_var(std::string_view var_name) const override;
    virtual std::span<const Account> get_accounts() const override;
    virtual std::span<const Instrument> get_instruments() const override;
    virtual void enum_vars(std::string_view start, std::string_view end,
             Function<void(std::string_view,std::string_view)> &fn) const override;
    virtual void enum_vars(std::string_view prefix,
            Function<void(std::string_view,std::string_view)> &fn) const override;
    virtual const Config &get_config() const override;

protected:

    GlobalScheduler _scheduler;
    std::unique_ptr<IStorage> _storage;
    std::unique_ptr<IStrategy> _strategy;
    Log _logger;
    std::vector<Account> _accounts;
    std::vector<Instrument> _instruments;
    Config _config;
    Timestamp _event_time = Timestamp::min();
    Timestamp _scheduled_time = Timestamp::max();


    struct EvUpdateInstrument {
        BasicContext *me;
        Instrument i;
        void operator()();
    };

    struct EvUpdateAccount {
        BasicContext *me;
        Account a;
        void operator()();
    };

    struct EvMarketData {
        BasicContext *me;
        Instrument i;
        bool ticker = false;
        bool orderbook = false;
        void operator()();
    };

    struct EvOrderStatus {
        BasicContext *me;
        Order order;
        Order::Report report;
        void operator()();
    };

    struct EvOrderFill {
        BasicContext *me;
        Order order;
        Fill fill;
        void operator()();
    };


    using QueueItem = std::variant<
            EvUpdateAccount,
            EvUpdateInstrument,
            EvOrderStatus,
            EvOrderFill,
            EvMarketData>;

    struct TimerItem {
        Timestamp tp;
        TimerID id;
        Function<void()> r;
        struct ordering {
            bool operator()(const TimerItem &a, const TimerItem &b) const {
                return a.tp > b.tp;
            }
        };
    };

    struct Batches {
        std::vector<Order> _batch_place;
        std::vector<Order> _batch_cancel;
    };

    std::mutex _queue_mx;
    std::deque<QueueItem> _queue;
    PriorityQueue<TimerItem, typename TimerItem::ordering> _timed_queue;

    std::multimap<Account, CompletionCB> _cb_update_account;
    std::multimap<Instrument, CompletionCB> _cb_update_instrument;
    std::map<Exchange, Batches> _exchanges;

    void flush_batches();
    void notify_queue();

    void on_scheduler(Timestamp tp) noexcept {
        _event_time = tp;
        _storage->begin_transaction();
        try {
            std::unique_lock lk(_queue_mx);

            while (!_queue.empty()) {
                lk.unlock();
                std::visit([&](auto &item) {item();},_queue.front());
                lk.lock();
                _queue.pop_front();
            }
            while (!_timed_queue.empty() && _timed_queue.front().tp <= tp) {
                    Function<void()> fn (std::move(_timed_queue.front().r));
                    _timed_queue.pop();
                    lk.unlock();
                    fn();
                    lk.lock();
            }
            if (!_timed_queue.empty()) {
                _scheduled_time = _timed_queue.front().tp;
                _scheduler(_scheduled_time, [this](auto tp){on_scheduler(tp);}, this);
            } else {
                _scheduled_time = Timestamp::max();
            }
        } catch (...) {
            try {
                _strategy->on_unhandled_exception();
            } catch (...) {
                _storage->rollback();
                _logger.fatal("Unhandled exception in strategy{}", std::current_exception());
            }
        }
        flush_batches();
        _storage->commit();

    }

};


}
