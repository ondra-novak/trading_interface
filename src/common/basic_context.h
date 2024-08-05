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



class BasicContext: public IContext,
                    public IEventTarget,
                    public IErrorHandler,
                    public IMQBroker::IListener{
public:

    using GlobalScheduler = std::function<void(Timestamp,std::function<void(Timestamp)>, const void *)>;

    BasicContext(std::unique_ptr<IStorage> storage,
            GlobalScheduler gscheduler,
            Log logger,
            MQBroker mq,
            std::string_view strategy_name)
        :_scheduler(std::move(gscheduler))
        ,_storage(std::move(storage))
        ,_logger(std::move(logger), "{}", strategy_name)
        ,_mq(mq)
    {
    }

    BasicContext(const BasicContext &) = delete;
    BasicContext &operator=(const BasicContext &) = delete;

    ~BasicContext();


    void init(std::unique_ptr<IStrategy> strategy,
            std::vector<Account> accounts,
            std::vector<Instrument> instruments,
            Config config);

    virtual void on_event(const Instrument &i, AsyncStatus st) override;
    virtual void on_event(const Account &a, AsyncStatus st) override;
    virtual void on_event(const Instrument &i, SubscriptionType subscription_type) override;
    virtual void on_event(const Order &order, const Order::Report &report) override;
    virtual void on_event(const Order &order, const Fill &fill) override;
    virtual void subscribe(SubscriptionType type, const Instrument &i)override;
    virtual Order replace(const Order &order, const Order::Setup &setup, bool amend) override;
    virtual Fills get_fills(std::size_t limit, std::string_view filter = {}) const override;
    virtual Fills get_fills(Timestamp tp, std::string_view filter = {}) const override;
    virtual Order place(const Instrument &instrument, const Account &account,  const Order::Setup &setup) override;
    virtual void cancel(const Order &order) override;
    virtual void set_timer(Timestamp at, TimerEventCB fnptr, TimerID id) override;
    virtual void unsubscribe(SubscriptionType type, const Instrument &i) override;
    virtual Timestamp get_event_time() const override;
    virtual Order bind_order(const Instrument &instrument, const Account &account) override;
    virtual void update_account(const Account &a, CompletionCB complete_ptr) override;
    virtual void allocate(const Account &a, double equity) override;
    virtual bool clear_timer(TimerID id) override;
    virtual void update_instrument(const Instrument &i, CompletionCB complete_ptr) override;
    virtual void unset_var(std::string_view var_name) override;
    virtual void set_var(std::string_view var_name, std::string_view value) override;
    virtual bool get_service(const std::type_info &tinfo, std::shared_ptr<void> &ptr) override;
    virtual Log get_logger() const override;
    virtual std::string get_var(std::string_view var_name) const override;
    virtual std::span<const Account> get_accounts() const override;
    virtual std::span<const Instrument> get_instruments() const override;
    virtual void enum_vars(std::string_view start, std::string_view end,
             Function<void(std::string_view,std::string_view)> &fn) const override;
    virtual void enum_vars(std::string_view prefix,
            Function<void(std::string_view,std::string_view)> &fn) const override;
    virtual const Config &get_config() const override;
    virtual void on_message(MQClient::Message message) override;
    virtual void mq_subscribe_channel(std::string_view channel) override;
    virtual void mq_unsubscribe_channel(std::string_view channel) override;
    virtual void mq_send_message(std::string_view channel, std::string_view msg) override;

    virtual void on_unhandled_exception()  override;
protected:

    GlobalScheduler _scheduler;
    std::unique_ptr<IStorage> _storage;
    std::unique_ptr<IStrategy> _strategy;
    Log _logger;
    MQBroker _mq;
    std::vector<Account> _accounts;
    std::vector<Instrument> _instruments;
    Config _config;
    Timestamp _event_time = Timestamp::min();
    Timestamp _scheduled_time = Timestamp::max();


    struct EvUpdateInstrument {
        BasicContext *me;
        Instrument i;
        AsyncStatus st;
        void operator()();
    };

    struct EvUpdateAccount {
        BasicContext *me;
        Account a;
        AsyncStatus st;
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

    struct EvException {
        std::exception_ptr eptr;
        void operator()();
    };

    struct EvMQ {
        BasicContext *me;
        MQBroker::Message msg;
        void operator()();
    };

    using QueueItem = std::variant<
            EvUpdateAccount,
            EvUpdateInstrument,
            EvOrderStatus,
            EvOrderFill,
            EvMarketData,
            EvException,
            EvMQ
            >;

    struct TimerItem {
        Timestamp tp;
        TimerID id;
        TimerEventCB r;
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

    void begin_transaction();
    void commit();
    void rollback();
    void notify_queue();

    void on_scheduler(Timestamp tp) noexcept;
    template<std::invocable<> Fn>
    void call_strategy(Fn &&strategy_fn);
};


}
