#include <trading_api/exchange.h>
#include <trading_api/module.h>


using namespace trading_api;

class ExampleExchange: public IExchangeService {
public:
    virtual StrategyConfigSchema get_config_schema() const
            override {return {};}
    virtual Order create_order(
            const Instrument &,
            const Account &,
            const Order::Setup &) override {return {};}
    virtual void unsubscribe(SubscriptionType ,
            const Instrument &) override {}
    virtual void init(ExchangeContext ,
            const StrategyConfig &) override {}
    virtual void order_apply_fill(const Order &,
            const Fill &) override {}
    virtual void update_account(const Account &) override {}
    virtual Order create_order_replace(
            const Order &,
            const Order::Setup &, bool ) override {return {};}
    virtual std::string get_id() const override {return {};}
    virtual std::optional<IExchange::Icon> get_icon() const override {return {};}
    virtual void update_instrument(const Instrument &) override {}
    virtual void order_apply_report(const Order &, const Order::Report &) override {}
    virtual std::string get_name() const override {return {};}
    virtual void subscribe(SubscriptionType ,const Instrument &) override {};
    virtual void batch_place(std::span<Order> ) override {};
    virtual void batch_cancel(std::span<Order> ) override {};
    virtual void restore_orders(void *, std::span<SerializedOrder> ) override {}

    virtual void query_accounts(std::string_view , std::string_view ,
            Function<void(Account)> ) override {}
    virtual void query_instruments(std::string_view , std::string_view ,
            Function<void(Instrument)> ) override {}
};


EXPORT_EXCHANGE(ExampleExchange);

