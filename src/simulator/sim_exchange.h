#pragma once

#include "../trading_ifc/exchange_service.h"
#include "../trading_ifc/ticker.h"
#include "../trading_ifc/orderbook.h"
#include "../common/priority_queue.h"

#include <vector>
namespace trading_api {

class SimExchange: public IExchangeService {
public:

    struct Config {
        std::vector<Account> accounts;
        std::vector<Instrument> instruments;
    };

    using GlobalScheduler = std::function<void(Timestamp,std::function<void(Timestamp)>, const void *)>;

    SimExchange(GlobalScheduler scheduler, Config config);

    virtual void init(ExchangeContext context, const StrategyConfig &config) override;
    virtual StrategyConfigSchema get_config_schema() const override;
    virtual Order create_order(const Instrument &instrument,
            const Account &account, const Order::Setup &setup) override;
    virtual void batch_cancel(std::span<Order> orders) override;
    virtual void order_apply_fill(const Order &order, const Fill &fill)
            override;
    virtual void update_account(const Account &a) override;
    virtual Order create_order_replace(const Order &replace,
            const Order::Setup &setup, bool amend) override;
    virtual std::string get_id() const override;
    virtual std::optional<IExchange::Icon> get_icon() const override;
    virtual void restore_orders(void *context,
            std::span<SerializedOrder> orders) override;
    virtual void batch_place(std::span<Order> orders) override;
    virtual void subscribe(SubscriptionType type, const Instrument &i) override;
    virtual void update_instrument(const Instrument &i) override;
    virtual void unsubscribe(SubscriptionType type, const Instrument &i)
            override;
    virtual void order_apply_report(const Order &order,
            const Order::Report &report) override;
    virtual std::string get_name() const override;


    void add_record(const Timestamp &tp, const Instrument &i, const Ticker &tk);
    void add_record(const Timestamp &tp, const Instrument &i, const OrderBook::Update &ordb);

protected:

    struct Record {
        Timestamp tp;
        Instrument i;
        std::variant<Ticker, OrderBook::Update> data;
        struct ordering {
            bool operator()(const Record &a, const Record &b) const {
                return a.tp > b.tp;
            }
        };
    };


    std::mutex _mx;
    ExchangeContext ctx;
    GlobalScheduler _scheduler;
    std::unordered_map<std::string, Account> _accounts;
    std::unordered_map<std::string, Instrument> _instruments;
    std::map<Instrument, OrderBook> _orderbooks;
    PriorityQueue<Record, Record::ordering> _price_data;

    void on_timer(Timestamp tp);
    void reschedule();

};

}
