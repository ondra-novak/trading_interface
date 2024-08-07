#pragma once

#include "../trading_ifc/strategy_context.h"
#include "../trading_ifc/exchange_service.h"

#include "event_target.h"
#include <map>
#include <set>

namespace trading_api {

class BasicExchangeContext: public IExchangeContext {
public:


    void init(std::unique_ptr<IExchangeService> svc, StrategyConfig configuration);


    ///Disconnect given event target
    /**
     * Causes that any objects associated with this event target are disposed.
     * This includes open orders, subscriptions, pending updates, etc
     * @param target target to disconnect
     *
     * @b implementation: target must be removed from all subscriptions and
     * asynchronous operations
     */
    void disconnect(const IEventTarget *target);

    ///Request to update orders from exchange
    /**
     * Function is used to update state of orders stored in database
     *
     * @param target target which will consume update events. Impementation
     * must pass this pointer to order_restore()
     * @param orders list of orders to update. The orders are passed in binary
     * serialized form (because they are read from the database). The
     * exchange instance must use binary form to restore instances of Order class.
     * Then all these orders are passed as on_order event. If there are unprocessed
     * fills, the exachange must also generate on_fill()
     *
     * @b implementation: the service provider must know, which order can restore.
     * The binary representation must contain identification of the exchange. So
     * the service provider can restore orders which are known for the exchange.
     * other orders are order_restore(), then order_fill for every fill on the order,
     * and finally order_state_change with final order state.
     */
    void restore_orders(IEventTarget *target, std::span<SerializedOrder> orders);
    ///Request to subscribe market data (stream)
    /**
     * @param target object which consumes updates
     * @param sbstype type of subscription
     * @param instrument instrument which is subscribed
     */
    void subscribe(IEventTarget *target, SubscriptionType sbstype, const Instrument &instrument);
    ///Unsubscribe stream
    /**
     * @param target object which consumes updates
     * @param sbstype type of subscription
     * @param instrument instrument
     */
    void unsubscribe(IEventTarget *target, SubscriptionType sbstype, const Instrument &instrument);

    ///Create order instance - don't place order yet
    /**
     * @param instrument associated instrument
     * @param setup order setup
     * @return pointer to newly created order. In case of error, discarded order must be created
     *
     * @b implementation: The function must create own instance of the order. Default implementation
     * creates BasicOrder instance. It must not place the order
     */
    Order create_order(const Instrument &instrument, const Account &account, const Order::Setup &setup);

    ///Create order instance which replaces other order - don't place order yet
    /**
     * @param replace order to replace
     * @param setup order setup
     * @param amend try to amend order
     * @return pointer to newly created order. In case of error, discarded order must be created
     *
     * @b implementation: The function must create own instance of the order. Default implementation
     * creates BasicOrder instance. It must not place the order
     */
    Order create_order_replace(const Order &replace, const Order::Setup &setup, bool amend);

    ///Place orders in batch
    /**
     * @param target event target where these orders belongs
     * @param orders orders - must be created by create_order
     */
    void batch_place(IEventTarget *target, std::span<Order> orders);

    ///Cancel orders in batch
    /**
     * @param orders list of orders to cancel
     */
    void batch_cancel(std::span<Order> orders);
    ///Request update of ticker
    /**
     * You can request update of a current ticker in case that your strategy doesn't
     * subscribe on stream, but needs time to time check price of the instrument.
     * There can be limit how often the strategy can request to update ticker.
     * If you call this function too often, extra requests are dropped (there can
     * be rate limit defined by exchange) However the service provider still
     * must report completion of the operation (it can send the last known ticker data
     * in this case)
     *
     * Purpose of this function is to ask ticker in minute interval or more
     *
     * @param target
     * @param instrument
     */
    void update_ticker(IEventTarget *target, const Instrument &instrument);
    ///Request to update account
    /**
     * The account don't need to be tracked realtime. The strategy needs to
     * request update account state manually. There can be also limit
     * how often the update can be called. If the update is called too often
     * the request can be dropped (however the service provider must report
     * completion of the request)
     *
     * @note The service provider should update account before fill is reported.
     * However it don't need to update all informations, just informations related
     * to fill. It must for example update the position on the account.
     *
     * @param target
     * @param account
     */
    void update_account(IEventTarget *target, const Account &account);
    ///Request to update instrument
    /**
     * Updates instrument information.
     *
     * @param target
     * @param instrument
     */
    void update_instrument(IEventTarget *target, const Instrument &instrument);

    ///Retrieve last ticker synchronously
    /**
     * @param instrument instrument object
     * @param tk variable which receives ticker
     * @retval true received
     * @retval false there is no last ticker stored
     */

    bool get_last_ticker(const Instrument &instrument, Ticker &tk);

