#include <trading_api/exchange.h>
#include <trading_api/module.h>


using namespace trading_api;

class ExampleExchange: public IExchangeService {
public:
    virtual void create_accounts(std::vector<std::string> account_idents, Function<void(std::vector<Account>)> cb) override {}
    virtual StrategyConfigSchema get_config_schema() const
            override {return {};}
    virtual Order create_order(
            const Instrument &instrument,
            const Account &account,
            const Order::Setup &setup) override {}
    virtual void unsubscribe(SubscriptionType type,
            const Instrument &i) override {}
    virtual void init(ExchangeContext context,
            const StrategyConfig &config) override {}
    virtual void order_apply_fill(const Order &order,
            const Fill &fill) override {}
    virtual void update_account(const Account &a) override {}
    virtual Order create_order_replace(
            const Order &replace,
            const Order::Setup &setup, bool amend) override {return {};}
    virtual std::string get_id() const override {return {};}
    virtual std::optional<IExchange::Icon> get_icon() const override {return {};}
    virtual void update_instrument(const Instrument &i) override {}
    virtual void create_instruments(std::vector<std::string> instruments, Account accunt, Function<void(std::vector<Instrument>)> cb) override {}
    virtual void order_apply_report(const Order &order, const Order::Report &report) override {}
    virtual std::string get_name() const override {return {};}
    virtual void subscribe(SubscriptionType type,const Instrument &i) override {};
    virtual void batch_place(std::span<Order> orders) override {};
    virtual void batch_cancel(std::span<Order> orders) override {};
    virtual void restore_orders(void *context, std::span<SerializedOrder> orders) {}

};


EXPORT_EXCHANGE(ExampleExchange);

