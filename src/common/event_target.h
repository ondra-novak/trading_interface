#pragma once
#include <memory>

#include "../trading_ifc/instrument.h"
#include "../trading_ifc/account.h"
#include "../trading_ifc/orderbook.h"
#include "../trading_ifc/tickdata.h"
#include "../trading_ifc/order.h"
#include "../trading_ifc/fill.h"
#include "../trading_ifc/function.h"
#include "basic_order.h"

namespace trading_api {



class IEventTarget;


using PEventTarget = std::shared_ptr<IEventTarget>;
using WPEventTarget = std::weak_ptr<IEventTarget>;

///Represents strategy (with context) from service provider side
/**
 * Each strategy is represented by this interface. When service provider detects
 * a market event, it calls a function on_event defined on this interface
 */
class IEventTarget {
public:

    virtual ~IEventTarget () {}
    ///called when update of an instrument is finished
    /**
     * @param i instrument updated
     * @note called under exchange's lock. It is expected, that event is put
     * into execution queue. Don't call exchange object directly from the event. Do
     * not perform blocking operations in this event
     */
    virtual void on_event(const Instrument &i) = 0;

    ///called when update on an account is finished
    /**
     * @param a account updated
     * @note called under exchange's lock. It is expected, that event is put
     * into execution queue. Don't call exchange object directly from the event. Do
     * not perform blocking operations in this event
     */
    virtual void on_event(const Account &a) = 0;

    ///called when subscription update
    /**
     * @param i instrument
     * @param subscription_type type of subscription
     *
     * @note actual market data are not part of event. When event is processed
     * the strategy must read last market data from the exchange object
     */
    virtual void on_event(const Instrument &i, SubscriptionType subscription_type) = 0;

    ///called when order state changed
    virtual void on_event(const Order &order,const Order::Report &report) = 0;

    ///called when fill is detected
    virtual void on_event(const Order &order, const Fill &fill) = 0;


};


}
