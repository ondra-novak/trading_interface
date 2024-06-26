#pragma once

#include "event_target.h"
#include "../trading_ifc/strategy_context.h"
#include "basic_order.h"

#include <span>

namespace trading_api {

template<typename T>
concept ExchangeType = requires(T exch, IEventTarget *target,
        std::span<SerializedOrder> orders,
        SubscriptionType sbstype,
        const Instrument &i,
        const Account &a,
        const Order::Setup &ostp,
        const PCBasicOrder &ord,
        std::span<PBasicOrder> order_batch,
        std::span<PCBasicOrder> cancel_order_batch,
        double equity,
        bool b) {
    {exch.disconnect(target)};
    {exch.update_orders(target, orders)};
    {exch.subscribe(target, sbstype, i)};
    {exch.create_order(i, ostp)}->std::same_as<PBasicOrder>;
    {exch.create_order_replace(ord, ostp, b)}->std::same_as<PBasicOrder>;
    {exch.batch_place(target, order_batch)};
    {exch.batch_cancel(cancel_order_batch)};
    {exch.unsubscribe(target, sbstype, i)};
    {exch.update_account(target, a)};
    {exch.update_ticker(target, i)};
    {exch.update_instrument(target, i)};
    {exch.allocate_equity(target, a, equity)};
};


class ExchangePrototype {
public:

    ///Disconnect given event target
    /**
     * Causes that any objects associated with this event target are disposed.
     * This includes open orders, subscriptions, pending updates, etc
     * @param target target to disconnect
     */
    void disconnect(IEventTarget *target);
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
    void update_orders(IEventTarget *target, std::span<SerializedOrder> orders);
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
     */
    PBasicOrder create_order(const Instrument &instrument, const Order::Setup &setup);

    ///Create order instance which replaces other order - don't place order yet
    /**
     * @param replace order to replace
     * @param setup order setup
     * @param amend try to amend order
     * @return pointer to newly created order. In case of error, discarded order must be created
     */
    PBasicOrder create_order_replace(const PCBasicOrder &replace, const Order::Setup &setup, bool amend);

    ///Place orders in batch
    /**
     * @param target event target where these orders belongs
     * @param orders orders - must be created by create_order
     */
    void batch_place(IEventTarget *target, std::span<PBasicOrder> orders);

    ///Cancel orders in batch
    /**
     * @param orders list of orders to cancel
     */
    void batch_cancel(std::span<PCBasicOrder> orders);
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

    ///Strategy reports allocation of the equity
    void allocate_equity(IEventTarget *target, const Account &account, double equity);
};

static_assert(ExchangeType<ExchangePrototype>);

}
