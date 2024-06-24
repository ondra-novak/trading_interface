#pragma once

#include "event_target.h"
#include "../trading_ifc/strategy_context.h"

#include <span>

namespace trading_api {

template<typename T>
concept ExchangeType = requires(T exch, IEventTarget *target,
        std::span<SerializedOrder> orders,
        SubscriptionType sbstype,
        const Instrument &i,
        const Account &a,
        const Order::Setup &ostp,
        const Order &ord,
        double equity,
        bool b) {
    {exch.disconnect(target)};
    {exch.update_orders(target, orders)};
    {exch.subscribe(target, sbstype, i)};
    {exch.place(target, i, ostp)}->std::same_as<Order>;
    {exch.replace(target, ord, ostp, b)}->std::same_as<Order>;
    {exch.unsubscribe(target, sbstype, i)};
    {exch.cancel(ord)};
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

    ///Place new order
    /**
     * @param target target object which consumes updates of placed order
     * @param instrument instrument
     * @param order_setup parameters of the order
     * @return newly created order
     */
    Order place(IEventTarget *target, const Instrument &instrument, const Order::Setup &order_setup);
    ///Replace existing order
    /**
     * Perform cancel+place (atomically if possible) or amend. You can replace
     * any order in any state. However if amend is requested only
     * compatible order can be replaced (same side, type, etc). The amend
     * operation is also possible on already finished order (which generates also
     * finished order with same finish reason)
     *
     * @param target target object which consumes updates of placed order
     * @param order order to replace. This order receives cancel state (if not finished yet)
     * @param order_setup new order setup
     * @param amend specify true, if you need amend operation. This can speed
     * up place operation, prevent loosing position in orderbook. Amend operation
     * limits which orders can be amended. If the amend is not possible, the
     * order is discarded. If amend is not supported by exchange, it is executed
     * as cancel+place.
     * @return new order
     *
     * @note if the order is amended (which means, that no new order was created),
     * the protocol requies, that old order is reported as canceled. If there is
     * fill on the way, it can happen, that fill will be associated with new
     * order. It is also possible, that old order and new order shares same order_id
     * in fill report
     */
    Order replace(IEventTarget *target, const Order &order, const Order::Setup &order_setup, bool amend);
    ///Cancel existing order
    /**
     * You can cancel only pending order. If order is done, no operation is perfomed
     * @param order order to cancel
     */
    void cancel(const Order &order);
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