    ///Retrieve last orderbook state synchronously
    /**
     * @param instrument instrument object
     * @param ordb variable which receives orderbook
     * @retval true received
     * @retval false cannot be retrieved synchronously
     */
    bool get_last_orderbook(const Instrument &instrument, OrderBook &ordb);

    ///Retrieve exchange icon
    std::optional<IExchange::Icon> get_icon() const;

    ///Get pointer to AbstractExchange from Exchange object
    static BasicExchangeContext &from_exchange(Exchange ex);

    std::string get_name() const;
    std::string get_id() const;

    ///Applies report to order object
    /**
     * Order object is owned by strategy and can be accessed anytime during processing,
     * because there is no locking scheme. This means, that new order state must
     * be applied synchronously with the strategy. This function is called in
     * strategy thread before the order is passed to the strategy and allows to
     * apply report to the order's internal state.
     *
     * @param order subject
     * @param report report to apply
     *
     * @note the order must be created by this exchange, otherwise function can throw exception
     */
    virtual void order_apply_report(const Order &order, const Order::Report &report);


    ///Applies fill to order object
    /**
     * Because order object can be modified only synchronously with the strategy,
     * this function must be called in strategy thread.
     *
     * @param order order
     * @param fill fill
     *
     * @note the order must be created by this exchange, otherwise function can throw exception
     *
     */
    virtual void order_apply_fill(const Order &order, const Fill &fill);






protected:

    ///Object's lock, derived class must use this lock to lock internals
    std::recursive_mutex _mx;

    struct Subscription {
        SubscriptionType type;
        Instrument i;
        IEventTarget *target;

        bool operator==(const Subscription &) const = default;
        std::strong_ordering operator<=>(const Subscription &) const = default;
    };

    enum class SubscriptionLimit {
        onceshot,
        unlimited
    };

    std::map<Instrument, Ticker> _tickers;
    std::map<Instrument, OrderBook> _orderbooks;
    std::map<Subscription, SubscriptionLimit> _subscriptions;
    std::map<Instrument, std::vector<IEventTarget *> > _instrument_update_waiting;
    std::map<Account, std::vector<IEventTarget *> > _account_update_waiting;
    std::map<Order, IEventTarget *, std::less<> > _orders;


    ///call this function when ticker for given instrument arrived
    /**
     * @param i instrument
     * @param t ticker
     */
    virtual void income_data(const Instrument &i, const Ticker &t) override;
    ///call this function when orderbook for given instrument arrived
    /**
     * @param i instrument
     * @param o orderbook
     */
    virtual void income_data(const Instrument &i, const OrderBook &o) override;
    ///call this function when account is updated
    virtual void object_updated(const Account &i) override;
    ///call this function when instrument is updated
    virtual void object_updated(const Instrument &i) override;
    ///call this function when order's state changed
    /**
     * As the orders are const, you cannot change state of the order directly. The
     * state is changed during processing the event because orders are in possesion
     * of the strategy
     *
     * @param order order instance
     * @param state new state
     * @param reason optional reason (for the state)
     * @param message optional string message if error
     */
    virtual void order_state_changed(const Order &order, const Order::Report &report) override;
    ///call this function for every fill on the order
    /**
     * @param order order instance
     * @param fill fill information
     */
    virtual void order_fill(const Order &order, const Fill &fill) override;
    ///call this function for every restored order from set passed to restore_orders()
    /**
     * @param target target argument passed to function restored_orders
     * @param order restored order instance
     */
    virtual void order_restore(void *target, const Order &order) override;

private:
    void send_subscription_notify(const Instrument &i, SubscriptionType type);

    std::unique_ptr<IExchangeService> _ptr;

};


class BasicExchange: public IExchange {
public:

    BasicExchange(std::unique_ptr<BasicExchangeContext> impl, std::string label)
        :_impl(std::move(impl)),_label(label) {}

    virtual std::string get_label() const override {return _label;};
    virtual std::string get_name() const override {
        return _impl->get_name();
    }
    virtual std::string get_id() const override {
        return _impl->get_id();
    }
    virtual std::optional<Icon> get_icon() const override {
        return _impl->get_icon();
    }
    virtual void disconnect(void *t) const {
        _impl->disconnect(reinterpret_cast<const IEventTarget *>(t));
    }
    virtual BasicExchangeContext *get_instance() const {return _impl.get();}

    virtual bool get_last_orderbook(const Instrument &instrument,
            OrderBook &ordb) const override {
        return _impl->get_last_orderbook(instrument, ordb);
    }
    virtual bool get_last_ticker(const Instrument &instrument,
            Ticker &tk) const override {
        return _impl->get_last_ticker(instrument, tk);
    }


protected:
    std::unique_ptr<BasicExchangeContext> _impl;
    std::string _label;

};




}
