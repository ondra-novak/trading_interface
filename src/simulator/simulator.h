#pragma once

#include "../common/exchange.h"

#include "../common/basic_order.h"
#include <map>
#include <set>

namespace trading_api {


class SimulatorBackend {
public:

    class SimOrder: public BasicOrder {
    public:
        using BasicOrder::BasicOrder;
        virtual SerializedOrder to_binary() const override {return {};}
        PCBasicOrder replace;
        bool amend;
        std::string id = gen_id();
        virtual std::string get_id() const override;
        static std::string gen_id();

    };

    //frontend
    void disconnect(IEventTarget *target);
    void update_orders(IEventTarget *target, std::span<SerializedOrder> orders);
    void subscribe(IEventTarget *target, SubscriptionType sbstype, const Instrument &instrument);
    void unsubscribe(IEventTarget *target, SubscriptionType sbstype, const Instrument &instrument);
    Order place(IEventTarget *target, const Instrument &instrument, const Order::Setup &order_setup);
    Order replace(IEventTarget *target, const Order &order, const Order::Setup &order_setup, bool amend);
    void cancel(const Order &order);
    void update_ticker(IEventTarget *target, const Instrument &instrument);
    void update_account(IEventTarget *target, const Account &account);
    void update_instrument(IEventTarget *target, const Instrument &instrument);
    void allocate_equity(IEventTarget *target, const Account &account, double equity);
    PBasicOrder create_order(const Instrument &instrument, const Order::Setup &setup);
    PBasicOrder create_order_replace(const PCBasicOrder &replace, const Order::Setup &setup, bool amend);
    void batch_place(IEventTarget *target, std::span<PBasicOrder> orders);
    void batch_cancel(std::span<PCBasicOrder> orders);

    //backend

protected:
    std::mutex _mx;

    struct PendingOrder {
        PBasicOrder _order;
        IEventTarget *_target;
        double _filled = 0;
        double _last_fill_price = 0;
        double _trailing_price = 0;

    };

    struct InstrumentState {
        std::vector<PendingOrder> _active_orders;
        std::optional<Ticker> _last_ticker;
        OrderBook _orderbook;
        std::vector<IEventTarget *> _subscriptions_ticker;
        std::vector<IEventTarget *> _subscriptions_orderbook;

        void disconnect(IEventTarget *target);
    };

    std::unordered_map<Instrument, InstrumentState, Instrument::Hasher> _instruments;

    std::shared_ptr<SimOrder> create_sim_order(const Instrument &i, const Order::Setup &stp);

    struct MarketExecState {
        Order::State final_state;
        Order::Reason reject_reason = Order::Reason::no_reason;
        double total_filled = 0;
        double last_fill_price = 0;
        Side side = Side::undefined;
    };

    MarketExecState try_market_order(const PBasicOrder &ord, double already_filled);
    MarketExecState try_market_order_spec(const Instrument &instrument, const IOrder::Undefined &, double already_filled);
    MarketExecState try_market_order_spec(const Instrument &instrument, const IOrder::Transfer &, double already_filled);
    MarketExecState try_market_order_spec(const Instrument &instrument, const Order::Stop &, double already_filled);
    MarketExecState try_market_order_spec(const Instrument &instrument, const Order::TrailingStop &, double already_filled);
    MarketExecState try_market_order_spec(const Instrument &instrument, const Order::ClosePosition &order, double already_filled);
    MarketExecState try_market_order_spec(const Instrument &instrument, const Order::Market &order, double already_filled);
    MarketExecState try_market_order_spec(const Instrument &instrument, const Order::ImmediateOrCancel &order, double already_filled);
    MarketExecState try_market_order_spec(const Instrument &instrument, const Order::LimitPostOnly &order, double already_filled);
    MarketExecState try_market_order_spec(const Instrument &instrument, const Order::Limit &order, double already_filled);

    std::pair<double, double> ticker_info(const Instrument &instrument, Side side);
    void send_fill(const PBasicOrder &o, Side side,  double price, double size);
};


class Simulator {
public:

    Simulator() = default;
    Simulator(std::shared_ptr<SimulatorBackend> ptr):_ptr(ptr) {}
    void disconnect(IEventTarget *target) {
        _ptr->disconnect(target);
    }
    void update_orders(IEventTarget *target, std::span<SerializedOrder> orders) {
        _ptr->update_orders(target, orders);
    }
    void subscribe(IEventTarget *target, SubscriptionType sbstype, const Instrument &instrument) {
        _ptr->subscribe(target, sbstype, instrument);
    }
    void unsubscribe(IEventTarget *target, SubscriptionType sbstype, const Instrument &instrument) {
        _ptr->unsubscribe(target, sbstype, instrument);
    }
    void update_ticker(IEventTarget *target, const Instrument &instrument) {
        _ptr->update_ticker(target, instrument);
    }
    void update_account(IEventTarget *target, const Account &account) {
        _ptr->update_account(target, account);
    }
    void update_instrument(IEventTarget *target, const Instrument &instrument) {
        _ptr->update_instrument(target, instrument);
    }
    void allocate_equity(IEventTarget *target, const Account &account, double equity) {
        _ptr->allocate_equity(target, account, equity);
    }
    PBasicOrder create_order(const Instrument &instrument, const Order::Setup &setup) {
        return _ptr->create_order(instrument, setup);
    }
    PBasicOrder create_order_replace(const PCBasicOrder &replace, const Order::Setup &setup, bool amend) {
        return _ptr->create_order_replace(replace, setup, amend);
    }
    void batch_place(IEventTarget *target, std::span<PBasicOrder> orders) {
        _ptr->batch_place(target, orders);
    }
    void batch_cancel(std::span<PCBasicOrder> orders) {
        _ptr->batch_cancel(orders);
    }


protected:
    std::shared_ptr<SimulatorBackend> _ptr;
};

static_assert(ExchangeType<Simulator>);

}
