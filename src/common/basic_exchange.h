#pragma once

#include "../trading_ifc/strategy_context.h".h"

#include "event_target.h"
#include <map>
#include <set>

namespace trading_api {

class AbstractExchange {
public:



    virtual void list_instruments(Function<void(const Instrument&)> ) const {}
    virtual std::string get_name() const  {return {};}
    virtual std::string get_id() const {return {};}

    ///Disconnect given event target
    /**
     * Causes that any objects associated with this event target are disposed.
     * This includes open orders, subscriptions, pending updates, etc
     * @param target target to disconnect
     */
    virtual void disconnect(const IEventTarget *target);
    ///Request to update orders from exchange
    /**
     * Function is used to update state of orders stored in database
     *
     * @param target target which will consume update events
     * @param orders list of orders to update. The orders are passed in binary
     * serialized form (because they are probably read from the database). The
     * exchange instance must use binary form to restore instances of Order class.
     * Then all these orders are passed as on_order event. If there are unprocessed
     * fills, the exachange must also generate on_fill()
     *
     * @note it is not necessery to track processed and unprocessed fills. The
     * function can send all fills to the target. The target is responsible to
     * discard any duplicated fills. This requires that generated fills have
     * same unique identifier.
     *
     */
    virtual void restore_orders(IEventTarget *target, std::span<SerializedOrder> orders) = 0;
    ///Request to subscribe market data (stream)
    /**
     * @param target object which consumes updates
     * @param sbstype type of subscription
     * @param instrument instrument which is subscribed
     */
    virtual void subscribe(IEventTarget *target, SubscriptionType sbstype, const Instrument &instrument);
    ///Unsubscribe stream
    /**
     * @param target object which consumes updates
     * @param sbstype type of subscription
     * @param instrument instrument
     */
    virtual void unsubscribe(IEventTarget *target, SubscriptionType sbstype, const Instrument &instrument);

    ///Create order instance - don't place order yet
    /**
     * @param instrument associated instrument
     * @param setup order setup
     * @return pointer to newly created order. In case of error, discarded order must be created
     */
    virtual PBasicOrder create_order(const Instrument &instrument, const Order::Setup &setup) = 0;

    ///Create order instance which replaces other order - don't place order yet
    /**
     * @param replace order to replace
     * @param setup order setup
     * @param amend try to amend order
     * @return pointer to newly created order. In case of error, discarded order must be created
     */
    virtual PBasicOrder create_order_replace(const PCBasicOrder &replace, const Order::Setup &setup, bool amend) = 0;

    ///Place orders in batch
    /**
     * @param target event target where these orders belongs
     * @param orders orders - must be created by create_order
     */
    virtual void batch_place(IEventTarget *target, std::span<PBasicOrder> orders);

    ///Cancel orders in batch
    /**
     * @param orders list of orders to cancel
     */
    virtual void batch_cancel(std::span<PCBasicOrder> orders) = 0;
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
    virtual void update_ticker(IEventTarget *target, const Instrument &instrument);
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
    virtual void update_account(IEventTarget *target, const Account &account) = 0;
    ///Request to update instrument
    /**
     * Updates instrument information.
     *
     * @param target
     * @param instrument
     */
    virtual void update_instrument(IEventTarget *target, const Instrument &instrument) = 0;

    bool get_last_ticker(const Instrument &instrument, Ticker &tk);

    bool get_last_orderbook(const Instrument &instrument, OrderBook &ordb);

    virtual std::optional<IExchange::Icon> get_icon() const {return {};}

    static AbstractExchange *from_exchange(Exchange ex);

protected:

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
    std::map<PBasicOrder, IEventTarget *, std::less<> > _orders;


    virtual void do_subscribe(SubscriptionType type, const Instrument &i) = 0;
    virtual void do_unsubscribe(SubscriptionType type, const Instrument &i) = 0;
    virtual void do_update_account(const Account &a) = 0;
    virtual void do_update_instrument(const Instrument &i) = 0;
    virtual void do_batch_place(std::span<PBasicOrder> orders) = 0;


    void income_data(const Instrument &i, const Ticker &t);
    void income_data(const Instrument &i, const OrderBook &t);
    void object_updated(const Account &i);
    void object_updated(const Instrument &i);
    void order_state_changed(const PCBasicOrder &order, Order::State state, Order::Reason reason = Order::Reason::no_reason, std::string message = {});
    void order_fill(const PCBasicOrder &order, const Fill &fill);
    void order_restore(IEventTarget *target, const PBasicOrder &order);

    void send_subscription_notify(const Instrument &i, SubscriptionType type);



};


class BasicExchange: public IExchange {
public:

    BasicExchange(std::unique_ptr<AbstractExchange> impl, std::string label)
        :_impl(std::move(impl)),_label(label) {}

    virtual std::string get_label() const override {return _label;};
    virtual void list_instruments(Function<void(const Instrument&)> fn) const override {
        _impl->list_instruments(std::move(fn));
    }
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
    virtual AbstractExchange *get_instance() const {return _impl.get();}



protected:
    std::unique_ptr<AbstractExchange> _impl;
    std::string _label;

};




}
